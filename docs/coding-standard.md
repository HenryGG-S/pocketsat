# PocketSat Mk1 Coding Standard

This is a lightweight, ECSS-inspired C style guide for Mk1. It is not a formal MISRA or ECSS compliance claim.

## Project rules

1. No dynamic allocation in firmware.
2. No recursion.
3. No unbounded string operations.
4. All `snprintf` destinations shall be fixed-size local or static buffers.
5. Command responses shall be explicit: `ACK`, `DENY`, `REJECT`, or documented telemetry.
6. FDIR owns fault state mutation.
7. Low-level drivers return facts; high-level services decide policy.
8. Hardware-specific HAL handles remain inside the board layer.
9. Watchdog refresh shall be gated by health-manager state, not unconditional loop activity.
10. New external interfaces shall be documented before or alongside implementation.

## Module ownership

| Module | Owns | Must not own |
|---|---|---|
| `main.c` | Startup loop glue | Mission policy |
| `app.c` | Periodic scheduling | Raw HAL handles |
| `board_nucleo_f401re.c` | HAL handles and board I/O | Mission safety policy |
| `command.c` | Command parse/dispatch | Sensor register details |
| `telemetry.c` | ASCII/binary telemetry formatting | Fault policy |
| `fdir.c` | Fault state and recovery policy | Sensor register access |
| `health_manager.c` | Liveness and watchdog gate decision | Command parsing |
| `sensor_service.c` | Sensor policy and cache | Raw I2C transactions |
| `mpu6050.c` | MPU6050 register access | FDIR or LCD updates |
| `lcd1602.c` | LCD driver and display pages | Command parsing |
| `packet.c` | Binary packet encoding/CRC | App scheduling |

## Naming guidance

- Public functions use module prefixes: `board_`, `telemetry_`, `health_manager_`, `mpu6050_`, etc.
- Private functions are `static`.
- Constants use uppercase names with units where relevant: `HEALTH_PERIOD_MS`.
- Requirement IDs in comments should match `docs/requirements.md` where practical.

## Safety notes

- Do not kick the watchdog from interrupts.
- Do not kick the watchdog unconditionally from `main.c`.
- Do not directly assign `app->fault` outside FDIR helpers.
- Do not silently discard operator commands except for defined command-buffer overflow behaviour.
- Do not change binary packet layout without updating `docs/packet-spec.md`, `firmware/Mk1/docs/protocol.md`, and decoder tests.

