#ifndef COMMAND_H
#define COMMAND_H

#include "app_types.h"
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    CMD_INVALID = 0,
    CMD_PING,
    CMD_GET_STATUS,
    CMD_GET_HEALTH,
    CMD_CHECK_SENSOR,
    CMD_DEBUG_I2C,
    CMD_INIT_SENSOR,
    CMD_SAMPLE_IMU,
    CMD_GET_IMU,
    CMD_GET_SENSOR_STATUS,
    CMD_SET_MODE,
    CMD_INJECT_FAULT,
    CMD_CLEAR_FAULTS,
    CMD_TEST_WATCHDOG_ARM,
    CMD_TEST_WATCHDOG_TRIGGER
} CommandId;

typedef struct
{
    CommandId id;
    SystemMode requested_mode;
    SystemFault requested_fault;
} ParsedCommand;

void command_process_byte(
    uint8_t byte, char *buffer, size_t *length, size_t capacity, AppState *app);

ParsedCommand command_parse(const char *line);

void command_execute(const ParsedCommand *cmd, AppState *app, uint32_t cmd_seq);

#endif