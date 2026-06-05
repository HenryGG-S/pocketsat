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

