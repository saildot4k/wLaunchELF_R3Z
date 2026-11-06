# UDPFS SMAP driver

Vendored from this project's supplied `nhddl-legacy/iop/udpfs/smap` snapshot of
[Neutrino's SMAP driver](https://github.com/ps2max32/neutrino/tree/06188cce7cd07d2a158cb4b2646c5719fc16a565/iop/smap).
`include/mprintf.h` comes from the snapshot's `iop/udpfs/common` directory.
The original AFL-3.0 license is retained in `LICENSE`.

Repairs follow Launchstation commits
[`6947dd7`](https://github.com/saildot4k/Launchstation/commit/6947dd7ab4de100d071e6905de2d1e8959a56241)
and [`5d8515c`](https://github.com/saildot4k/Launchstation/commit/5d8515cae2d4810eb91c95a50ddd14a8cda507df):

- `src/xfer.c`: receive the final padded FIFO word and copy only its valid bytes.
  The original reader omitted the final byte of odd-length reads while reporting
  the expected length. File sizes could be correct with a stale final byte.
- `src/smap.c`: preserve a bounded PHY reset wait instead of failing on the
  first busy response, and test the SMAP capability bit with correct precedence.
  These inherited startup defects must not accompany the rebuilt driver.
- `src/exports.tab`: declare `_retonly` before the table but define it after the
  table for PS2SDK iopfixup compatibility. Export name `smap`, version 1.0 and
  function ordinals 4–7 are unchanged.
- Standalone PS2SDK Makefile with header/table dependencies; the root build
  supplies an absolute output filename to ensure the generated IRX is embedded.

`embed.make` always builds this tracked driver. It does not use the ignored
`thirdparty/` copy or fall back to the historical precompiled SMAP. The existing
ministack and UDPFS IOMAN modules retain their current build/fallback paths.
The historical SMAP's SHA-256 is
`4b81d9bf0c176d3291258eb97f6188a2bb935a69ae4cba13546b51d603ee1a09`.

Run `python3 tests/test_udpfs_smap_fifo_tail.py` and
`python3 tests/test_udpfs_smap_startup.py` for host simulations of the production
functions, including negative controls reproducing the original defects.
`CC` selects a host C compiler. These tests do not emulate a physical console.

After building, run:

```sh
python3 scripts/check_udpfs_smap.py iop/__generated/udpfs_smap.irx UNC-BOOT*.ELF
```

The check verifies the SMAP export ABI, peer imports, and that each ELF embeds
the new module rather than the defective historical module. Hardware acceptance
still requires odd/even UDPFS-to-USB and UDPFS-to-card copies, byte/hash checks
after reading them back, and reconnecting after a server restart.
