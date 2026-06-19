#ifndef HEALTH_MANAGER_H
#define HEALTH_MANAGER_H

#include "app_types.h"
#include <stdint.h>

void health_manager_note_app_tick(AppState *app);
void health_manager_note_health_task(AppState *app);
void health_manager_note_sensor_task(AppState *app);
void health_manager_note_imu_task(AppState *app);

void health_manager_report(AppState *app, uint32_t cmd_seq);

uint8_t health_manager_system_alive(const AppState *app);
void health_manager_check_liveness(AppState *app);

#endif