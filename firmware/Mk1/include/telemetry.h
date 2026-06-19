#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "app_types.h"
#include <stdint.h>

const char *telemetry_format_to_string(TelemetryFormat format);

void telemetry_emit(AppState *app, const char *prefix, const char *fields);

void telemetry_tlm(AppState *app, const char *type, const char *fields);
void telemetry_event(AppState *app, const char *event_fields);

void telemetry_ack(AppState *app, uint32_t cmd_seq, const char *cmd);
void telemetry_ack_detail(AppState *app,
                          uint32_t cmd_seq,
                          const char *cmd,
                          const char *detail);

void telemetry_reject(AppState *app, uint32_t cmd_seq, const char *reason);
void telemetry_reject_detail(AppState *app,
                             uint32_t cmd_seq,
                             const char *reason,
                             const char *detail);

void telemetry_deny(AppState *app,
                    uint32_t cmd_seq,
                    const char *reason,
                    const char *detail);

void telemetry_binary_health(AppState *app, uint32_t health_counter);

#endif