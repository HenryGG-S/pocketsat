# PocketSat

PocketSat is a small embedded “space avionics” learning project built around disciplined firmware architecture, explicit modes, fault handling, telemetry, and hardware-in-the-loop verification.

The project is intentionally modest in hardware scope, but the firmware is structured as though it were a serious embedded avionics stack: clear module ownership, bounded command handling, FDIR-owned faults, health-gated watchdog refresh, reset-cause reporting, documented telemetry, and verification notes.

PocketSat Mk1 is now considered complete.

## Current status

| Milestone                                             |   Status |
| ----------------------------------------------------- | -------: |
| Mk0 Arduino/trainer bring-up                          | Complete |
| Mk1 STM32/Nucleo avionics baseline                    | Complete |
| Mk2 stronger avionics architecture and ground tooling |  Planned |

Mk1 proves the core architecture on real hardware. Future work should happen under Mk2 unless it is a bugfix for the Mk1 baseline.

## Mk1 summary

PocketSat Mk1 runs on an STM32 Nucleo-F401RE using STM32Cube/HAL through PlatformIO.

The Mk1 system provides:

* explicit modes: `BOOT`, `SAFE`, `NOMINAL`, `PAYLOAD`;
* explicit faults: `NONE`, `SENSOR_MISSING`, `OVERTEMP`, `COMMAND_STORM`, `APP_STALL`;
* line-based ASCII command interface over USART2/ST-LINK VCOM;
* structured ASCII telemetry and event output;
* binary HEALTH telemetry baseline;
* host-side binary packet decoder;
* MPU6050/GY-521 health check, init, live sample, and cached IMU sample path;
* LCD1602 local status/IMU display;
* interrupt-backed UART RX ring buffer;
* health manager with task-age diagnostics;
* independent watchdog gated by health-manager liveness;
* reset-cause reporting after watchdog reset;
* ECSS-inspired requirements, verification, coding-standard, packet, and closure documentation.

Mk1 is not a formal ECSS, MISRA, or flight-qualified system. It is a learning project that deliberately borrows good engineering habits from those environments.

## Hardware

### Target board

| Item         | Value                               |
| ------------ | ----------------------------------- |
| Board        | STM32 Nucleo-F401RE                 |
| MCU          | STM32F401RE                         |
| Framework    | STM32Cube/HAL                       |
| Build system | PlatformIO                          |
| Serial link  | USART2 via ST-LINK virtual COM port |
| Baud rate    | 115200                              |

### Connected hardware

| Hardware         | Interface      | Purpose                        |
| ---------------- | -------------- | ------------------------------ |
| GY-521 / MPU6050 | I2C1           | Sensor health and IMU sampling |
| LCD1602          | 4-bit parallel | Local mode/fault/IMU display   |
| LD2 / PA5        | GPIO           | Heartbeat LED                  |
| ST-LINK VCOM     | USART2         | Commands and telemetry         |

## Repository layout

```text
.
├── docs
│   ├── architecture.md
│   ├── coding-standard.md
│   ├── command-reference.md
│   ├── mk1-closure.md
│   ├── mk2-roadmap.md
│   ├── packet-spec.md
│   ├── release-notes-mk1.md
│   ├── repo-hygiene.md
│   ├── requirements.md
│   └── test-report.md
├── firmware
│   ├── arduino_trainer
│   └── Mk1
│       ├── docs
│       │   ├── protocol.md
│       │   └── verification.md
│       ├── include
│       ├── scripts
│       │   ├── decode_packets.py
│       │   └── export-context.sh
│       ├── src
│       └── test
├── ground
│   └── cli
├── tests-host
└── README.md
```

## Mk1 firmware architecture

The Mk1 firmware is split by responsibility:

| Module                  | Responsibility                                                       |
| ----------------------- | -------------------------------------------------------------------- |
| `main.c`                | Startup and infinite loop glue                                       |
| `app.c`                 | High-level periodic mission scheduling                               |
| `board_nucleo_f401re.c` | STM32 HAL handles and board-specific hardware access                 |
| `command.c`             | UART command parsing and command dispatch                            |
| `telemetry.c`           | ASCII telemetry, events, ACK/DENY/REJECT, binary telemetry selection |
| `packet.c`              | Binary packet encoding and CRC                                       |
| `fdir.c`                | Fault state and safe-mode enforcement                                |
| `health_manager.c`      | Liveness tracking and watchdog gate decision                         |
| `mode.c`                | Legal mode transitions                                               |
| `sensor_service.c`      | Sensor policy, cache, and fault response                             |
| `mpu6050.c`             | Raw MPU6050 register access                                          |
| `lcd1602.c`             | LCD1602 driver and display pages                                     |

Design rule:

> Low-level drivers return facts. High-level services decide what those facts mean.

## Build, flash, and monitor

From the repository root:

```bash
pio run -d firmware/Mk1
```

Upload to the Nucleo:

```bash
pio run -d firmware/Mk1 --target upload
```

Open the serial monitor:

```bash
pio device monitor -d firmware/Mk1 --baud 115200 --echo
```

If your shell or PlatformIO version does not support `-d` for monitor, change into the Mk1 firmware directory first:

```bash
cd firmware/Mk1
pio device monitor --baud 115200 --echo
```

## Command interface

Commands are ASCII lines sent over USART2/ST-LINK VCOM at 115200 baud. Newline terminates a command. Carriage returns are ignored.

Every command should produce one of:

* `ACK` for accepted commands;
* `DENY` for understood commands rejected by safety policy or current state;
* `REJECT` for malformed, unknown, unsafe, or impossible requests;
* documented telemetry for query commands.

Common commands:

