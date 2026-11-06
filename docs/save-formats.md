# Save imports and backups

Copy a save archive, browse to the root of the matching memory card, then choose
**R1 → Extract <extension>**, such as **Extract PSU**, **Extract PSV** or
**Extract MCS**. The extension is shown in uppercase; mixed formats and
extensionless saves use **Extract saves**. PS1 saves require a PS1 card;
PS2 saves require a PS2 card.
The same restriction applies to mounted virtual cards and direct copies between
cards. PS1 cards have a flat directory with a 20-byte save filename limit.

| System | Import formats | Individual backup formats |
| --- | --- | --- |
| PS2 | PSU, PSV, MAX, CBS, XPS, SPS | PSU, signed PSV |
| PS1 | MCS, Action Replay PSX, raw SC save, PS1 PSV | MCS, PSX, raw, signed PSV |

The existing PSU importer/exporter is retained. New archive importers validate
all entries, names, lengths and offsets before writing, and refuse to overwrite
existing saves. Remove an existing save explicitly before replacing it. Failed
new PS2 imports remove their partial output. Backup/import conversions keep their
clipboard sources, including sources selected with Cut.

To back up a card save, copy its directory (PS2) or file (PS1), browse to a storage
directory, and choose **R1 → Create archive**. A second popup selects the format.
PS2 offers **Create PSU (PS2)** and **Create PSV (PS3)**. PS1 offers individual
save formats and whole-card containers. A PS1 whole-card backup contains all
selected saves and must fit in 15 blocks. The VMC choices create these exact names:

| Backup option | Filename |
| --- | --- |
| Create VMC (Ember - Card 1) | `MC1.vmc` |
| Create VMC (Ember - Card 2) | `MC2.vmc` |
| Create VMC (PopStarter - Card 1) | `SLOT0.VMC` |
| Create VMC (PopStarter - Card 2) | `SLOT1.VMC` |

These presets save directly in the current destination directory without a
filename prompt. Other card containers use a filename prompt. Existing backup
files are preserved. The PSU path keeps its existing naming and
overwrite settings. New menu strings currently use English fallbacks in the
bundled non-English language files.

## PS1 virtual cards

Use **Mount vmc0:** or **Mount vmc1:** on a PS1 image to browse, copy, import,
rename, delete or back up its saves. Supported containers are:

| Container | Size | Typical use |
| --- | ---: | --- |
| Raw VMC, MCR, MCD, BIN or VM1 | 131,072 bytes | Ember, POPStarter, emulators, PS3 VM1 |
| GME | 134,976 bytes | DexDrive |
| VGS | 131,136 bytes | Virtual Game Station |
| VMP | 131,200 bytes | PSP / Vita |
| MCX | 131,232 bytes | PocketStation / Vita |

Raw PS1 cards have an `MC` header and a valid directory/block chain. Size alone
does not identify a PS1 card. Raw image extensions do not affect detection, so
existing Ember and POPStarter filenames work. VMP signatures and MCX encryption
are regenerated after editing. GME comments and existing container headers are
retained where applicable. Copy a card-image file and choose **Create archive**
to convert its container. PS2 images continue through the existing IOP VMC driver.

PS1 virtual-card edits are staged in memory. Saving creates `image.wle-new`,
renames the original to `image.wle-old`, publishes the new image, and removes the
old backup. Cancellation or a failed replacement preserves the original. After
a power interruption, recover the original from `.wle-old` if necessary; leftover
transaction files prevent further edits until resolved. Images changed outside
the current mount must be mounted again before editing.

## Validation

Run `python3 tests/run_save_tests.py` on a host with a C compiler, Python 3.9+
and zlib development headers/library. `CC`, `CFLAGS` and `SAVE_TEST_LIBS` can
select an alternate compiler and library location.

Tests cover every new save/card output format, MAX/CBS/XPS decoding, bounds and
filename rejection, fragmented PS1 saves, full cards, duplicate names, wrong-card
rejection before writes, partial-import cleanup, and failed/cancelled VMC edits.
OpenSSL, when present, supplies an independent AES reference for PSV signatures;
Python supplies independent SHA-1/HMAC and SHA-256 references. Real PS1/PS2 cards
and PS3/PSP/emulator interoperability still need hardware testing.

## Credits

Format layouts, CBS cipher state and PSV/VMP signing are based on the supplied
[Apollo Save Tool source](../thirdparty/apollo-ps2-main/), by Damian Parrino
(Bucanero), with contributions by dots_tb, root670 and Shendo (MemcardRex).
Apollo-derived support is distributed under GPL-3.0-or-later; see
[the included license](../licenses/Apollo-GPL-3.0.txt). The LZARI implementation
retains Haruhiko Okumura's original permission notice and the Apollo source's
Luigi Auriemma / Wesley Castro credits in `src/save_lzari.c`.
