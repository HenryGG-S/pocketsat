#include "mode.h"
#include "fdir.h"
#include "telemetry.h"

#include <stdio.h>

const char *mode_to_string(SystemMode mode)
{
    switch (mode)
    {
        case MODE_BOOT:
            return "BOOT";

        case MODE_SAFE:
            return "SAFE";

        case MODE_NOMINAL:
            return "NOMINAL";

        case MODE_PAYLOAD:
            return "PAYLOAD";

        default:
            return "UNKNOWN";
    }
}

void request_mode_change(AppState *app, SystemMode requested, uint32_t cmd_seq)
{
    SystemMode previous = app->mode;

    if (app->fault != FAULT_NONE && requested != MODE_SAFE)
    {
        char details[96];

        snprintf(details,
                 sizeof(details),
                 "fault=%s,requested=%s",
                 fault_to_string(app->fault),
                 mode_to_string(requested));

        telemetry_deny(app, cmd_seq, "ACTIVE_FAULT", details);
        return;
    }

    if (mode_transition_allowed(previous, requested) == 0u)
    {
        char details[96];

        snprintf(details,
                 sizeof(details),
                 "from=%s,to=%s",
                 mode_to_string(previous),
                 mode_to_string(requested));

        telemetry_deny(app, cmd_seq, "MODE_NOT_ALLOWED", details);
        return;
    }

    if (previous == requested)
    {
        char details[64];

        snprintf(details,
                 sizeof(details),
                 "mode=%s,result=NO_CHANGE",
                 mode_to_string(requested));

        telemetry_ack_detail(app, cmd_seq, "SET_MODE", details);
        return;
    }

    app->mode = requested;

    {
        char details[64];

        snprintf(details, sizeof(details), "mode=%s", mode_to_string(requested));

        telemetry_ack_detail(app, cmd_seq, "SET_MODE", details);
    }

    {
        char event[96];

        snprintf(event,
                 sizeof(event),
                 "type=MODE_CHANGE,from=%s,to=%s,reason=command",
                 mode_to_string(previous),
                 mode_to_string(requested));

        telemetry_event(app, event);
    }
}

uint8_t mode_transition_allowed(SystemMode from, SystemMode to)
{
    if (from == to)
    {
        return 1u;
    }

    if (to == MODE_SAFE)
    {
        return 1u;
    }

    if (from == MODE_SAFE && to == MODE_NOMINAL)
    {
        return 1u;
    }

    if (from == MODE_NOMINAL && to == MODE_PAYLOAD)
    {
        return 1u;
    }

    if (from == MODE_PAYLOAD && to == MODE_NOMINAL)
    {
        return 1u;
    }

    return 0u;
}
