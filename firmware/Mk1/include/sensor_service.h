#ifndef SENSOR_SERVICE_H
#define SENSOR_SERVICE_H

#include "app_types.h"
#include <stdint.h>

const char *sensor_status_to_string(SensorStatus status);

void sensor_init_and_report(AppState *app, uint32_t cmd_seq);
void sensor_check_and_report(AppState *app, uint32_t cmd_seq);
void sensor_cached_report(AppState *app, uint32_t cmd_seq);

void imu_sample_and_report(AppState *app, uint32_t cmd_seq);
void imu_cached_report(AppState *app, uint32_t cmd_seq);

#endif
