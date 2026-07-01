#!/usr/bin/env sh
set -eu

if [ "$#" -ne 6 ]; then
	echo "Usage: $0 <loader.elf> <nm-tool> <loadaddr> <stackaddr|auto> <stacksize|auto> <low_mem_limit>" >&2
	exit 2
fi

elf_path="$1"
nm_tool="$2"
load_addr_expr="$3"
stack_addr_expr="$4"
stack_size_expr="$5"
low_limit_expr="$6"

if [ ! -f "$elf_path" ]; then
	echo "layout-check: missing ELF '$elf_path'" >&2
	exit 2
fi

find_sym_hex() {
	sym_name="$1"
	"$nm_tool" -n "$elf_path" | awk -v s="$sym_name" '$3 == s { print $1; found=1; exit } END { if (!found) exit 1 }'
}

find_tool_from_nm() {
	target_name="$1"
	case "$nm_tool" in
		*nm)
			printf '%s\n' "${nm_tool%nm}${target_name}"
			;;
		*)
			printf '%s\n' "$target_name"
			;;
	esac
}

image_lo=
image_hi=

if image_lo_hex="$(find_sym_hex _ftext 2>/dev/null)" && image_hi_hex="$(find_sym_hex _end 2>/dev/null)"; then
	image_lo=$((0x$image_lo_hex))
	image_hi=$((0x$image_hi_hex))
else
	readelf_tool="$(find_tool_from_nm readelf)"
	if ! command -v "$readelf_tool" >/dev/null 2>&1; then
		echo "layout-check: missing symbols and no readelf tool available for fallback" >&2
		exit 1
	fi

	while read -r seg_vaddr seg_memsz; do
		[ -n "$seg_vaddr" ] || continue
		seg_lo=$((seg_vaddr))
		seg_hi=$((seg_vaddr + seg_memsz))
		if [ -z "${image_lo}" ] || [ "$seg_lo" -lt "$image_lo" ]; then
			image_lo="$seg_lo"
		fi
		if [ -z "${image_hi}" ] || [ "$seg_hi" -gt "$image_hi" ]; then
			image_hi="$seg_hi"
		fi
	done <<EOF_SEGMENTS
$("$readelf_tool" -l "$elf_path" | awk '/^[[:space:]]*LOAD[[:space:]]/ { print $3, $6 }')
EOF_SEGMENTS

	if [ -z "${image_lo}" ] || [ -z "${image_hi}" ]; then
		echo "layout-check: could not determine loader image range from symbols or LOAD segments" >&2
		exit 1
	fi
fi

load_addr=$((load_addr_expr))
low_limit=$((low_limit_expr))

if [ "$stack_addr_expr" = "auto" ] || [ "$stack_size_expr" = "auto" ]; then
	if [ "$stack_addr_expr" != "auto" ] || [ "$stack_size_expr" != "auto" ]; then
		echo "layout-check: stack address and size must both be numeric or both be auto" >&2
		exit 2
	fi
	if stack_addr_hex="$(find_sym_hex _stack 2>/dev/null)" && stack_size_hex="$(find_sym_hex _stack_size 2>/dev/null)"; then
		stack_addr=$((0x$stack_addr_hex))
		stack_size=$((0x$stack_size_hex))
	else
		stack_addr=$image_hi
		stack_size=$((low_limit - image_hi))
	fi
	stack_mode=up
else
	stack_addr=$((stack_addr_expr))
	stack_size=$((stack_size_expr))
	stack_mode=conservative
fi

if [ "$image_hi" -le "$image_lo" ]; then
	echo "layout-check: invalid image range _ftext=0x$image_lo_hex _end=0x$image_hi_hex" >&2
	exit 1
fi

# In low-memory mode, keep the image below the configured low-memory boundary.
if [ "$load_addr" -lt "$low_limit" ] && [ "$image_hi" -gt "$low_limit" ]; then
	echo "layout-check: loader image crosses low-memory limit: _end=0x$(printf '%x' "$image_hi"), limit=0x$(printf '%x' "$low_limit")" >&2
	exit 1
fi

if [ "$stack_size" -le 0 ]; then
	echo "layout-check: invalid stack size: 0x$(printf '%x' "$stack_size")" >&2
	exit 1
fi

stack_hi=$((stack_addr + stack_size))
if [ "$load_addr" -lt "$low_limit" ] && [ "$stack_hi" -gt "$low_limit" ]; then
	echo "layout-check: stack crosses low-memory limit: stack_hi=0x$(printf '%x' "$stack_hi"), limit=0x$(printf '%x' "$low_limit")" >&2
	exit 1
fi

overlaps() {
	a_lo="$1"
	a_hi="$2"
	b_lo="$3"
	b_hi="$4"
	[ "$a_lo" -lt "$b_hi" ] && [ "$b_lo" -lt "$a_hi" ]
}

if [ "$stack_mode" = "up" ]; then
	stack_up_lo=$((stack_addr))
	stack_up_hi=$((stack_hi))
	if overlaps "$image_lo" "$image_hi" "$stack_up_lo" "$stack_up_hi"; then
		echo "layout-check: loader image overlaps stack window" >&2
		echo "  image:     0x$(printf '%x' "$image_lo")..0x$(printf '%x' "$image_hi")" >&2
		echo "  stack(up): 0x$(printf '%x' "$stack_up_lo")..0x$(printf '%x' "$stack_up_hi")" >&2
		exit 1
	fi
else
	# Be conservative for legacy fixed-stack layouts: check both possible interpretations.
	stack_down_lo=$((stack_addr - stack_size))
	stack_down_hi=$((stack_addr))
	stack_up_lo=$((stack_addr))
	stack_up_hi=$((stack_hi))

	if overlaps "$image_lo" "$image_hi" "$stack_down_lo" "$stack_down_hi" || \
	   overlaps "$image_lo" "$image_hi" "$stack_up_lo" "$stack_up_hi"; then
		echo "layout-check: loader image overlaps stack window" >&2
		echo "  image:     0x$(printf '%x' "$image_lo")..0x$(printf '%x' "$image_hi")" >&2
		echo "  stack(dn): 0x$(printf '%x' "$stack_down_lo")..0x$(printf '%x' "$stack_down_hi")" >&2
		echo "  stack(up): 0x$(printf '%x' "$stack_up_lo")..0x$(printf '%x' "$stack_up_hi")" >&2
		exit 1
	fi
fi

echo "layout-check: OK image=0x$(printf '%x' "$image_lo")..0x$(printf '%x' "$image_hi") stack=0x$(printf '%x' "$stack_addr") size=0x$(printf '%x' "$stack_size")"
