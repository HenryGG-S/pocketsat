#include "app.h"
#include "app_config.h"
#include "board_nucleo_f401re.h"
#include "fdir.h"
#include "health_manager.h"
#include "lcd1602.h"
#include "mode.h"
#include "sensor_service.h"
#include "telemetry.h"

#include <stdio.h>
#include <string.h>

void app_init(AppState *app)
{
    char fields[96];
    char reset_cause_str[80];

    memset(app, 0, sizeof(*app));

    app->mode = MODE_BOOT;
    app->fault = FAULT_NONE;
    app->lcd_page = LCD_PAGE_STATUS;
    app->health.boot_ms = board_millis();

    lcd_init();
    lcd_show_status_page(app);

    BoardResetCause reset_cause = board_reset_cause_get();
    snprintf(reset_cause_str,
             sizeof(reset_cause_str),
             "type=BOOT,reset_cause=%s",
             board_reset_cause_to_string(reset_cause));
    telemetry_event(app, reset_cause_str);

    snprintf(fields, sizeof(fields), "name=%s,version=%s", FW_NAME, FW_VERSION);
    telemetry_tlm(app, "FW", fields);

    telemetry_tlm(app, "BOARD", "type=NUCLEO-F401RE");
    telemetry_tlm(app, "MCU", "type=STM32F401RE");
    telemetry_tlm(app, "MODE", "current=BOOT");

    app->mode = MODE_SAFE;
    telemetry_event(app,
                    "type=MODE_CHANGE,from=BOOT,to=SAFE,reason=boot_complete");

    lcd_show_status_page(app);
}

void app_tick(AppState *app)
{
    static uint32_t last_health_tick = 0u;
    static uint32_t health_counter = 0u;
    static uint32_t last_sensor_check_tick = 0u;
    static uint32_t last_imu_sample_tick = 0u;

    uint32_t now = board_millis();

    health_manager_note_app_tick(app);

    if ((now - last_health_tick) >= HEALTH_PERIOD_MS)
    {
        last_health_tick = now;
        health_counter++;

        board_heartbeat_toggle();

        if (app->telemetry.format == TELEMETRY_FORMAT_BINARY)
        {
            telemetry_binary_health(app, health_counter);
        }
        else
        {
            char fields[128];

            snprintf(fields,
                     sizeof(fields),
                     "counter=%lu,mode=%s,fault=%s",
                     (unsigned long)health_counter,
                     mode_to_string(app->mode),
                     fault_to_string(app->fault));

            telemetry_tlm(app, "HEALTH", fields);
        }

        lcd_refresh(app);

        health_manager_note_health_task(app);
    }

    if ((now - last_sensor_check_tick) >= SENSOR_CHECK_PERIOD_MS)
    {
        last_sensor_check_tick = now;
        sensor_check_and_report(app, 0u);

        health_manager_note_sensor_task(app);
    }

    if (app->fault == FAULT_NONE &&
        (app->mode == MODE_NOMINAL || app->mode == MODE_PAYLOAD) &&
        (now - last_imu_sample_tick) >= IMU_SAMPLE_PERIOD_MS)
    {
        last_imu_sample_tick = now;
        imu_sample_and_report(app, 0u);

        health_manager_note_imu_task(app);
    }

    health_manager_check_liveness(app);
}
