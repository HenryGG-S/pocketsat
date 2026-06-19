#include "command.h"
#include "board_nucleo_f401re.h"
#include "fdir.h"
#include "health_manager.h"
#include "lcd1602.h"
#include "mode.h"
#include "sensor_service.h"
#include "telemetry.h"

#include <stdio.h>
#include <string.h>

static void command_handle_line(const char *line, AppState *app);
static void
command_make_input_detail(const char *line, char *detail, size_t detail_size);
static uint8_t watchdog_test_armed = 0u;

void command_process_byte(
    uint8_t byte, char *buffer, size_t *length, size_t capacity, AppState *app)
{
    if (byte == '\r')
    {
        return;
    }

    if (byte == '\n')
    {
        buffer[*length] = '\0';

        command_handle_line(buffer, app);

        *length = 0u;
        buffer[0] = '\0';

        return;
    }

    if (*length >= capacity - 1u)
    {
        telemetry_reject(app, 0u, "COMMAND_BUFFER_OVERFLOW");

        *length = 0u;
        buffer[0] = '\0';

        return;
    }

    buffer[*length] = (char)byte;
    *length = *length + 1u;
    buffer[*length] = '\0';
}

static void command_handle_line(const char *line, AppState *app)
{
    ParsedCommand cmd = {0};
    uint32_t cmd_seq = 0u;

    app->telemetry.command_seq++;
    cmd_seq = app->telemetry.command_seq;

    if (command_rate_note(&app->command_rate) != 0u)
    {
        telemetry_reject(app, cmd_seq, "COMMAND_STORM");
        raise_fault(app, FAULT_COMMAND_STORM);
        lcd_show_status_page(app);
        return;
    }

    if (strcmp(line, "") == 0)
    {
        telemetry_reject(app, cmd_seq, "EMPTY_COMMAND");
        return;
    }

    cmd = command_parse(line);

    if (cmd.id == CMD_INVALID)
    {
        char detail[64];
        command_make_input_detail(line, detail, sizeof(detail));
        telemetry_reject_detail(app, cmd_seq, "UNKNOWN_COMMAND", detail);
        return;
    }

    command_execute(&cmd, app, cmd_seq);
}

static void
command_make_input_detail(const char *line, char *detail, size_t detail_size)
{
    char input[64];
    size_t in_index = 0u;
    size_t out_index = 0u;

    if (detail_size == 0u)
    {
        return;
    }

    detail[0] = '\0';

    if (line == NULL)
    {
        snprintf(detail, detail_size, "input=<null>");
        return;
    }

    while (line[in_index] != '\0' && out_index < (sizeof(input) - 1u))
    {
        char ch = line[in_index];

        if (ch == ',')
        {
            input[out_index] = ';';
        }
        else if ((unsigned char)ch < 32u || (unsigned char)ch > 126u)
        {
            input[out_index] = '?';
        }
        else
        {
            input[out_index] = ch;
        }

        in_index++;
        out_index++;
    }

    input[out_index] = '\0';

    snprintf(detail, detail_size, "input=%s", input);
}

ParsedCommand command_parse(const char *line)
{
    ParsedCommand cmd = {0};

    cmd.id = CMD_INVALID;
    cmd.requested_mode = MODE_SAFE;
    cmd.requested_fault = FAULT_NONE;

    if (strcmp(line, "PING") == 0)
    {
        cmd.id = CMD_PING;
    }
    else if (strcmp(line, "GET_STATUS") == 0)
    {
        cmd.id = CMD_GET_STATUS;
    }
    else if (strcmp(line, "GET_HEALTH") == 0)
    {
        cmd.id = CMD_GET_HEALTH;
    }
    else if (strcmp(line, "CHECK_SENSOR") == 0)
    {
        cmd.id = CMD_CHECK_SENSOR;
    }
    else if (strcmp(line, "DEBUG_I2C") == 0)
    {
        cmd.id = CMD_DEBUG_I2C;
    }
    else if (strcmp(line, "INIT_SENSOR") == 0)
    {
        cmd.id = CMD_INIT_SENSOR;
    }
    else if (strcmp(line, "SAMPLE_IMU") == 0)
    {
        cmd.id = CMD_SAMPLE_IMU;
    }
    else if (strcmp(line, "GET_IMU") == 0)
    {
        cmd.id = CMD_GET_IMU;
    }
    else if (strcmp(line, "GET_SENSOR_STATUS") == 0)
    {
        cmd.id = CMD_GET_SENSOR_STATUS;
    }
    else if (strcmp(line, "SET_MODE SAFE") == 0)
    {
        cmd.id = CMD_SET_MODE;
        cmd.requested_mode = MODE_SAFE;
    }
    else if (strcmp(line, "SET_MODE NOMINAL") == 0)
    {
        cmd.id = CMD_SET_MODE;
        cmd.requested_mode = MODE_NOMINAL;
    }
    else if (strcmp(line, "SET_MODE PAYLOAD") == 0)
    {
        cmd.id = CMD_SET_MODE;
        cmd.requested_mode = MODE_PAYLOAD;
    }
    else if (strcmp(line, "INJECT_FAULT SENSOR_MISSING") == 0)
    {
        cmd.id = CMD_INJECT_FAULT;
        cmd.requested_fault = FAULT_SENSOR_MISSING;
    }
    else if (strcmp(line, "INJECT_FAULT OVERTEMP") == 0)
    {
        cmd.id = CMD_INJECT_FAULT;
        cmd.requested_fault = FAULT_OVERTEMP;
    }
    else if (strcmp(line, "INJECT_FAULT COMMAND_STORM") == 0)
    {
        cmd.id = CMD_INJECT_FAULT;
        cmd.requested_fault = FAULT_COMMAND_STORM;
    }
    else if (strcmp(line, "CLEAR_FAULTS") == 0)
    {
        cmd.id = CMD_CLEAR_FAULTS;
    }
    else if (strcmp(line, "TEST_WATCHDOG ARM") == 0)
    {
        cmd.id = CMD_TEST_WATCHDOG_ARM;
    }
    else if (strcmp(line, "TEST_WATCHDOG TRIGGER") == 0)
    {
        cmd.id = CMD_TEST_WATCHDOG_TRIGGER;
    }

    return cmd;
}

