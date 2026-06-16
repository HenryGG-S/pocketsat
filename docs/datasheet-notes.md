# GY-521 

## Purpose
First real sensor subsystem for PocketSat Mk1.

## Bus
I2C.

## First objective
Read WHO_AM_I register.

## Health rule
If WHO_AM_I cannot be read or returns an unexpected value, raise SENSOR_MISSING.

## Firmware behaviour
- Emit sensor telemetry when OK.
- Enter SAFE mode when missing.
- Do not crash or block forever if missing.