```text
PING
GET_STATUS
GET_HEALTH
CHECK_SENSOR
GET_SENSOR_STATUS
INIT_SENSOR
SAMPLE_IMU
GET_IMU
SET_MODE SAFE
SET_MODE NOMINAL
SET_MODE PAYLOAD
INJECT_FAULT SENSOR_MISSING
INJECT_FAULT OVERTEMP
INJECT_FAULT COMMAND_STORM
CLEAR_FAULTS
TEST_WATCHDOG ARM
TEST_WATCHDOG TRIGGER
SET_TLM_FORMAT ASCII
SET_TLM_FORMAT BINARY
DEBUG_I2C
```

See [`docs/command-reference.md`](docs/command-reference.md) for the full command reference.

## Telemetry

Mk1 keeps ASCII telemetry as the default operator/debug format.

Example telemetry classes include:

```text
TLM
EVENT
ACK
DENY
REJECT
```

Mk1 also supports a binary HEALTH telemetry baseline. Binary mode is enabled with:

```text
SET_TLM_FORMAT BINARY
```

and disabled with:

```text
SET_TLM_FORMAT ASCII
```

In Mk1, binary mode affects periodic HEALTH telemetry only. Commands, command responses, events, rejects, denies, and ad-hoc query responses remain ASCII so the board stays easy to operate during bring-up.

Decode captured binary telemetry with:

```bash
python3 firmware/Mk1/scripts/decode_packets.py capture.bin
```

See [`docs/packet-spec.md`](docs/packet-spec.md) and [`firmware/Mk1/docs/protocol.md`](firmware/Mk1/docs/protocol.md).

## Watchdog and safety behaviour

Mk1 uses the STM32 independent watchdog when enabled in firmware configuration.

The watchdog is not kicked unconditionally. Instead, `main.c` refreshes the watchdog only when the health manager considers the app alive.

The health policy includes:

* app tick freshness;
* startup grace period;
* health task freshness;
* sensor task freshness;
* mode-dependent IMU task freshness.

A deliberate watchdog test path is available:

```text
TEST_WATCHDOG ARM
TEST_WATCHDOG TRIGGER
```

Expected behaviour:

1. The test is armed in `SAFE`.
2. The trigger intentionally stalls the application.
3. The independent watchdog resets the MCU.
4. Boot telemetry reports `reset_cause=IWDG`.
5. The system returns to `SAFE`.

## Verification

Mk1 includes a lightweight verification trail:

* [`docs/requirements.md`](docs/requirements.md) defines Mk1 requirements.
* [`docs/test-report.md`](docs/test-report.md) records the Mk1 bench-verification campaign.
* [`firmware/Mk1/docs/verification.md`](firmware/Mk1/docs/verification.md) contains firmware-local verification notes.
* [`docs/mk1-closure.md`](docs/mk1-closure.md) defines the Mk1 completion criteria.

Release smoke test:

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

* every command produces `ACK`, `DENY`, `REJECT`, or documented telemetry;
* active faults prevent unsafe mode transitions;
* `GET_HEALTH` reports task ages and UART RX diagnostics;
* binary telemetry can be entered and exited without losing ASCII command authority;
* watchdog reset test reports `reset_cause=IWDG`.

## Mk1 definition of done

PocketSat Mk1 is complete when the firmware can be built from a clean checkout, flashed to a Nucleo-F401RE, boot into `SAFE`, accept documented commands, report health and sensor status, detect and recover from basic faults, reset under watchdog supervision, report watchdog reset cause on reboot, emit at least one documented binary telemetry packet, and pass the verification procedures listed in `docs/test-report.md`.

Mk1 is considered complete as of the `v0.5.0-mk1-complete` baseline.

## Known Mk1 limitations

Mk1 deliberately stops short of:

* binary command uplink;
* DMA UART TX/RX;
* RTOS scheduling;
* persistent event/fault storage;
* boot counters stored in flash or backup registers;
* full ground-station UI;
* custom PCB;
* formal ECSS/MISRA compliance;
* autonomous payload mission logic.

These are Mk2 candidates.

## Mk2 direction

Mk2 should focus on:

### A — stronger avionics architecture

Likely work:

* table-driven cooperative scheduler;
* event ring buffer;
* persistent boot/reset counters;
* stronger fault taxonomy;
* clearer mode-state-machine tests;
* non-blocking telemetry backend.

### B — better telemetry, protocol, and ground tooling

Likely work:

* binary `STATUS`, `IMU`, `EVENT`, and `SENSOR` packets;
* protocol decoder tests;
* live ground CLI;
* JSONL telemetry logging;
* packet sync/CRC/error test cases;
* optional binary command uplink only after telemetry is mature.

### C — payload/sensors, only if already available

Possible additions:

* second environmental sensor;
* light sensor or photodiode;
* magnetometer;
* simple payload-mode experiment;
* simulated payload if no extra hardware is available.

Mk2 should not expand hardware just for the sake of adding hardware. The main value is architecture and tooling.

See [`docs/mk2-roadmap.md`](docs/mk2-roadmap.md).

## Release hygiene

Before tagging or sharing a release:

```bash
pio run -d firmware/Mk1
python3 firmware/Mk1/scripts/decode_packets.py --help
git status
```

Do not commit generated PlatformIO output:

```text
.pio/
```

Context exports such as `pocketsat_context.md` are useful for reviews, but should normally not be included in release commits unless intentionally archived.

Suggested tag:

```bash
git tag v0.5.0-mk1-complete
```

## Project philosophy

PocketSat Mk1 is intentionally small.

The point is not to simulate an entire spacecraft. The point is to practise building embedded software with the habits that make larger systems survivable:

* explicit state;
* explicit faults;
* bounded inputs;
* documented interfaces;
* observable health;
* watchdog supervision;
* verification evidence;
* clean release boundaries.
  ::: 

