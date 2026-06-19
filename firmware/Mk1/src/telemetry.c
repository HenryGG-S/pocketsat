#include "telemetry.h"
#include "board_nucleo_f401re.h"
#include "packet.h"

#include <stddef.h>
#include <stdio.h>

const char *telemetry_format_to_string(TelemetryFormat format)
{
    switch (format)
    {
        case TELEMETRY_FORMAT_ASCII:
            return "ASCII";

        case TELEMETRY_FORMAT_BINARY:
            return "BINARY";

        default:
            return "UNKNOWN";
    }
}

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

void telemetry_binary_health(AppState *app, uint32_t health_counter)
{
    uint8_t payload[PACKET_MAX_PAYLOAD_SIZE];
    uint8_t frame[PACKET_MAX_FRAME_SIZE];
    size_t payload_len = 0u;
    size_t frame_len = 0u;
    uint32_t now = board_millis();

    app->telemetry.telemetry_seq++;
    app->telemetry.packet_seq++;

    payload_len = packet_put_u32_le(payload, payload_len, now);
    payload_len = packet_put_u32_le(
        payload, payload_len, app->telemetry.telemetry_seq);
    payload_len = packet_put_u32_le(payload, payload_len, health_counter);
    payload_len = packet_put_u8(payload, payload_len, (uint8_t)app->mode);
    payload_len = packet_put_u8(payload, payload_len, (uint8_t)app->fault);
    payload_len = packet_put_u32_le(
        payload, payload_len, now - app->health.boot_ms);
    payload_len = packet_put_u32_le(
        payload, payload_len, now - app->health.last_app_tick_ms);
    payload_len = packet_put_u16_le(
        payload, payload_len, board_uart_rx_buffer_used());
    payload_len = packet_put_u32_le(
        payload, payload_len, board_uart_rx_overflow_count());

    frame_len = packet_encode(frame,
                              sizeof(frame),
                              PACKET_TYPE_HEALTH,
                              app->telemetry.packet_seq,
                              payload,
                              (uint8_t)payload_len);

    if (frame_len != 0u)
    {
        board_uart_write_bytes(frame, (uint16_t)frame_len);
    }
}
