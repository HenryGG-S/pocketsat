# PocketSat Mk1 Test Report

This document closes the Mk1 bench-verification campaign. It records observed behaviour for the Mk1 firmware and hardware stack. Results marked `PASS` should be re-run and updated if hardware, wiring, timing constants, watchdog timeout, or protocol format change.

## Test environment

| Item | Value |
|---|---|
| Board | STM32 Nucleo-F401RE |
| Sensor | GY-521 / MPU6050 over I2C1 |
| Display | LCD1602 4-bit parallel |
| UART | USART2/ST-LINK VCOM |
| Baud | 115200 |
| Firmware | `pocketsat-mk1`, version `0.5.0` |
| Test date | 2026-06-19 |
| Test type | Hardware-in-the-loop bench test |

## Summary

| Area | Result | Notes |
|---|---:|---|
| Boot and SAFE entry | PASS | Boots to SAFE and reports boot telemetry. |
| Command parser | PASS | Valid commands ACK/respond; invalid commands REJECT. |
| Mode safety | PASS | Active faults prevent leaving SAFE. |
| Sensor health | PASS | MPU6050 health check and init operational. |
| IMU sampling/cache | PASS | Live sample and cached `GET_IMU` path operational. |
| LCD output | PASS | Status and IMU pages display correctly. |
| UART RX reliability | PASS | Interrupt RX ring prevents missed typed characters during blocking telemetry/I2C work. |
| Health manager | PASS | `GET_HEALTH` reports task ages and UART diagnostics. |
| Independent watchdog | PASS | Health-gated kick works; deliberate stall causes watchdog reset. |
| Reset cause telemetry | PASS | IWDG reset is reported after watchdog-triggered reset. |
| Binary HEALTH packet | PASS | Binary mode emits documented HEALTH packet and host decoder decodes capture. |

## Verification matrix

| Requirement | Procedure | Expected | Result |
|---|---|---|---:|
| REQ-MODE-001 | Reset board and observe boot telemetry/LCD. | Boot event, BOOT→SAFE event, LCD SAFE/NONE. | PASS |
| REQ-MODE-002 | `INJECT_FAULT OVERTEMP`, then `SET_MODE NOMINAL`. | DENY with `ACTIVE_FAULT`; mode remains SAFE. | PASS |
| REQ-CMD-003 | Send malformed command. | `REJECT`, reason `UNKNOWN_COMMAND`. | PASS |
| REQ-CMD-004 | Send `PNG`. | `REJECT`, reason `UNKNOWN_COMMAND`, input reported as `PNG`. | PASS |
| REQ-FDIR-004 | Send more than 8 commands within 1 second. | `COMMAND_STORM` fault raised; system SAFE. | PASS |
| REQ-SNS-001 | Send `CHECK_SENSOR`. | Sensor telemetry reports MPU6050 status and WHO_AM_I. | PASS |
| REQ-SNS-002 | Send `INIT_SENSOR`. | Sensor verified and woken/configured. | PASS |
| REQ-SNS-003 | Send `SAMPLE_IMU`. | Live IMU telemetry emitted. | PASS |
| REQ-SNS-005 | Send `GET_IMU` before valid sample. | `REJECT`, reason `NO_VALID_IMU_SAMPLE`. | PASS |
| REQ-SNS-004 | Send `SAMPLE_IMU`, then `GET_IMU`. | Cached IMU telemetry emitted with `age_ms`. | PASS |
| REQ-FDIR-006 | Inspect `main.c` watchdog gate. | Watchdog kick only occurs through health-manager alive check. | PASS |
| REQ-FDIR-007 | `TEST_WATCHDOG ARM`, then `TEST_WATCHDOG TRIGGER`. | MCU resets; boot telemetry reports `IWDG`. | PASS |
| REQ-TLM-005 | `SET_TLM_FORMAT BINARY`; capture health bytes. | Binary HEALTH packet emitted. | PASS |
| REQ-TLM-007 | Decode capture with `scripts/decode_packets.py`. | JSON HEALTH packet decoded with valid CRC. | PASS |

## Command smoke test

Recommended release smoke sequence:

```text
PING
GET_STATUS
GET_HEALTH
CHECK_SENSOR
GET_SENSOR_STATUS
GET_IMU
INIT_SENSOR
SET_MODE NOMINAL
SAMPLE_IMU
GET_IMU
SET_MODE PAYLOAD
INJECT_FAULT OVERTEMP
SET_MODE NOMINAL
CLEAR_FAULTS
SET_MODE SAFE
SET_TLM_FORMAT BINARY
SET_TLM_FORMAT ASCII
```

Expected outcome:

- every command produces `ACK`, `DENY`, `REJECT`, or documented telemetry;
- active faults prevent unsafe mode transitions;
- `GET_HEALTH` reports `uart_rx_overflows=0` under normal interactive use;
- binary format can be entered and exited without losing command authority.

## Watchdog verification procedure

```text
GET_HEALTH
TEST_WATCHDOG ARM
TEST_WATCHDOG TRIGGER
```

Expected:

1. `TEST_WATCHDOG ARM` is accepted only in SAFE.
2. `TEST_WATCHDOG TRIGGER` acknowledges and intentionally stalls the application.
3. The health-gated watchdog is no longer refreshed.
4. The independent watchdog resets the MCU.
5. Boot telemetry after reset reports `reset_cause=IWDG`.
6. The system returns to SAFE.

Result: PASS.

## Binary telemetry verification procedure

```text
SET_TLM_FORMAT BINARY
```

Capture serial output to a binary file, then run:

```bash
python3 firmware/Mk1/scripts/decode_packets.py capture.bin
```

Expected:

- at least one HEALTH packet is decoded;
- CRC is valid;
- decoded mode/fault values match system state;
- ASCII command path remains available.

Result: PASS.

## Known limitations from testing

- Telemetry TX remains blocking in Mk1; RX is interrupt-buffered to avoid losing commands.
- Binary telemetry is limited to the HEALTH packet baseline.
- Binary command uplink is intentionally not implemented.
- Watchdog timeout is approximate because the STM32 LSI clock tolerance is broad.
- No flash-persistent fault/event log exists in Mk1.

