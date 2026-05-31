BEGIN {
	in_detect = 0
	patched = 0
}

/^int mcman_detectcard\(int port, int slot\)/ {
	in_detect = 1
}

/^int McDetectCard2\(int port, int slot\)/ {
	in_detect = 1
}

{
	print

	if (in_detect && $0 ~ /^[[:space:]]*mcdi = \(MCDevInfo \*\)&mcman_devinfos\[port\]\[slot\];/) {
		print ""
		print "#if defined(BUILDING_VMCMAN)"
		print "\t/* VMC media cannot physically change while mounted; avoid fragile re-probes. */"
		print "\tif ((mcdi->cardtype == sceMcTypePS2) && (mcdi->cardform == 1))"
		print "\t\treturn sceMcResSucceed;"
		print "#endif"
		patched++
		in_detect = 0
	}
}

END {
	if (patched < 2)
		exit 1
}
