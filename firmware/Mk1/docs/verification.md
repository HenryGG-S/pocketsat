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

## REQ-SNS-004/005 — Cached IMU behaviour

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
