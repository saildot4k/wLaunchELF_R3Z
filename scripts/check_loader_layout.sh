#!/usr/bin/env sh
set -eu

if [ "$#" -ne 6 ]; then
	echo "Usage: $0 <loader.elf> <nm-tool> <loadaddr> <stackaddr> <stacksize> <low_mem_limit>" >&2
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

image_lo_hex="$(find_sym_hex _ftext)" || {
	echo "layout-check: missing _ftext in $elf_path" >&2
	exit 1
}

image_hi_hex="$(find_sym_hex _end)" || {
	echo "layout-check: missing _end in $elf_path" >&2
	exit 1
}

image_lo=$((0x$image_lo_hex))
image_hi=$((0x$image_hi_hex))
load_addr=$((load_addr_expr))
stack_addr=$((stack_addr_expr))
stack_size=$((stack_size_expr))
low_limit=$((low_limit_expr))

if [ "$image_hi" -le "$image_lo" ]; then
	echo "layout-check: invalid image range _ftext=0x$image_lo_hex _end=0x$image_hi_hex" >&2
	exit 1
fi

# In low-memory mode (loader linked below first MiB), keep image below 1 MiB.
if [ "$load_addr" -lt "$low_limit" ] && [ "$image_hi" -gt "$low_limit" ]; then
	echo "layout-check: loader image crosses low-memory limit: _end=0x$(printf '%x' "$image_hi"), limit=0x$(printf '%x' "$low_limit")" >&2
	exit 1
fi

overlaps() {
	a_lo="$1"
	a_hi="$2"
	b_lo="$3"
	b_hi="$4"
	[ "$a_lo" -lt "$b_hi" ] && [ "$b_lo" -lt "$a_hi" ]
}

# Be conservative: check both possible interpretations for stack range.
stack_down_lo=$((stack_addr - stack_size))
stack_down_hi=$((stack_addr))
stack_up_lo=$((stack_addr))
stack_up_hi=$((stack_addr + stack_size))

if overlaps "$image_lo" "$image_hi" "$stack_down_lo" "$stack_down_hi" || \
   overlaps "$image_lo" "$image_hi" "$stack_up_lo" "$stack_up_hi"; then
	echo "layout-check: loader image overlaps stack window" >&2
	echo "  image:     0x$(printf '%x' "$image_lo")..0x$(printf '%x' "$image_hi")" >&2
	echo "  stack(dn): 0x$(printf '%x' "$stack_down_lo")..0x$(printf '%x' "$stack_down_hi")" >&2
	echo "  stack(up): 0x$(printf '%x' "$stack_up_lo")..0x$(printf '%x' "$stack_up_hi")" >&2
	exit 1
fi

echo "layout-check: OK image=0x$(printf '%x' "$image_lo")..0x$(printf '%x' "$image_hi") stack=0x$(printf '%x' "$stack_addr") size=0x$(printf '%x' "$stack_size")"
