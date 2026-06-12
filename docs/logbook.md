# Bring-up Log

## Date
11-06-2026

## Board
NUCLEO-F401RE

## Toolchain

## Build command
pio run

## Flash command
pio run --target upload

## Monitor command
pio device monitor --baud 115200 --echo

## Observed output
### lsusb
Bus 001 Device 013: ID 0483:374b STMicroelectronics ST-LINK/V2.1
### tty device
/dev/ttyACM0

## Date
12-06-2026

## LED stuck on

Observed:
LED turned on but did not blink.

Hypothesis:
Main loop reached the first GPIO toggle, then blocked inside HAL_Delay.

Cause:
HAL_Delay depends on the HAL millisecond tick. The SysTick interrupt handler was missing or not incrementing the HAL tick.

Fix:
Added SysTick_Handler calling HAL_IncTick.

Lesson:
A blocking delay depends on interrupt/tick infrastructure. GPIO can work while timekeeping is broken.
