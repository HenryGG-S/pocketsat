# PocketSat Mk1 Requirements

PocketSat Mk1 is an embedded “space avionics” learning project. These requirements are intentionally lightweight and ECSS-inspired: they are written to make behaviour explicit, reviewable, and testable, but they do not claim formal ECSS compliance.

## Scope

Mk1 demonstrates a small flight-software-style control stack on a Nucleo-F401RE with:

- explicit operational modes;
- structured command responses;
- FDIR-owned fault behaviour;
- MPU6050/GY-521 sensor health and IMU sampling;
- LCD1602 local display;
- ASCII operator telemetry;
- binary HEALTH telemetry baseline;
- health-gated independent watchdog reset;
- reset-cause reporting;
- hardware-in-the-loop verification notes.

## Mode requirements

| ID | Requirement | Verification |
|---|---|---|
| REQ-MODE-001 | The system shall enter `SAFE` after boot completes. | HIL boot test. |
| REQ-MODE-002 | The system shall deny transitions out of `SAFE` while an active fault exists. | Fault injection and `SET_MODE NOMINAL`. |
| REQ-MODE-003 | The system shall allow transition from `SAFE` to `NOMINAL` only when no active fault exists. | `CLEAR_FAULTS`, then `SET_MODE NOMINAL`. |
| REQ-MODE-004 | The system shall allow transition between `NOMINAL` and `PAYLOAD` when no active fault exists. | Mode command test. |
| REQ-MODE-005 | The system shall always allow transition to `SAFE`. | Mode command test from each mode. |

## Command requirements

| ID | Requirement | Verification |
|---|---|---|
| REQ-CMD-001 | The system shall process line-based UART commands over USART2/ST-LINK VCOM at 115200 baud. | Serial command test. |
| REQ-CMD-002 | Every valid command shall produce an explicit response or telemetry output. | Command matrix test. |
| REQ-CMD-003 | Invalid commands shall be rejected with `REJECT` and reason `UNKNOWN_COMMAND`. | Send malformed command. |
| REQ-CMD-004 | Unknown command rejection shall include the received input string after sanitisation. | Send `PNG`. |
| REQ-CMD-005 | Unsupported mode transitions shall be denied with `DENY`, not silently ignored. | Invalid mode transition test. |
| REQ-CMD-006 | Overlong command input shall be rejected without corrupting the command buffer. | Buffer overflow test. |
| REQ-CMD-007 | Excessive command rate shall raise `COMMAND_STORM`. | Command storm test. |

## Telemetry requirements

| ID | Requirement | Verification |
|---|---|---|
| REQ-TLM-001 | ASCII telemetry records shall include a monotonically increasing telemetry sequence number. | Serial log inspection. |
| REQ-TLM-002 | ASCII telemetry records shall include the current HAL tick. | Serial log inspection. |
| REQ-TLM-003 | Event telemetry shall include a monotonically increasing event identifier. | Event log inspection. |
| REQ-TLM-004 | The system shall support ASCII telemetry as the default operator/debug format. | Boot and command tests. |
| REQ-TLM-005 | The system shall support binary HEALTH telemetry when commanded. | Binary capture and decoder test. |
| REQ-TLM-006 | Binary packet format shall be documented with version, type, length, sequence, payload, and CRC. | Packet specification review. |
| REQ-TLM-007 | A host-side decoder shall decode Mk1 binary HEALTH packets. | Decoder script test. |
| REQ-TLM-008 | Binary telemetry shall not remove the ASCII command/response operator path in Mk1. | Format switching test. |

## FDIR and watchdog requirements

| ID | Requirement | Verification |
|---|---|---|
| REQ-FDIR-001 | Fault state changes shall be owned by FDIR functions. | Code inspection. |
| REQ-FDIR-002 | Raising a fault outside `SAFE` shall force the system into `SAFE`. | Fault injection test. |
| REQ-FDIR-003 | `SENSOR_MISSING` shall be raised if the MPU6050 WHO_AM_I register cannot be read or returns an unexpected ID. | Sensor disconnect / I2C fault test. |
| REQ-FDIR-004 | `COMMAND_STORM` shall be raised if more than `COMMAND_STORM_LIMIT` complete commands are received within `COMMAND_STORM_WINDOW_MS`. | Command storm test. |
| REQ-FDIR-005 | `APP_STALL` shall be raised when required liveness markers become stale while the main loop continues. | Liveness fault test. |
| REQ-FDIR-006 | The independent watchdog shall only be refreshed when the health manager considers the system alive. | Code inspection and watchdog test. |
| REQ-FDIR-007 | The system shall report `IWDG` as reset cause after an independent-watchdog reset. | Watchdog trigger test. |
| REQ-FDIR-008 | The watchdog policy shall include a startup grace period to avoid reset before initial periodic tasks run. | Boot stability test. |

## Sensor requirements

| ID | Requirement | Verification |
|---|---|---|
| REQ-SNS-001 | `CHECK_SENSOR` shall actively read MPU6050 `WHO_AM_I`. | Command test. |
| REQ-SNS-002 | `INIT_SENSOR` shall verify `WHO_AM_I` before waking/configuring the MPU6050. | Command test. |
| REQ-SNS-003 | `SAMPLE_IMU` shall perform an immediate MPU6050 burst sample attempt. | Command test. |
| REQ-SNS-004 | The latest valid IMU sample shall be cached. | `SAMPLE_IMU`, then `GET_IMU`. |
| REQ-SNS-005 | `GET_IMU` shall reject if no valid IMU sample has been acquired. | Boot then `GET_IMU`. |
| REQ-SNS-006 | `GET_SENSOR_STATUS` shall report the latest cached sensor status. | Sensor status command test. |

## Board and local display requirements

| ID | Requirement | Verification |
|---|---|---|
| REQ-HW-001 | The board target shall be STM32 Nucleo-F401RE. | Build configuration and hardware inspection. |
| REQ-HW-002 | The GY-521/MPU6050 shall use I2C1 on PB8/PB9 with AD0 grounded. | Wiring inspection and sensor test. |
| REQ-HW-003 | The LCD1602 shall use the documented Nucleo D2-D7 4-bit mapping. | Wiring inspection and LCD test. |
| REQ-HW-004 | LD2/PA5 shall provide a heartbeat indication. | Visual observation. |
| REQ-HW-005 | UART RX shall use an interrupt-backed ring buffer to avoid losing typed characters during blocking work. | Stress typing and `GET_HEALTH` diagnostics. |

## Mk1 non-requirements

These are intentionally out of scope for Mk1:

- binary command uplink;
- RTOS scheduling;
- DMA UART TX/RX;
- flash-persistent event log;
- custom PCB;
- formal ECSS qualification;
- full MISRA compliance;
- autonomous payload science mission.

