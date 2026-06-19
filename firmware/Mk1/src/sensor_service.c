#include "sensor_service.h"
#include "board_nucleo_f401re.h"
#include "fdir.h"
#include "lcd1602.h"
#include "mpu6050.h"
#include "telemetry.h"

#include <stdio.h>

const char *sensor_status_to_string(SensorStatus status)
{
    switch (status)
    {
        case SENSOR_OK:
            return "OK";

        case SENSOR_MISSING:
            return "MISSING";

        case SENSOR_BAD_ID:
            return "BAD_ID";

        default:
            return "UNKNOWN";
    }
}

void sensor_init_and_report(AppState *app, uint32_t cmd_seq)
{
    SensorStatus status = mpu6050_init_sensor();

    if (status == SENSOR_OK)
    {
        telemetry_ack(app, cmd_seq, "INIT_SENSOR");
        sensor_check_and_report(app, cmd_seq);
    }
    else
    {
        char details[64];

        snprintf(details,
                 sizeof(details),
                 "status=%s",
                 sensor_status_to_string(status));

        telemetry_reject_detail(app, cmd_seq, "INIT_SENSOR_FAILED", details);

        if (app->fault == FAULT_NONE)
        {
            raise_fault(app, FAULT_SENSOR_MISSING);
        }
    }

    lcd_show_status_page(app);
}

void sensor_check_and_report(AppState *app, uint32_t cmd_seq)
{
    uint8_t whoami = 0u;
    SensorStatus status = mpu6050_read_whoami(&whoami);
    char fields[144];

    app->sensor.latest_sensor_status = status;
    app->sensor.latest_whoami = whoami;
    app->sensor.latest_sensor_tick = board_millis();
    app->sensor.sensor_status_valid = 1u;

    if (cmd_seq != 0u)
    {
        snprintf(
            fields,
            sizeof(fields),
            "cmd_seq=%lu,module=GY521,chip=MPU6050,status=%s,whoami=0x%02X",
            (unsigned long)cmd_seq,
            sensor_status_to_string(status),
            whoami);
    }
    else
    {
        snprintf(fields,
                 sizeof(fields),
                 "module=GY521,chip=MPU6050,status=%s,whoami=0x%02X",
                 sensor_status_to_string(status),
                 whoami);
    }

    telemetry_tlm(app, "SENSOR", fields);

    if (status == SENSOR_OK)
    {
        if (app->fault == FAULT_SENSOR_MISSING)
        {
            clear_fault_if_current(
                app, FAULT_SENSOR_MISSING, "sensor_recovered");

            telemetry_event(app,
                            "type=RECOVERY_ACTION,action=remain_in_safe,reason="
                            "operator_required");

            lcd_show_status_page(app);
        }

        return;
    }

    if (app->fault == FAULT_NONE)
    {
        raise_fault(app, FAULT_SENSOR_MISSING);
        lcd_show_status_page(app);
    }
}

void sensor_cached_report(AppState *app, uint32_t cmd_seq)
{
    char fields[128];

    if (app->sensor.sensor_status_valid == 0u)
    {
        telemetry_reject(app, cmd_seq, "NO_VALID_SENSOR_STATUS");
        return;
    }

    snprintf(fields,
             sizeof(fields),
             "cmd_seq=%lu,age_ms=%lu,status=%s,whoami=0x%02X",
             (unsigned long)cmd_seq,
             (unsigned long)(board_millis() - app->sensor.latest_sensor_tick),
             sensor_status_to_string(app->sensor.latest_sensor_status),
             app->sensor.latest_whoami);

    telemetry_tlm(app, "SENSOR_STATUS_CACHED", fields);
}

void imu_sample_and_report(AppState *app, uint32_t cmd_seq)
{
    ImuSample sample = {0};
    SensorStatus status = mpu6050_read_sample(&sample);

    if (status != SENSOR_OK)
    {
        char fields[96];

        if (cmd_seq != 0u)
        {
            char details[64];

            snprintf(details,
                     sizeof(details),
                     "status=%s",
                     sensor_status_to_string(status));

            telemetry_reject_detail(app, cmd_seq, "SAMPLE_IMU_FAILED", details);
        }

        snprintf(fields,
                 sizeof(fields),
                 "status=%s",
                 sensor_status_to_string(status));

        telemetry_tlm(app, "IMU", fields);

        if (app->fault == FAULT_NONE)
        {
            raise_fault(app, FAULT_SENSOR_MISSING);
            lcd_show_status_page(app);
        }

        return;
    }

    app->sensor.latest_imu = sample;
    app->sensor.latest_imu_tick = board_millis();
    app->sensor.imu_valid = 1u;

    {
        char fields[192];

        if (cmd_seq != 0u)
        {
            snprintf(fields,
                     sizeof(fields),
                     "cmd_seq=%lu,source=LIVE,ax=%d,ay=%d,az=%d,temp=%d,gx=%d,"
                     "gy=%d,gz=%d",
                     (unsigned long)cmd_seq,
                     (int)sample.ax,
                     (int)sample.ay,
                     (int)sample.az,
                     (int)sample.temp,
                     (int)sample.gx,
                     (int)sample.gy,
                     (int)sample.gz);
        }
        else
        {
            snprintf(fields,
                     sizeof(fields),
                     "source=LIVE,ax=%d,ay=%d,az=%d,temp=%d,gx=%d,gy=%d,gz=%d",
                     (int)sample.ax,
                     (int)sample.ay,
                     (int)sample.az,
                     (int)sample.temp,
                     (int)sample.gx,
                     (int)sample.gy,
                     (int)sample.gz);
        }

        telemetry_tlm(app, "IMU", fields);
    }

    lcd_show_imu_page(app, &sample);
}

void imu_cached_report(AppState *app, uint32_t cmd_seq)
{
    char fields[192];
    const ImuSample *sample = NULL;

    if (app->sensor.imu_valid == 0u)
    {
        telemetry_reject(app, cmd_seq, "NO_VALID_IMU_SAMPLE");
        return;
    }

    sample = &app->sensor.latest_imu;

    snprintf(
        fields,
        sizeof(fields),
        "cmd_seq=%lu,age_ms=%lu,ax=%d,ay=%d,az=%d,temp=%d,gx=%d,gy=%d,gz=%d",
        (unsigned long)cmd_seq,
        (unsigned long)(board_millis() - app->sensor.latest_imu_tick),
        (int)sample->ax,
        (int)sample->ay,
        (int)sample->az,
        (int)sample->temp,
        (int)sample->gx,
        (int)sample->gy,
        (int)sample->gz);

    telemetry_tlm(app, "IMU_CACHED", fields);
    lcd_show_imu_page(app, sample);
}
