#include "fdir.h"
#include "app_config.h"
#include "board_nucleo_f401re.h"
#include "mode.h"
#include "telemetry.h"

#include <limits.h>
#include <stdio.h>

const char *fault_to_string(SystemFault fault)
{
    switch (fault)
    {
        case FAULT_NONE:
            return "NONE";

        case FAULT_SENSOR_MISSING:
            return "SENSOR_MISSING";

        case FAULT_OVERTEMP:
            return "OVERTEMP";

        case FAULT_COMMAND_STORM:
            return "COMMAND_STORM";

        default:
            return "UNKNOWN";
    }
}

void raise_fault(AppState *app, SystemFault fault)
{
    char event[128];

    if (fault == FAULT_NONE)
    {
        return;
    }

    if (app->fault == fault)
    {
        return;
    }

    app->fault = fault;

    snprintf(event,
             sizeof(event),
             "type=FAULT_RAISED,fault=%s",
             fault_to_string(fault));

    telemetry_event(app, event);

    if (app->mode != MODE_SAFE)
    {
        SystemMode previous = app->mode;
        app->mode = MODE_SAFE;

        snprintf(event,
                 sizeof(event),
                 "type=MODE_CHANGE,from=%s,to=SAFE,reason=fault",
                 mode_to_string(previous));

        telemetry_event(app, event);
    }
}

void clear_faults(AppState *app, uint32_t cmd_seq)
{
    SystemFault previous = app->fault;

    app->fault = FAULT_NONE;

    telemetry_ack(app, cmd_seq, "CLEAR_FAULTS");

    if (previous != FAULT_NONE)
    {
        char event[96];

        snprintf(event,
                 sizeof(event),
                 "type=FAULT_CLEARED,fault=%s,reason=operator_command",
                 fault_to_string(previous));

        telemetry_event(app, event);
    }
    else
    {
        telemetry_event(app, "type=FAULT_CLEAR_REQUEST,result=NO_ACTIVE_FAULT");
    }
}

uint8_t command_rate_note(CommandRateState *rate)
{
    uint32_t now = board_millis();

    if ((now - rate->window_start_tick) >= COMMAND_STORM_WINDOW_MS)
    {
        rate->window_start_tick = now;
        rate->count = 0u;
    }

    if (rate->count < UINT8_MAX)
    {
        rate->count++;
    }

    return (rate->count > COMMAND_STORM_LIMIT) ? 1u : 0u;
}
