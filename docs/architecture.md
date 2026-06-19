# Architecture

## Mk0 Architecture

USB Serial Ground Link
|
v
Command Parser -> Command Router -> Mode Manager
|                   |                   |
|                   |                   +--> SAFE / NOMINAL / PAYLOAD
|                   |
|                   +--> Fault Injector
|
+--> Telemetry Output

Periodic Loop / Tiny Scheduler
|-> heartbeat task
|-> sensor sample task
|-> health monitor task
|-> telemetry task

Sensor Service
|-> GY-521 / DHT11 / thermistor
|-> health: OK / STALE / MISSING / OUT_OF_RANGE

## Mk1 Architecture

main.c              owns startup and the infinite loop
app.c               owns high-level mission scheduling
command.c           owns UART command parsing/execution
telemetry.c         owns ACK/DENY/REJECT/TLM/EVENT formatting
fdir.c              owns faults and safe-mode enforcement
mode.c              owns legal mode transitions
sensor_service.c    owns sensor policy, cache, and fault response
mpu6050.c           owns raw MPU6050 I2C access
lcd1602.c           owns LCD driving and display pages
board_*.c           owns HAL handles and board-specific hardware plumbing

Low-level drivers return facts.
High-level services decide what those facts mean.

main
 ├─ board
 ├─ app
 └─ command

app
 ├─ telemetry
 ├─ mode
 ├─ fdir
 ├─ sensor_service
 └─ lcd1602

command
 ├─ telemetry
 ├─ mode
 ├─ fdir
 ├─ sensor_service
 ├─ lcd1602
 └─ board

sensor_service
 ├─ mpu6050
 ├─ telemetry
 ├─ fdir
 └─ lcd1602

mpu6050
 └─ board

lcd1602
 └─ board or HAL directly

telemetry
 └─ board

fdir
 ├─ telemetry
 ├─ mode
 └─ lcd1602

mode
 ├─ telemetry
 └─ lcd1602
