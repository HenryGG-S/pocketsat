#include "telemetry.h"
#include "board_nucleo_f401re.h"

#include <stdio.h>

void telemetry_emit(AppState *app, const char *record_type, const char *fields)
{
    char line[224];

    app->telemetry.telemetry_seq++;

    if (fields != NULL && fields[0] != '\0')
    {
        snprintf(line,
                 sizeof(line),
                 "%s,seq=%lu,tick=%lu,%s",
                 record_type,
                 (unsigned long)app->telemetry.telemetry_seq,
                 (unsigned long)board_millis(),
                 fields);
    }
    else
    {
        snprintf(line,
                 sizeof(line),
                 "%s,seq=%lu,tick=%lu",
                 record_type,
                 (unsigned long)app->telemetry.telemetry_seq,
                 (unsigned long)board_millis());
    }

    board_uart_write(line);
    board_uart_write("\r\n");
}

void telemetry_tlm(AppState *app, const char *tlm_type, const char *fields)
{
    char payload[176];

    if (fields != NULL && fields[0] != '\0')
    {
        snprintf(payload, sizeof(payload), "type=%s,%s", tlm_type, fields);
    }
    else
    {
        snprintf(payload, sizeof(payload), "type=%s", tlm_type);
    }

    telemetry_emit(app, "TLM", payload);
}

void telemetry_event(AppState *app, const char *event_fields)
{
    char payload[176];

    app->telemetry.event_seq++;

    if (event_fields != NULL && event_fields[0] != '\0')
    {
        snprintf(payload,
                 sizeof(payload),
                 "event_id=%lu,%s",
                 (unsigned long)app->telemetry.event_seq,
                 event_fields);
    }
    else
    {
        snprintf(payload,
                 sizeof(payload),
                 "event_id=%lu",
                 (unsigned long)app->telemetry.event_seq);
    }

    telemetry_emit(app, "EVENT", payload);
}

void telemetry_ack(AppState *app, uint32_t cmd_seq, const char *cmd_name)
{
    char fields[96];

    snprintf(fields,
             sizeof(fields),
             "cmd_seq=%lu,cmd=%s",
             (unsigned long)cmd_seq,
             cmd_name);

    telemetry_emit(app, "ACK", fields);
}

void telemetry_ack_detail(AppState *app,
                          uint32_t cmd_seq,
                          const char *cmd_name,
                          const char *details)
{
    char fields[144];

    if (details != NULL && details[0] != '\0')
    {
        snprintf(fields,
                 sizeof(fields),
                 "cmd_seq=%lu,cmd=%s,%s",
                 (unsigned long)cmd_seq,
                 cmd_name,
                 details);
    }
    else
    {
        snprintf(fields,
                 sizeof(fields),
                 "cmd_seq=%lu,cmd=%s",
                 (unsigned long)cmd_seq,
                 cmd_name);
    }

    telemetry_emit(app, "ACK", fields);
}

void telemetry_reject(AppState *app, uint32_t cmd_seq, const char *reason)
{
    telemetry_reject_detail(app, cmd_seq, reason, "");
}

void telemetry_reject_detail(AppState *app,
                             uint32_t cmd_seq,
                             const char *reason,
                             const char *details)
{
    char fields[160];

    if (details != NULL && details[0] != '\0')
    {
        snprintf(fields,
                 sizeof(fields),
                 "cmd_seq=%lu,reason=%s,%s",
                 (unsigned long)cmd_seq,
                 reason,
                 details);
    }
    else
    {
        snprintf(fields,
                 sizeof(fields),
                 "cmd_seq=%lu,reason=%s",
                 (unsigned long)cmd_seq,
                 reason);
    }

    telemetry_emit(app, "REJECT", fields);
}

void telemetry_deny(AppState *app,
                    uint32_t cmd_seq,
                    const char *reason,
                    const char *details)
{
    char fields[160];

    if (details != NULL && details[0] != '\0')
    {
        snprintf(fields,
                 sizeof(fields),
                 "cmd_seq=%lu,reason=%s,%s",
                 (unsigned long)cmd_seq,
                 reason,
                 details);
    }
    else
    {
        snprintf(fields,
                 sizeof(fields),
                 "cmd_seq=%lu,reason=%s",
                 (unsigned long)cmd_seq,
                 reason);
    }

    telemetry_emit(app, "DENY", fields);
}
