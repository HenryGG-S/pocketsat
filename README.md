# PocketSat Trainer Mk0
## Purpose
Arduino-based rehearsal rig for embedded space systems foundations.
## Hardware
- UNO R3 compatible board
- LED/button/GY-521/DHT11 as configured in docs/wiring.md
## Build
arduino-cli compile --fqbn arduino:avr:uno firmware/arduino_trainer
## Upload
arduino-cli upload -p <PORT> --fqbn arduino:avr:uno firmware/arduino_trainer
## Demo sequence
1. Boot telemetry
3. SET_MODE NOMINAL
4. SAFE mode entry

# PocketSat Mk1
## Purpose

## Hardware

## Build

## Upload

## Sequence

