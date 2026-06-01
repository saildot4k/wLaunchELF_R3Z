# wLaunchELF VMCMAN

This directory vendors the PS2SDK `mcman`/`vmcman` source used to build the
embedded `vmcman.irx` module for wLaunchELF.

Base snapshot:

- Repository: `ps2dev/ps2sdk`
- Commit: `ff7c5f93763d90c8b10bc97fbce5d25939ed94fb`
- Source path: `iop/memorycard/mcman`

wLaunchELF keeps this as a local module because VMC support needs project
specific fixes and diagnostics. Building from a live PS2SDK source checkout and
patching it during every build made behavior dependent on whichever PS2SDK
revision happened to be in the container.

Local VMC changes include:

- Correct VMC backing geometry validation for no-ECC and ECC images.
- Preserve mounted VMC state so normal file operations do not keep re-probing
  physical memory card hardware.
- Add targeted diagnostics around mount, mkdir, create-file, and backing
  read/write paths.
- Register a startup marker: `vmcman: wLaunchELF patched VMC build active`.