void command_execute(const ParsedCommand *cmd, AppState *app, uint32_t cmd_seq)
{
    switch (cmd->id)
    {
        case CMD_PING:
            telemetry_ack(app, cmd_seq, "PING");
            break;

        case CMD_GET_STATUS:
        {
            char fields[96];

            snprintf(fields,
                     sizeof(fields),
                     "cmd_seq=%lu,mode=%s,fault=%s",
                     (unsigned long)cmd_seq,
                     mode_to_string(app->mode),
                     fault_to_string(app->fault));

            telemetry_tlm(app, "STATUS", fields);
            lcd_show_status_page(app);
            break;
        }
        case CMD_GET_HEALTH:
            health_manager_report(app, cmd_seq);
            break;

        case CMD_CHECK_SENSOR:
            sensor_check_and_report(app, cmd_seq);
            break;

        case CMD_DEBUG_I2C:
        {
            char fields[128];

            snprintf(fields,
                     sizeof(fields),
                     "cmd_seq=%lu,error=0x%08lX,scl=%lu,sda=%lu",
                     (unsigned long)cmd_seq,
                     (unsigned long)board_i2c1_error(),
                     (unsigned long)board_i2c1_scl_state(),
                     (unsigned long)board_i2c1_sda_state());

            telemetry_tlm(app, "I2C", fields);
            break;
        }

        case CMD_INIT_SENSOR:
            sensor_init_and_report(app, cmd_seq);
            break;

        case CMD_SAMPLE_IMU:
            imu_sample_and_report(app, cmd_seq);
            break;

        case CMD_GET_IMU:
            imu_cached_report(app, cmd_seq);
            break;

        case CMD_GET_SENSOR_STATUS:
            sensor_cached_report(app, cmd_seq);
            break;

        case CMD_SET_MODE:
            request_mode_change(app, cmd->requested_mode, cmd_seq);
            lcd_show_status_page(app);
            break;

        case CMD_INJECT_FAULT:
        {
            char details[64];

            snprintf(details,
                     sizeof(details),
                     "fault=%s",
                     fault_to_string(cmd->requested_fault));

            telemetry_ack_detail(app, cmd_seq, "INJECT_FAULT", details);
            raise_fault(app, cmd->requested_fault);
            lcd_show_status_page(app);
            break;
        }

        case CMD_CLEAR_FAULTS:
            clear_faults(app, cmd_seq);
            lcd_show_status_page(app);
            break;

        case CMD_TEST_WATCHDOG_ARM:
            if (app->mode != MODE_SAFE)
            {
                telemetry_deny(app, cmd_seq, "MODE_NOT_SAFE", "required=SAFE");
                break;
            }

            watchdog_test_armed = 1u;
            telemetry_ack(app, cmd_seq, "TEST_WATCHDOG ARM");
            break;

        case CMD_TEST_WATCHDOG_TRIGGER:
            if (watchdog_test_armed == 0u)
            {
                telemetry_deny(app, cmd_seq, "WATCHDOG_TEST_NOT_ARMED", "");
                break;
            }

            telemetry_ack(app, cmd_seq, "TEST_WATCHDOG TRIGGER");

            while (1)
            {
                /*
                 * Intentional stall.
                 * Do not kick watchdog.
                 */
            }

        default:
            telemetry_reject(app, cmd_seq, "CMD_NOT_IMPLEMENTED");
            break;
    }
}
