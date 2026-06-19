# PocketSat Mk1 Command Reference

Commands are ASCII lines sent over USART2/ST-LINK VCOM at 115200 baud. Commands are terminated by newline. Carriage returns are ignored.

Every operator command shall produce one of:

- `ACK` for accepted commands;
- `DENY` for understood commands rejected by current state or safety policy;
- `REJECT` for malformed, unknown, unsafe, or impossible requests;
- documented telemetry output for query commands.

## Core commands

| Command | Purpose | Expected response |
|---|---|---|
| `PING` | Link sanity check. | `ACK` |
| `GET_STATUS` | Report current mode/fault. | `TLM,type=STATUS` |
| `GET_HEALTH` | Report health-manager task ages and UART RX diagnostics. | `TLM,type=HEALTH_DETAIL` |
| `CLEAR_FAULTS` | Clear active fault through operator command. | `ACK` plus event |

## Sensor commands

| Command | Purpose | Expected response |
|---|---|---|
| `CHECK_SENSOR` | Read MPU6050 `WHO_AM_I`. | `TLM,type=SENSOR` |
| `GET_SENSOR_STATUS` | Report cached sensor health. | `TLM,type=SENSOR_STATUS_CACHED` or `REJECT` |
| `INIT_SENSOR` | Verify and wake/configure MPU6050. | `ACK` or `REJECT` |
| `SAMPLE_IMU` | Perform live IMU burst sample. | `TLM,type=IMU` or `REJECT` |
| `GET_IMU` | Report latest cached IMU sample. | `TLM,type=IMU_CACHED` or `REJECT` |

## Mode commands

| Command | Purpose |
|---|---|
| `SET_MODE SAFE` | Enter SAFE. Always allowed. |
| `SET_MODE NOMINAL` | Enter NOMINAL if no active fault and transition is legal. |
| `SET_MODE PAYLOAD` | Enter PAYLOAD if no active fault and transition is legal. |

Denied transitions shall return `DENY`, not silently fail.

## Fault-injection commands

| Command | Purpose |
|---|---|
| `INJECT_FAULT SENSOR_MISSING` | Raise sensor-missing fault through FDIR. |
| `INJECT_FAULT OVERTEMP` | Raise over-temperature fault through FDIR. |
| `INJECT_FAULT COMMAND_STORM` | Raise command-storm fault through FDIR. |

Fault injection is a bench-verification feature and should be treated as a test interface.

## Watchdog test commands

| Command | Purpose | Safety rule |
|---|---|---|
| `TEST_WATCHDOG ARM` | Arm deliberate watchdog reset test. | Should only be accepted in SAFE. |
| `TEST_WATCHDOG TRIGGER` | Intentionally stall the app and allow IWDG reset. | Requires prior arm. |

The trigger command intentionally does not return to normal control flow. The independent watchdog is expected to reset the MCU.

## Telemetry format commands

| Command | Purpose |
|---|---|
| `SET_TLM_FORMAT ASCII` | Use ASCII periodic health telemetry. |
| `SET_TLM_FORMAT BINARY` | Use binary HEALTH packets for periodic health telemetry. |

Mk1 keeps the command/response path ASCII even when binary HEALTH telemetry is enabled.

## Diagnostics command

| Command | Purpose |
|---|---|
| `DEBUG_I2C` | Report I2C error register and SCL/SDA pin states. |

## Invalid command behaviour

Unknown input should be rejected with the sanitised received input included.

Example:

```text
PNG
```

Expected:

```text
REJECT,...reason=UNKNOWN_COMMAND,input=PNG
```

Commas and non-printable characters in the reported input may be sanitised to preserve telemetry field structure.

