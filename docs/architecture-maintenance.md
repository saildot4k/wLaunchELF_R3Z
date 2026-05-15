# wLaunchELF Maintainer Notes

This project is now organized around two maintenance goals:

1. Keep runtime behavior stable on real PS2 hardware.
2. Make feature combinations predictable in CI and local builds.

## Build Profiles

High-level build profile resolution lives in:

- `scripts/ci/resolve_make_args.sh`

It maps profile selectors:

- `psx_profile`: `no-psx` or `psx`
- `pad_profile`: `no-ds34` or `ds34`
- `storage_profile`: `usb`, `mmce`, `mx4sio`, `all`, `minimal`

into concrete make flags (e.g. `EXFAT`, `MMCE`, `MX4SIO`, `DVRP`, `XFROM`, `DS34`).

This keeps CI and local profile builds aligned.

## CI Matrix

Workflow: `.github/workflows/compile.yml`

- Release variants use the `all` storage profile and keep the historical four profile combinations.
- Extra matrix entries (`USB-ONLY`, `MMCE-ONLY`, `MX4SIO-ONLY`, `MINIMAL`) are coverage builds for overlap and dependency regressions.

## Stale Code Audit

Script: `scripts/stale_code_audit.sh`

Current checks:

- stale markers (`TODO`/`FIXME`/`HACK`, legacy aliases)
- disabled `#if 0` blocks
- duplicate config-probe hotspots
- largest source files (refactor candidates)

Use:

- `make stale-audit`

CI uploads `stale-code-report.txt` as an artifact.

## Recommended Future Splits

Most of the code is already modularized, but `src/main.c` and `src/filer.c` are still large.

Completed so far:

1. `src/main_fileops.c` now owns file/path support helpers (`wleExists`, `uLE_related`, file-type checks).
2. `src/main_startup.c` now owns startup/system parsing helpers (`getIpConfig`, `readSystemCnf`).

Recommended extraction order (lowest risk first):

1. `main_boot.c` (boot path normalize + startup config probing)
2. `main_modules.c` (IRX/module loading + stack switching)
3. `main_actions.c` (execute/launch command handlers)
4. `filer_menu.c` (R1 menu enable/dispatch logic)
5. `filer_paths.c` (device/path mapping helpers)

Keep each split behavior-neutral and verify on both PCSX2 and real hardware after each pass.
