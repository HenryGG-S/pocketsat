# PocketSat Mk1 Closure

Mk1 is the first complete PocketSat embedded-avionics learning milestone. It is intentionally small, but it is structured and verified like a serious embedded system.

## Definition of done

PocketSat Mk1 is complete when the firmware can be built from a clean checkout, flashed to a Nucleo-F401RE, boot into SAFE, accept documented commands, report health and sensor status, detect and recover from basic faults, reset under watchdog supervision, report watchdog reset cause on reboot, emit at least one documented binary telemetry packet, and pass the verification procedures listed in `docs/test-report.md`.

## Closure status

| Area | Status |
|---|---:|
| Modular firmware architecture | Complete |
| USART2 command interface | Complete |
| Interrupt-driven UART RX ring | Complete |
| ASCII telemetry | Complete |
| Binary HEALTH telemetry baseline | Complete |
| Health manager | Complete |
| Independent watchdog | Complete |
| Reset-cause reporting | Complete |
| FDIR-owned fault handling | Complete |
| MPU6050 health/init/sample path | Complete |
| LCD1602 status/IMU display | Complete |
| Command reference | Complete |
| Requirements document | Complete |
| Test report | Complete |
| Mk2 roadmap | Complete |

## Release checklist

Before tagging Mk1 complete:

```bash
pio run
python3 firmware/Mk1/scripts/decode_packets.py --help
```

Then check:

```bash
git status
```

Recommended release commit/tag:

```bash
git add docs firmware/Mk1 README.md
git commit -m "docs: close PocketSat Mk1 baseline"
git tag v0.5.0-mk1-complete
```

If the binary telemetry patch file is already applied, remove the patch artifact before release unless you intentionally want to keep it as development history:

```bash
git rm --ignore-unmatch firmware/Mk1/pocketsat_mk1_binary_telemetry.patch
rm -f firmware/Mk1/pocketsat_mk1_binary_telemetry.patch
```

Generated context exports such as `pocketsat_context.md` should normally be excluded from the release commit unless intentionally archived.

## Known limitations

Mk1 deliberately stops short of:

- binary command uplink;
- DMA telemetry;
- RTOS scheduling;
- persistent fault/event storage;
- flash boot counters;
- host ground-station UI;
- custom PCB;
- formal ECSS/MISRA compliance;
- autonomous payload mission logic.

These are Mk2 candidates.

## Engineering value

Mk1 proves the project can support disciplined embedded development:

- state is explicit;
- faults are explicit;
- operator responses are explicit;
- watchdog behaviour is verified;
- telemetry is structured;
- binary protocol work has begun;
- documentation and tests exist alongside code.

