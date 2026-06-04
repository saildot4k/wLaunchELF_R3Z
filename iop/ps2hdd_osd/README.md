# Local ps2hdd-osd

This directory vendors the PS2SDK APA/ps2hdd sources needed to build `ps2hdd-osd.irx`.

We keep a local OSD build because wLaunchELF needs protected-partition access, and the
stock PS2SDK APA open-slot tracking treats partitions with the same APA ID as the
same open partition even when they are on different HDD units.

Local change:

- Track the HDD unit in each open file slot.
- Treat a partition as already open only when both the HDD unit and APA ID match.

`embed.make` builds this module and embeds it as `ps2hdd_irx`; `ps2fs.irx` still comes
from the active PS2SDK/latest ps2dev container.
