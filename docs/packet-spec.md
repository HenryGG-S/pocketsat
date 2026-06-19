# PocketSat Mk1 Packet Specification

This document is the top-level packet interface summary. The firmware-local protocol document lives at `firmware/Mk1/docs/protocol.md`; this file exists so the project-level documentation has a stable interface-control entry point.

## Design intent

Mk1 keeps ASCII telemetry and command responses as the operator/debug interface. Binary packets are introduced only for machine-readable telemetry. This avoids making the board difficult to operate during bring-up while still establishing a disciplined packet protocol for Mk2.

## Transport

| Field | Value |
|---|---|
| Physical link | USART2 via ST-LINK virtual COM port |
| Baud | 115200 |
| Command input | ASCII line commands |
| Default telemetry | ASCII |
| Optional telemetry | Binary HEALTH packet |

## Frame format

All multi-byte integer fields are little-endian.

| Offset | Field | Size | Description |
|---:|---|---:|---|
| 0 | `SYNC_0` | 1 | `0x50` (`'P'`) |
| 1 | `SYNC_1` | 1 | `0x53` (`'S'`) |
| 2 | `VERSION` | 1 | Protocol version, currently `0x01` |
| 3 | `TYPE` | 1 | Packet type |
| 4 | `SEQ` | 2 | Binary packet sequence counter |
| 6 | `LEN` | 1 | Payload length in bytes |
| 7 | `PAYLOAD` | `LEN` | Packet payload |
| 7 + LEN | `CRC16` | 2 | CRC-16/CCITT-FALSE over header and payload |

## Packet types

| Type | Name | Mk1 status |
|---:|---|---|
| `0x01` | `HEALTH` | Implemented |
| `0x02` | `STATUS` | Reserved for Mk2 |
| `0x03` | `IMU` | Reserved for Mk2 |
| `0x04` | `SENSOR` | Reserved for Mk2 |
| `0x05` | `EVENT` | Reserved for Mk2 |
| `0x06` | `ACK` | Reserved for Mk2 |
| `0x07` | `REJECT` | Reserved for Mk2 |
| `0x08` | `DENY` | Reserved for Mk2 |

## HEALTH payload

| Offset | Field | Type | Description |
|---:|---|---|---|
| 0 | `tick_ms` | `uint32` | HAL tick when packet was generated |
| 4 | `telemetry_seq` | `uint32` | Current telemetry sequence |
| 8 | `health_counter` | `uint32` | Health task counter |
| 12 | `mode` | `uint8` | Current system mode enum value |
| 13 | `fault` | `uint8` | Current system fault enum value |
| 14 | `app_age_ms` | `uint32` | Time since app init |
| 18 | `app_tick_age_ms` | `uint32` | Age of most recent app tick marker |
| 22 | `uart_rx_used` | `uint16` | UART RX ring occupancy |
| 24 | `uart_rx_overflows` | `uint32` | UART RX overflow counter |

Total payload size: 28 bytes.

## Mode values

| Value | Name |
|---:|---|
| 0 | `BOOT` |
| 1 | `SAFE` |
| 2 | `NOMINAL` |
| 3 | `PAYLOAD` |

## Fault values

| Value | Name |
|---:|---|
| 0 | `NONE` |
| 1 | `SENSOR_MISSING` |
| 2 | `OVERTEMP` |
| 3 | `COMMAND_STORM` |
| 4 | `APP_STALL` |

## Telemetry format control

```text
SET_TLM_FORMAT ASCII
SET_TLM_FORMAT BINARY
```

Mk1 binary mode affects periodic HEALTH telemetry only. Operator command responses, events, rejects, denies, and ad-hoc status reports remain ASCII.

## Decoder

```bash
python3 firmware/Mk1/scripts/decode_packets.py capture.bin
```

The decoder shall:

- search for sync bytes;
- validate version and length;
- validate CRC;
- decode HEALTH payloads to JSON lines;
- resynchronise after invalid bytes.

## Mk1 constraints

- No binary command uplink.
- No packet retransmission.
- No packet authentication.
- No flow control.
- No binary event log yet.

