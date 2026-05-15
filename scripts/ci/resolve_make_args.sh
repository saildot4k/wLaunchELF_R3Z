#!/usr/bin/env sh
set -eu

# Resolve high-level CI build profile selectors into concrete make flags.
#
# Usage:
#   resolve_make_args.sh <psx_profile> <pad_profile> <storage_profile> [debug] [ppc_uart]
#
# Profiles:
#   psx_profile:     no-psx | psx
#   pad_profile:     no-ds34 | ds34
#   storage_profile: usb | mmce | mx4sio | all | minimal
#
# Optional:
#   debug:    0|1 (default 0)
#   ppc_uart: 0|1 (default 0)

PSX_PROFILE="${1:-no-psx}"
PAD_PROFILE="${2:-no-ds34}"
STORAGE_PROFILE="${3:-all}"
DEBUG_FLAG="${4:-0}"
PPC_UART_FLAG="${5:-0}"

# Shared defaults.
ETH=1
EXFAT=1
MMCE=0
MX4SIO=0
DS34=0
DVRP=0
XFROM=0
LCDVD=LATEST

case "$PSX_PROFILE" in
no-psx)
	DVRP=0
	XFROM=0
	;;
psx)
	DVRP=1
	XFROM=1
	;;
*)
	echo "Unknown psx_profile: $PSX_PROFILE" >&2
	exit 1
	;;
esac

case "$PAD_PROFILE" in
no-ds34)
	DS34=0
	;;
ds34)
	DS34=1
	;;
*)
	echo "Unknown pad_profile: $PAD_PROFILE" >&2
	exit 1
	;;
esac

case "$STORAGE_PROFILE" in
usb)
	EXFAT=1
	MMCE=0
	MX4SIO=0
	;;
mmce)
	EXFAT=1
	MMCE=1
	MX4SIO=0
	;;
mx4sio)
	EXFAT=1
	MMCE=0
	MX4SIO=1
	;;
all)
	EXFAT=1
	MMCE=1
	MX4SIO=1
	;;
minimal)
	EXFAT=0
	MMCE=0
	MX4SIO=0
	;;
*)
	echo "Unknown storage_profile: $STORAGE_PROFILE" >&2
	exit 1
	;;
esac

# Dependency closure / overlap guards.
if [ "$MX4SIO" = "1" ] && [ "$EXFAT" != "1" ]; then
	echo "MX4SIO requires EXFAT=1" >&2
	exit 1
fi

if [ "$MMCE" = "1" ] && [ "$EXFAT" != "1" ]; then
	echo "MMCE profile currently requires EXFAT=1 in this build matrix." >&2
	exit 1
fi

printf '%s\n' \
	"ETH=$ETH EXFAT=$EXFAT MMCE=$MMCE MX4SIO=$MX4SIO LCDVD=$LCDVD DVRP=$DVRP XFROM=$XFROM DS34=$DS34 DEBUG=$DEBUG_FLAG PPC_UART=$PPC_UART_FLAG"
