# PocketSat Mk2 Roadmap

Mk2 should build on the Mk1 safety and protocol baseline. The realistic focus is both:

- **A — stronger avionics architecture**;
- **B — better telemetry/protocol/ground tooling**.

Payload/sensor expansion is optional and should only be pulled in if components are already available and do not distract from A/B.

## Mk2 theme

Mk2 should answer:

> Can PocketSat behave like a small, testable spacecraft avionics stack rather than a single-board firmware demo?

## Track A — stronger avionics architecture

### A1. Formal app scheduler

Replace ad-hoc periodic checks with a small cooperative scheduler table:

| Task | Period | Deadline | Health marker |
|---|---:|---:|---|
| Health | 1000 ms | 2500 ms | Required |
| Sensor check | 3000 ms | 8000 ms | Required |
| IMU sample | 1000 ms in NOMINAL/PAYLOAD | 3000 ms | Mode-dependent |
| Telemetry service | TBD | TBD | Required if non-blocking TX added |

### A2. Event ring buffer

Add a fixed-size RAM event log:

```text
GET_EVENTS
CLEAR_EVENTS
```

Events should include boot, reset cause, mode transitions, faults raised/cleared, watchdog test arm/trigger, command storm, sensor recovery, and telemetry format changes.

### A3. Persistent boot/reset counters

Add flash-backed or backup-register-backed counters:

- total boots;
- watchdog resets;
- software resets;
- last reset cause;
- last critical event ID.

This would make watchdog/reset evidence survive reboot beyond immediate telemetry.

### A4. Stronger state-machine definition

Move mode transition rules into a table or explicitly documented state machine.

Add tests for:

- all allowed transitions;
- all denied transitions;
- fault-forced SAFE behaviour;
- post-watchdog SAFE behaviour.

### A5. Non-blocking telemetry backend

Move from blocking UART TX to a TX ring buffer with interrupt or DMA drain.

Do this after binary telemetry is stable, not before.

### A6. Fault taxonomy

Refine faults into:

- active faults;
- historical reset causes;
- warnings;
- latched faults;
- clearable vs non-clearable faults.

Mk1 intentionally keeps this simple.

## Track B — telemetry, protocol, and ground tooling

### B1. Expand binary packets

Add packet types in this order:

1. `STATUS`
2. `IMU`
3. `EVENT`
4. `SENSOR`
5. `ACK`/`REJECT`/`DENY` only if useful

Keep ASCII command operation available until binary command uplink is justified.

### B2. Decoder tests

Add host tests for:

- valid HEALTH packet;
- CRC failure;
- sync recovery;
- truncated frame;
- unknown packet type;
- future version rejection.

### B3. Ground CLI

Build a small ground CLI that can:

- open serial;
- send ASCII commands;
- decode binary packets live;
- log raw bytes;
- export JSONL telemetry;
- run verification scripts.

### B4. Protocol versioning policy

Define:

- how packet versions change;
- how payload sizes change;
- how ground tooling handles unknown packet types;
- how test vectors are stored.

### B5. Optional binary command uplink

Only after binary telemetry is mature:

- command frame format;
- command sequence IDs;
- CRC;
- ACK/REJECT correlation;
- replay/duplicate handling;
- safe command whitelist.

## Track C — payload/sensors, only if already available

Possible additions:

- second environmental sensor;
- light sensor / photodiode;
- magnetometer;
- simple payload mode experiment;
- simulated payload if no hardware is available.

Do not buy hardware just to expand Mk2. Mk2’s value is architecture and tooling.

## Suggested Mk2 milestones

| Milestone | Theme | Completion criterion |
|---|---|---|
| Mk2.0 | Scheduler and event log | Periodic tasks table-driven; recent events queryable. |
| Mk2.1 | Binary telemetry expansion | STATUS and IMU packets decoded live. |
| Mk2.2 | Ground CLI | Commands and packet decoding in one tool. |
| Mk2.3 | Persistent reset evidence | Boot/watchdog counters survive reset. |
| Mk2.4 | Non-blocking TX | Telemetry no longer blocks app loop. |
| Mk2.5 | Optional payload | Payload mode performs one documented experiment. |

## Mk2 non-goals

- full RTOS unless scheduler complexity justifies it;
- custom PCB before software architecture stabilises;
- binary command uplink before telemetry and ground tooling are reliable;
- formal certification claims.

