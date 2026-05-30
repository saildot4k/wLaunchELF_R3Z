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
3. Legacy SMB and JPG subsystems were removed from runtime/build paths (`src/main.c`, `Makefile`, `embed.make`).
4. `src/filer.c` helper splits now include:
   - `src/gui_sort.c` (`cmpFile`, `sort`)
   - `src/gui_texteditor.c` (`canOpenInTextEditor`)
   - `src/gui_hdd0_format.c` (`getHddParty`, `getHddDVRPParty`)
   - `src/psu_functions.c` (`clear_psu_header`, `pad_psu_header`)
   - `src/gui.c` (`ynDialog`, `nonDialog`, `keyboard`)
   - `src/gui_colors.c` (`scanSkinCNF`, `storeSkinCNF`)
5. PSU data layout structs are centralized in `include/psu_types.h` to avoid duplicated type blocks in `filer.c`.
6. `src/main.c` helper splits now include:
   - `src/main_info_screens.c` (`Show_About_uLE`, `Show_build_info`, `ShowDebugInfo`, `ShowFont`)
   - `src/main_gameid.c` (RetroGem/game-ID helpers)
   - `src/main_actions.c` (`Execute` action dispatch + launch cleanup)
7. Main startup/menu flow splits now include:
   - `src/main_boot.c` (early boot sequence + argument capture)
   - `src/main_modules.c` (runtime module/font/pad startup + CNF path validation)
   - `src/main_menu.c` (main menu rendering/input/timer state helpers)

Recommended extraction order (lowest risk first):

1. `filer_menu.c` (R1 menu enable/dispatch logic)
2. `filer_paths.c` (device/path mapping helpers)
3. `main_loop.c` (disc polling + event loop orchestration)

Keep each split behavior-neutral and verify on both PCSX2 and real hardware after each pass.
