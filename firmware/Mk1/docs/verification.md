# PocketSat Mk1 Verification Notes

This is an informal ECSS-inspired hardware-in-the-loop verification log.

## REQ-MODE-001 — Boot enters SAFE

Procedure:
1. Flash firmware.
2. Open USART2/ST-LINK serial at 115200 baud.
3. Reset board.

Expected:
- Boot event emitted.
- Mode change BOOT -> SAFE emitted.
- LCD shows SAFE / NONE.

Observed:
- Date:
- Firmware version:
- Result:

## REQ-MODE-002 — Active faults prevent leaving SAFE

Procedure:
1. Send `INJECT_FAULT OVERTEMP`.
2. Send `SET_MODE NOMINAL`.

Expected:
- OVERTEMP fault raised.
- Board remains or enters SAFE.
- SET_MODE NOMINAL returns DENY with reason ACTIVE_FAULT.

Observed:
- Date:
- Firmware version:
- Result:

## REQ-FDIR-003 — Command storm detection

Procedure:
1. Send more than 8 complete command lines within 1 second.

Expected:
- REJECT reason COMMAND_STORM.
- COMMAND_STORM fault raised.
- Board enters or remains SAFE.

Observed:
- Date:
- Firmware version:
- Result:

## REQ-FDIR-004 — App liveness fault enters SAFE

Procedure:
1. Artificially prevent a required periodic task from updating its health marker.
2. Continue running the main loop.

Expected:
- FAULT_APP_STALL is raised.
- Mode changes to SAFE if not already SAFE.
- Telemetry reports fault=APP_STALL.

Observed:
- Date:
- Firmware version:
- Result:

## REQ-FDIR-005 — Independent watchdog resets stalled application

Procedure:
1. Flash firmware with ENABLE_IWDG=1.
2. Boot system and confirm stable SAFE mode operation.
3. Send `TEST_WATCHDOG ARM`.
4. Send `TEST_WATCHDOG TRIGGER`.

Expected:
- System acknowledges both commands.
- Application intentionally stalls.
- Independent watchdog resets MCU.
- Boot telemetry reports reset_cause=IWDG.
- System returns to SAFE.

Observed:
- Date: 2026-06-19
- Firmware version: 0.5.0
- Result: PASS
- Notes:

## REQ-SNS-006 — Cached IMU behaviour

Procedure:
1. Boot system.
2. Send `GET_IMU` before any valid sample.
3. Send `INIT_SENSOR`.
4. Send `SET_MODE NOMINAL`.
5. Wait for an IMU sample or send `SAMPLE_IMU`.
6. Send `GET_IMU`.

Expected:
- Early GET_IMU rejects with NO_VALID_IMU_SAMPLE.
- Later GET_IMU reports IMU_CACHED with age_ms.

Observed:
- Date:
- Firmware version:
- Result:
