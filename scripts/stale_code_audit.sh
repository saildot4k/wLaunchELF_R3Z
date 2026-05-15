#!/usr/bin/env sh
set -eu

STRICT=0
if [ "${1:-}" = "--strict" ]; then
	STRICT=1
	shift
fi

ROOTS="src include iop loader .github/workflows Makefile embed.make"
REPORT_DIR="${1:-build}"
REPORT_FILE="$REPORT_DIR/stale-code-report.txt"

mkdir -p "$REPORT_DIR"

{
	echo "Stale Code Audit"
	echo "================"
	echo
	echo "1) Legacy marker scan"
	rg -n "\bTODO\b|\bFIXME\b|\bHACK\b|LNCHELF\.CNF|legacy" $ROOTS || true
	echo
	echo "2) Disabled code blocks (#if 0)"
	rg -n "^\s*#if\s+0\b" src include iop loader || true
	echo
	echo "3) Suspicious duplicate config probes"
	rg -n "LAUNCHELF\.CNF|IPCONFIG\.DAT" src/main.c src/config.c || true
	echo
	echo "4) Large files (refactor candidates)"
	wc -l src/*.c | sort -nr | head -n 10
} > "$REPORT_FILE"

cat "$REPORT_FILE"

if [ "$STRICT" -eq 1 ]; then
	# Strict mode fails if we detect explicit stale markers.
	if rg -q "\bTODO\b|\bFIXME\b|LNCHELF\.CNF" $ROOTS; then
		echo "Strict stale-code audit failed." >&2
		exit 1
	fi
fi
