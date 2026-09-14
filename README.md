# __wLaunchELF_R3Z__

wLaunchELF, formerly known as uLaunchELF, also known as wLE or uLE (abbreviated), is an open source file manager and executable launcher for the Playstation 2 console based off of the original LaunchELF. It contains many different features, including a text editor, hard drive manager, as well as network support, and much more.

## Supported devices:
| Device | Description | Visibility |
|---|---|---|
| __mc:/__ | Memory Cards | Always |
| __usb:/__ | Fat/exFAT (BDM) USB | Always |
| __mmce:/__[^1] | Multi Purpose Memory Card Emulator IE SD2PSX, PSxMemCard Gen2 or MemCard Pro 2. | Always |
| __mx4sio:/__[^1] | SD card interface over memory card port. | Always |
| __hdd:/__[^2] | APA formatted internal HDD | Hidden on deckard |
| __ata:/__[^2] | BDM hard drive, exFAT for now till more are supported | Hiddon on deckard |
| __xfrom:/__ | PSX DESR-XXXX flash storage | Hidden on non-PSX |
| __dvr_hdd0:/__ | PSX DESR-XXXX digital video recorder hdd partition/side | Hidden on non-PSX |
| __cdfs:/__ | CD/DVD File System | Always |
| __udpfs:/__ | Network interface used with [PCM720s UDPFSD Server](https://github.com/pcm720/udpfsd) | Always |

Drivers load on demand for maximum compatibility and initial boot speed.

[^1]: MMCE and MX4SIO will incure an IOP reboot as the 2 are incompatible.

[^2]: Dual HDD/ATA support is built in for future development.

## Features:
- LoadBOOTer MC Exploit installer, supports Retail PS2/PSX and DEX(DTL-HXXXXX)
- RetroGem Game ID for [PIXEL FX RetroGem](https://www.pixelfx.co/hdmi-retro-gem)
- Launch PS1 VCDs with option for custom POPStarter path, falls back to defaults.
- Writes to history file for disc launches for mmce vmc change
- Applies deckard disc patches
- X/0 applied per region if no config file is found so O is only default for Japan
- Keyboard layout choices: QWERTY, DVORAK, AZERTY, QWERTZ, ABNT, ABC
- Language built: English, Spanish, Italian, French, German, Polish, Portuguese, Portuguese Brazilian, Hungarian
- All config options exposed in gui
- HDD/ATA drives hidden for deckard ps2 (SCPH-75K+)
- Xfrom/dvr_hdd0 hidden from non-PSX consoles
- Dual hdd support
- multi-usb support
- HDD Manager can inject APA partition headers
- View/edit APA partiion header `system.cnf`
- All drivers besides core lazy loading for faster boot and support of all devices.
- Create and Extract PSU options so user knows what will happen
- Extra file extensions for Text Editor ShortCuts
- Timestamp manipulation feature to fix the date of any memory card folder containing any icon-based exploit _(\*tuna)_
- Launch KELFs (encrypted elfs)
- [LaunchELF with Args](#launchelf-with-args)
- [APA Header injection](#HDD-APA-header-injection) from usb, mmce and udpfs. Thanks to Alex Parrado and @israpps
- warnings when modifying/deleting exploit folders on PS2/PSX
- Support for PS3/PS4 Dualshocks thanks to Alex Parrado (DS34 build)


### LaunchELF with Args

<details>

<summary>LaunchELF with Args</summary>

In FileBrowser, select a TextEditor-supported file, press `R1`, then choose `LaunchELF with Args`.
wLaunchELF parses the file immediately, caches the args in memory, and leaves you in FileBrowser to select the target `ELF`.

The same action is available from TextEditor's `R1` menu for the active opened file.
From TextEditor the current in-memory buffer is parsed, so unsaved edits are used.
After caching args, wLaunchELF returns to FileBrowser so you can choose the target executable.

For normal executable launches, wLaunchELF also checks for a sidecar arg file named `<elf name>.arg` in the same folder, for example `BOOT.arg` for `BOOT.ELF`.
Sidecar args use the same parser and limits.
If args were already cached through `R1` -> `LaunchELF with Args`, the sidecar `.arg` file is ignored.

Each non-empty line becomes one argument.
Line endings are stripped; other characters on the line are preserved.

| Limit | Value |
|---|---:|
| User arguments | 12 lines |
| Characters per argument | 255 |
| Total cached argument text | 2,048 bytes including terminators |

The target receives the normal selected/handoff path as `argv[0]`, followed by the cached line arguments.

</details>

### HDD APA header injection

<details>

<summary>HDD APA header injection</summary>

The HDD Manager R1 menu includes `Inject Header` for PFS partitions.
Source devices are `usb:`, `mmce0:`, `mmce1:`, and `udpfs:`.
After choosing a source device, choose one injection mode:

| Mode | Behavior |
|---|---|
| `This partition only` | Injects only the currently selected PFS partition from `device:/__Headers/<selected partition>/`. |
| `Matching partitions` | Scans `device:/__Headers/` and injects every existing PFS partition with a matching folder name. |
| `Matching and create missing partitions` | Injects existing matches, then prompts for each valid header folder that has no matching partition. `No` skips that folder and continues; cancel stops the bulk operation. |

Header files must be placed in a flat folder matching the partition name:

`device:/__Headers/<matching partition name>/`


| File | Requirement | Destination | Maximum File Size | Format / Purpose | Notes |
|---|---|---|---:|---|---|
| `system.cnf` | Required | APA header | 512 bytes | Partition configuration | See [example](#example-systemcnf). |
| `icon.sys` | Required on PS2 | APA header | 1,024 bytes | Icon metadata | Not listed as required for PSX/DVR environments. |
| `list.ico` | Required on PS2 | APA header | 1,112,064 bytes | HDD-OSD icon | [Icon Generator](https://github.com/CosmicScale/HDD-OSD-Icon-Generator)  |
| `boot.kelf` or `BOOT.KELF` | Optional | APA header | 978,944 bytes | Executable KELF | Use KELFTool. |
| `BOOT.ELF` | Optional | `pfs:/BOOT.ELF` | N/A | Partition executable | Useful when the injected `boot.kelf` or `BOOT.KELF` is a KELF forwarder such as [OSDMenu Launcher](https://github.com/pcm720/OSDMenu/tree/main/launcher) |
| `info.sys` | Optional | `pfs:/res/info.sys` | N/A | Partition information used by PSBBN or PSX XMB | See [example](#example-infosys). |
| `jkt_001.png` | Optional | `pfs0:/res/jkt_001.png` | N/A | PSBBN image | 256x256 PNG 8-bit indexed color 32-bit RGBA palette. |
| `jkt_002.png` | Optional | `pfs0:/res/jkt_002.png` | N/A | PSX XMB image | 76x108 PNG 8-bit indexed color 32-bit RGBA palette. |
| `jkt_cp.png` | Optional | `pfs0:/res/jkt_cp.png` | N/A | PSX XMB copyright image | 290 pixels wide, 46-300 pixels high, 32-bit RGBA PNG. |

#### Example system.cnf

```ini
BOOT2 = pfs:/boot.kelf
VER = 1.00
VMODE = NTSC
HDDUNITPOWER = NICHDD
```

#### Example info.sys

```ini
title = [SYS] R3CONFIGURATOR
title_id = SYS-R3CONFI
title_sub_id = 0
release_date =
developer_id =
publisher_id = pcm720, R3Z3N
note =
content_web =
image_topviewflag = 0
image_type = 0
image_count = 1
image_viewsec = 600
copyright_viewflag = 0
copyright_imgcount = 0
genre =
parental_lock = 1
effective_date = 0
expire_date = 0
violence_flag = 0
content_type = 255
content_subtype = 0
```

</details>

### Custom ELF signing

<details>

<summary>Sign a custom ELF with MC Exploit Installer</summary>

In `MISC` -> `MC Exploit Installer`, choose a supported installation mode and then choose `Choose other ELF`.
Select a native `ELF` file. Its accompanying `PS2KEYS.dat` must be in the same source folder, for example:

```text
usb0:/APPS/MyApp.ELF
usb0:/APPS/PS2KEYS.dat
```

`PS2KEYS.dat` is user-provided, is required to sign a custom ELF, and is intentionally not included in this repository. It remains at the source location; it is not copied to the memory card.

The installer chooses the signing configuration from the detected console model:

| Console model | Key section | Header | System type |
|---|---|---|---|
| `SCPH-*` | `retail` | DNASLOAD | PS2 |
| `DESR-*` | `retail` | FMCB | PSX |
| `DTL-H*` or `DTL-T*` | `dev` | DNASLOAD | PS2 |
| `COH-*`, System 246/256, or Python 1 | `arcade` | DONGLE | PS2 |
| `SCPH-50000MB` / Python 2 | `retail` | DONGLE | PS2 |

The values below are SHA-256 fingerprints of the expected uppercase hexadecimal values, calculated from the ASCII value with no trailing newline. They identify the required format only: `sha256:<digest>` is **not** a valid `PS2KEYS.dat` value. Replace every fingerprint with the corresponding user-provided uppercase hexadecimal value. The `arcade` section requires both `OVERRIDE_KBIT` and `OVERRIDE_KC`.

PS2KEYS.dat example but replaced with hashed values:  
```ini
[retail]
MG_CARDKEY_0=10b3e77ebefdfc5799437702728daf8cfb690478f1609091b162df010a2c312d
MG_CARDIV_0=32a05c2d8df0a2f09c46a771797a8165fc4ed3bff287aa8eff2f5b32d80aea32
MG_CARDKEY2_0=f8fca3262cefaa7c2a395550f0db2e401f9f6227fdebe7426ee23f1c535a0371
MG_CARDIV2_0=e77db0f555df1cd15727e262572105d71f33b5b6728998cf5dba142b7332b859
MG_CARDKEY_1=095c3a5a9524c1718a0a486cdeb5674b6dbc43f0327facc83a8dad6adc9468c5
MG_CARDIV_1=563ae74b29f816162f9e74bae8aa41c782e68f19cdb9e17a500b8e95f0a467c2
MG_CARDKEY2_1=2c8329f3fc5b861cfe647b99cb7bebf3f92af3a95c08072afd9beea2a281b454
MG_CARDIV2_1=3cc5259d406637502de781af3658418a9988078fbd103cab43d55c26f42aaffb
MG_CARDKEY_2=34505998124831ce8cb5b2c86c80162d485653fc8c2b4a5025f6bfc1750d89f6
MG_CARDIV_2=9d3e9e8b18c8cd86bc28b7e8699169b0334241ac217303cfb7c3f98cb23d11ba
MG_CARDKEY2_2=c5333cf25cacef73597d73ba1fe1de988b8bf0138aee8f5085a5fdbc34e0fd59
MG_CARDIV2_2=0d27a23fd7154fe795aa0aa1f23209996ee7251ce6b7dc050bde21c20c4c2faa
MG_KBIT_MASTER_KEY=fcc2366ae3437b38425e16b01072be2c878e614f66b9c86899e912837c1f5973
MG_KC_MASTER_KEY=a0a827e712a19e541adc9ffee921a7c45240ef8137412a94227ec0a1f4b41dc0
MG_KBIT_IV=83f7bf81eb2513bde54522c4b18289ce259b81685ff55f021a533583da54a739
MG_KC_IV=5ec05c6613eb8c0f278070f9e36f610134021b7f29db548c9e26fed154191705
MG_SIG_MASTER_KEY=9a374c2a012666f3117fc11dfe2640ccff9b7c616af1defb70045ac08f193259
MG_SIG_HASH_KEY=b14de37e8381040ba0721129b87ab0f0d4d60f081f5d113675500ff941e500f0
MG_ROOTSIG_HASH_KEY=68b1ecec391291714f819e6974bfd22398b06c08fdd6546ddd7d066f42c32d30
MG_ROOTSIG_MASTER_KEY=ad77c6d5162de9004e684a609154131f8a92e41212374b0ae62a81fbfe3bf7a0
MG_CONTENT_IV=5f40d05a6b00218d000938172a90b17de5c13cdcc92e9d9bf24f892947206715
MG_CONTENT_TABLE_IV=8697f69515b4f7abb69c8e45283076da678aa6165cc3099306f6c0dd55a4e591
MG_CHALLENGE_IV=11569b681f98ebe5c9fab2cf670ee97e380a94ee9d85e69d99aa32d6ab2471b7


[dev]
MG_CARDKEY_0=5b3e6b167257de9607cb5a740a45dd91e64c63e7daaaea0b519ee422ccc3df2d
MG_CARDIV_0=75dce6aea56243fbc3a050e5c3e1efa54d6d172b7fdedcd624ebb47789ff7b59
MG_CARDKEY2_0=6e16dd5299ebd48f9e4ca454f308a0ac5a7e3e8e1ce982de0effcdc156f835c6
MG_CARDIV2_0=82f0055bdd2fd5504529f478f0ffad8a3b257b46b4b5df4b13085afc28ee212d
MG_CARDKEY_1=2fd64443f8baf1f1d515d1d29d67cf812dd607c54db9c1072d944531fad8eb16
MG_CARDIV_1=c086bc63539695c04b7ad0e3191ffca581edca16077398f55cc1099aa25e01d0
MG_CARDKEY2_1=2fd64443f8baf1f1d515d1d29d67cf812dd607c54db9c1072d944531fad8eb16
MG_CARDIV2_1=c086bc63539695c04b7ad0e3191ffca581edca16077398f55cc1099aa25e01d0
MG_CARDKEY_2=2fd64443f8baf1f1d515d1d29d67cf812dd607c54db9c1072d944531fad8eb16
MG_CARDIV_2=c086bc63539695c04b7ad0e3191ffca581edca16077398f55cc1099aa25e01d0
MG_CARDKEY2_2=2fd64443f8baf1f1d515d1d29d67cf812dd607c54db9c1072d944531fad8eb16
MG_CARDIV2_2=c086bc63539695c04b7ad0e3191ffca581edca16077398f55cc1099aa25e01d0
MG_KBIT_MASTER_KEY=fcc2366ae3437b38425e16b01072be2c878e614f66b9c86899e912837c1f5973
MG_KC_MASTER_KEY=a0a827e712a19e541adc9ffee921a7c45240ef8137412a94227ec0a1f4b41dc0
MG_KBIT_IV=83f7bf81eb2513bde54522c4b18289ce259b81685ff55f021a533583da54a739
MG_KC_IV=5ec05c6613eb8c0f278070f9e36f610134021b7f29db548c9e26fed154191705
MG_SIG_MASTER_KEY=9a374c2a012666f3117fc11dfe2640ccff9b7c616af1defb70045ac08f193259
MG_SIG_HASH_KEY=b14de37e8381040ba0721129b87ab0f0d4d60f081f5d113675500ff941e500f0
MG_ROOTSIG_HASH_KEY=68b1ecec391291714f819e6974bfd22398b06c08fdd6546ddd7d066f42c32d30
MG_ROOTSIG_MASTER_KEY=ad77c6d5162de9004e684a609154131f8a92e41212374b0ae62a81fbfe3bf7a0
MG_CONTENT_IV=5f40d05a6b00218d000938172a90b17de5c13cdcc92e9d9bf24f892947206715
MG_CONTENT_TABLE_IV=8697f69515b4f7abb69c8e45283076da678aa6165cc3099306f6c0dd55a4e591
MG_CHALLENGE_IV=11569b681f98ebe5c9fab2cf670ee97e380a94ee9d85e69d99aa32d6ab2471b7


[arcade]
MG_CARDKEY_0=56b688be9e3f4019986886c25c46ba881a72c167dfe4c7ab9a4b82ce90ee3d90
MG_CARDIV_0=f04115dc7b5e97624e9ee1fcebae9db330401cda3bdc8eb7de85a67c653d1acb
MG_CARDKEY2_0=55ef86747f1b8a6c746c85d429528afbdee934c9a5dcb52421cfba41875b2a78
MG_CARDIV2_0=89fd514976db406b9f53a76a3d27d3b59dc21486b1b71dc62d3488fac2c61c5d
MG_CARDKEY_1=bf670992348132010f10d3061728d5f8057b3e8575939fd1d7187e7664faa16c
MG_CARDIV_1=5ae0acd2dccfff45d6ef4ada82b8caf4b7b0fb84841bc6c56a986505d45e1c38
MG_CARDKEY2_1=810f0861759a757f098e04335fa584618ac8e461688bfee4cd8bc03e9f4efefc
MG_CARDIV2_1=c96656e1d9a02fc575148f4a734fa669ddeab346504c48017c21645e4d1da2d8
MG_CARDKEY_2=bf670992348132010f10d3061728d5f8057b3e8575939fd1d7187e7664faa16c
MG_CARDIV_2=5ae0acd2dccfff45d6ef4ada82b8caf4b7b0fb84841bc6c56a986505d45e1c38
MG_CARDKEY2_2=810f0861759a757f098e04335fa584618ac8e461688bfee4cd8bc03e9f4efefc
MG_CARDIV2_2=c96656e1d9a02fc575148f4a734fa669ddeab346504c48017c21645e4d1da2d8
MG_KBIT_MASTER_KEY=0ce7ef3379dc65192146c2acc19629a90db151ceda32ad5ed4b3646f6b5c688a
MG_KC_MASTER_KEY=d084d40e43a93d4f71de4e1cebfc565eec06411b9de6a6676e5a2baa078f2f3f
MG_KBIT_IV=7f233e2abe0609bd985a6d73126f8fa88408809454905875bc608681fd0850a4
MG_KC_IV=e234051a356ab9ddd49cc6498a3b5d225ecdfd4dfa0bc9ac89fb8a8d686b8a5c
MG_SIG_MASTER_KEY=b9294362296593489a96587d9e808097ecfb81c303466ec8270af583fa52267c
MG_SIG_HASH_KEY=29ebbbab992cf16081434bd9ea5c6aed0bd5ee6f10f8dba999c6554a617c3b8e
MG_ROOTSIG_HASH_KEY=fcd17bcaea888de254bd43d32011ade6c5a38739d690b1b5a7d344efb619664c
MG_ROOTSIG_MASTER_KEY=a073ee8a155a43b5ab879926efe048930e6e1d7388d552329f18ccb73d852777
MG_CONTENT_IV=9939fd347b26baa1e5ed8a89db9fa865f70ac9b53a14bccde18b1a1b86ee949f
MG_CONTENT_TABLE_IV=62b40bbd0f9a3ff72c0794e07a20c51537d08a8b93fdfa11433aa0c6413c9c90
MG_CHALLENGE_IV=11569b681f98ebe5c9fab2cf670ee97e380a94ee9d85e69d99aa32d6ab2471b7
OVERRIDE_KBIT=d45a3ffcbef644059e7a5b3171e61307f82706030c2aed2ad06b486190b934c0
OVERRIDE_KC=8ed5233f34040e6a60ef1cee714913e898d558cf552e2bbef147378fcc2539d0


[prototype]
MG_CARDKEY_0=0408351013a632af3b9942f861c128bdc036267d43f84c025d3a26fcc575cc7e
MG_CARDIV_0=26a8ecd3271975ac8ec60b7e55137fe332de7f2a6666c9ff5c6124e9ff308688
MG_CARDKEY2_0=ac1d5ea8aedb8ed6bbd7c703fd4a12618dd417fec67f12ae9d72f00a4feab0b5
MG_CARDIV2_0=0471eaff23672149fb328744837d9ff0031c557ba339a375877e3e9ab397bc9d
MG_CARDKEY_1=3b5102e174acca23b2c5f9cb420f84995ec96626da4b0af7adf5b4688dfe9728
MG_CARDIV_1=27a103b77fa24bc84fee78848adf3f86c009e165164cab9da788e83cb575acf1
MG_CARDKEY2_1=640da93499ed0fc938db5b4868fdbf9d1a755732e5c244ff4980c42bd2d62e67
MG_CARDIV2_1=73da8e3d8fbb243fb0d43c717790ee23148524e3e2608e0d650f07eaa0d741e4
MG_CARDKEY_2=7372c53892a720abe2e7d42a4e8b31c87bd5213579c513c539ecf8e9acb30ac5
MG_CARDIV_2=c33052107a3c3109722723301112609df51aa11ccf68a74f7a3ef8d64e996747
MG_CARDKEY2_2=a74a56fff8d44406bd9f2317583f7f28d62da49ec5649d7ab18d9cdec0de66da
MG_CARDIV2_2=49f11bb7a40ed48e55efb61b166f86a5b492e1e3dc8b2909b4defba9a2d70f59
MG_KBIT_MASTER_KEY=24f91d27cd4b60f843f08b83d8a3840a1c1eeb8cb2f6fad301feedea3a693b5b
MG_KC_MASTER_KEY=01d02a712965de548a60d8f6d6e99078d7e2097b3fa5fe245d8817585b29fc48
MG_KBIT_IV=6b8682420158660d3e5813e11aac50f89216fe0012dc4665b9ca8114065f12e8
MG_KC_IV=dc05524b2925091e103205959f608e21b71228938b390058e9c614de0306cbb5
MG_SIG_MASTER_KEY=6f7b5925f018c5d2ed24950a58e06daacc4638003f9fe6660f15a06ebb89f830
MG_SIG_HASH_KEY=969229b7cced83355317ae625aec8f2b9fb89606ea95c3b6cff32798be5f4e39
MG_ROOTSIG_HASH_KEY=6922fdef36385473e89660febc23471bfee20a02bf6587918f5d78d3767529df
MG_ROOTSIG_MASTER_KEY=560e50a8b9b4ca735b83248dddbaedda73fec20bbcc092d81e2b60c8d9fa232f
MG_CONTENT_IV=8224540de8460eb4bc26f25cadd39350ef9b5f64887a9afc1ac8bde5d4d69e3f
MG_CONTENT_TABLE_IV=6d187c4758f828b1bc228bd0ad7679978558501578cf0f9bbb3cdbd967523b95
MG_CHALLENGE_IV=11569b681f98ebe5c9fab2cf670ee97e380a94ee9d85e69d99aa32d6ab2471b7
```

</details>

### Build shortcuts

- `make all-ds34-off` builds a dedicated ELF without DS34 support.
- `make all-ds34-on` builds a dedicated ELF with DS34 support.
- `make all-ds34-variants` builds both variants in one run.

