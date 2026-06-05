# Board bring-up log

## Date
2026-06-05

## Board
ELEGOO UNO R3 compatible board

## Port
/dev/ttyACM0

## Build/upload command
arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:uno firmware/arduino_trainer

## Monitor command
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200

## Observed output
EVENT,BOOT
FW,NAME=arduino-trainer-mk0,VERSION=0.1.0
BOARD,TYPE=UNO_R3
MODE,current=BOOT
TLM,HEALTH,tick=1000,counter=1
...

## Notes
Opening the serial monitor can reset the UNO due to DTR/RTS.
Occasional initial garbage bytes or partial lines are expected when attaching mid-stream.
