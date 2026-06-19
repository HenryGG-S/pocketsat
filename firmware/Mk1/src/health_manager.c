#include "health_manager.h"
#include "board_nucleo_f401re.h"
#include "fdir.h"
#include "mode.h"
#include "telemetry.h"

#include <stdio.h>

static void format_age_field(char *buffer,
                             size_t buffer_size,
                             const char *name,
                             uint8_t seen,
                             uint32_t last_tick)
{
    if (seen == 0u)
    {
        snprintf(buffer, buffer_size, "%s_age_ms=NA", name);
        return;
    }

    snprintf(buffer,
             buffer_size,
             "%s_age_ms=%lu",
             name,
             (unsigned long)(board_millis() - last_tick));
}

void health_manager_note_app_tick(AppState *app)
{
    app->health.last_app_tick_ms = board_millis();
}

void health_manager_note_health_task(AppState *app)
{
    app->health.last_health_ms = board_millis();
    app->health.health_task_seen = 1u;
}

void health_manager_note_sensor_task(AppState *app)
{
    app->health.last_sensor_check_ms = board_millis();
    app->health.sensor_task_seen = 1u;
}

void health_manager_note_imu_task(AppState *app)
{
    app->health.last_imu_sample_ms = board_millis();
    app->health.imu_task_seen = 1u;
}

void health_manager_report(AppState *app, uint32_t cmd_seq)
{
    char health_age[32];
    char sensor_age[32];
    char imu_age[32];
    char fields[192];

    format_age_field(health_age,
                     sizeof(health_age),
                     "health",
                     app->health.health_task_seen,
                     app->health.last_health_ms);

    format_age_field(sensor_age,
                     sizeof(sensor_age),
                     "sensor",
                     app->health.sensor_task_seen,
                     app->health.last_sensor_check_ms);

    format_age_field(imu_age,
                     sizeof(imu_age),
                     "imu",
                     app->health.imu_task_seen,
                     app->health.last_imu_sample_ms);

    snprintf(fields,
             sizeof(fields),
             "cmd_seq=%lu,mode=%s,fault=%s,%s,%s,%s",
             (unsigned long)cmd_seq,
             mode_to_string(app->mode),
             fault_to_string(app->fault),
             health_age,
             sensor_age,
             imu_age);

    telemetry_tlm(app, "HEALTH_DETAIL", fields);
}

uint8_t health_manager_system_alive(const AppState *app)
{
    if (app->health.health_task_seen == 0u)
    {
        return 0u;
    }

    if (app->health.sensor_task_seen == 0u)
    {
        return 0u;
    }

    return 1u;
}

void health_manager_check_liveness(AppState *app)
{
    uint32_t now = board_millis();

    if (app->health.health_task_seen != 0u &&
        (now - app->health.last_health_ms) > HEALTH_TASK_TIMEOUT_MS)
    {
        raise_fault(app, FAULT_APP_STALL);
        return;
    }

    if (app->health.sensor_task_seen != 0u &&
        (now - app->health.last_sensor_check_ms) > SENSOR_TASK_TIMEOUT_MS)
    {
        raise_fault(app, FAULT_APP_STALL);
        return;
    }

    if (app->fault == FAULT_NONE &&
        (app->mode == MODE_NOMINAL || app->mode == MODE_PAYLOAD) &&
        app->health.imu_task_seen != 0u &&
        (now - app->health.last_imu_sample_ms) > IMU_TASK_TIMEOUT_MS)
    {
        raise_fault(app, FAULT_APP_STALL);
        return;
    }
}