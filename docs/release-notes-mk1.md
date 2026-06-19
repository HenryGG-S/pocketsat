# PocketSat Mk1 Release Notes

## Release

PocketSat Mk1 complete baseline.

Suggested tag:

```text
v0.5.0-mk1-complete
```

## Highlights

- Modular firmware split by ownership.
- Explicit modes: BOOT, SAFE, NOMINAL, PAYLOAD.
- Explicit faults: NONE, SENSOR_MISSING, OVERTEMP, COMMAND_STORM, APP_STALL.
- FDIR-owned fault raising and clearing.
- Health manager with task liveness tracking.
- Independent watchdog enabled and health-gated.
- Startup grace prevents premature watchdog reset.
- Reset-cause telemetry reports watchdog resets.
- UART RX moved to interrupt-backed ring buffer.
- MPU6050 health, init, live sample, and cached sample paths.
- LCD1602 status/IMU pages using Nucleo D2-D7 mapping.
- ASCII command/response interface retained.
- Binary HEALTH telemetry baseline added.
- Host decoder script added.
- Requirements, test report, command reference, packet spec, and Mk2 roadmap added.

## Verification status

Mk1 is considered complete when `docs/test-report.md` reflects the current bench run and all release smoke tests pass.

## Known limitations

- Blocking UART TX remains.
- Binary telemetry only covers HEALTH.
- No binary command uplink.
- No persistent fault/event log.
- No RTOS.
- No custom PCB.

