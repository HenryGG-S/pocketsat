# Repository Hygiene

Before closing Mk1, keep the repository clean and reproducible.

## Do not commit generated PlatformIO output

Generated build output should not be tracked:

```text
.pio/
```

If `.pio` is already tracked, remove it from the index:

```bash
git rm -r --cached .pio
```

## Context exports

Files such as `pocketsat_context.md` are useful for review snapshots but should not normally be committed unless deliberately archived.

## Patch artifacts

Once `firmware/Mk1/pocketsat_mk1_binary_telemetry.patch` has been applied, remove it before the release commit unless intentionally preserving it as historical evidence.

## Suggested release commands

```bash
pio run
git status
git add docs firmware/Mk1 README.md
git commit -m "docs: close PocketSat Mk1 baseline"
git tag v0.5.0-mk1-complete
```

