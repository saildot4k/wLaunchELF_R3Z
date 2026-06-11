# __wLaunchELF 4.50_R3Z__

Based off of [wLE_ISR](https://github.com/israpps/wLaunchELF_ISR)

# Supported devices:
- __mc:/__         Memory Cards
- __usb:/__        Fat/exFAT (BDM) USB
- __mmce:/__       Multi Purpose Memory Card Emulator IE SD2PSX, PSxMemCard Gen2 or MemCard Pro 2. There are a few more varients of SD2PSX
- __mx4sio:/__     SD card interface over memory card port. Slowe than MMCE
- __hdd:/__        APA formatted internal HDD
- __ata:/__        BDM hard drive, exFAT for now till more are supported
- __xfrom:/__      PSX DESR-XXXX flash storage
- __dvr_hdd0:/__   PSX DESR-XXXX digital video recorder hdd partition/side
- __cdfs:/__       CD/DVD File System
- __udpfs:/__      Network interface used with [PCM720s UDPFSD Server](https://github.com/pcm720/udpfsdf )

Drivers are lazy loading for for maximum compatibility. MMCE and MX4SIO will incure an IOP reboot as the 2 are incompatible.

Dual HDD/ATA support is built in for future development.

## Features:
- RetroGem Game ID for [PIXEL FX RetroGem](https://www.pixelfx.co/hdmi-retro-gem)
- Writes to history file for disc launches for mmce vmc change
- Applies deckard disc patches
- X/0 applied per region if no config file is found so O is only default for Japan
- Keyboard layout choices: QWERTY, DVORAK, AZERTY, QWERTZ, ABNT, ABC
- Language files built in for English, Spanish, Italian, French, German, Polish, Portuguese, Portuguese Brazilian
- All config options exposed in gui
- HDD/ATA drives hidden for deckard ps2 (SCPH-75K+)
- Xfrom/dvr_hdd0 hidden from non-PSX consoles
- Dual hdd support
- All drivers besides core lazy loading for faster boot and support of everything
- Create and Extract PSU options so user knows what will happen
- Extra file extensions for Text Editor ShortCuts
- Timestamp manipulation feature to fix the date of any memory card folder containing any icon-based exploit _(\*tuna)_
- Extra file extensions for Text Editor ShortCuts
- Support for PS3/PS4 Dualshocks thanks to Alex Parrado

### Build shortcuts

- `make all-ds34-off` builds a dedicated ELF without DS34 support.
- `make all-ds34-on` builds a dedicated ELF with DS34 support.
- `make all-ds34-variants` builds both variants in one run.


# **original readme**
wLaunchELF, formerly known as uLaunchELF, also known as wLE or uLE (abbreviated), is an open source file manager and executable launcher for the Playstation 2 console based off of the original LaunchELF. It contains many different features, including a text editor, hard drive manager, as well as network support, and much more.
