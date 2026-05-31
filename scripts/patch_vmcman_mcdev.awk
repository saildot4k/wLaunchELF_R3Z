BEGIN {
	in_open = 0
	in_mkdir = 0
	detect_patches = 0
	mkdir_logs = 0
	open_logs = 0
}

/^int mc_open\(/ {
	in_open = 1
}

/^int mc_mkdir\(/ {
	in_mkdir = 1
}

/^int mc_[a-z_]+\(/ && $0 !~ /^int mc_open\(/ && $0 !~ /^int mc_mkdir\(/ {
	in_open = 0
	in_mkdir = 0
}

{
	if ($0 ~ /^\tr = McDetectCard2\(mcman_mc_port, mcman_mc_slot\);/) {
		print "#if defined(BUILDING_VMCMAN)"
		print "\tif (mcman_devinfos[mcman_mc_port][mcman_mc_slot].cardform == 1)"
		print "\t\tr = sceMcResSucceed;"
		print "\telse"
		print "#endif"
		print "\t\tr = McDetectCard2(mcman_mc_port, mcman_mc_slot);"
		detect_patches++
		next
	}

	if (in_open && $0 ~ /^\treturn mcman_ioerrcode\(r\);/) {
		print "#if defined(BUILDING_VMCMAN)"
		print "\tif ((flags & (sceMcFileCreateFile | sceMcFileCreateDir)) != 0)"
		print "\t\tprintf(\"vmcman: mc_open return raw=%d mapped=%d\\n\", r, mcman_ioerrcode(r));"
		print "#endif"
		print
		open_logs++
		next
	}

	if (in_mkdir && $0 ~ /^\tif \(r >= -1\) \{/) {
		print "#if defined(BUILDING_VMCMAN)"
		print "\tprintf(\"vmcman: mc_mkdir detect ret=%d cardtype=%d cardform=%d\\n\", r, mcman_devinfos[mcman_mc_port][mcman_mc_slot].cardtype, mcman_devinfos[mcman_mc_port][mcman_mc_slot].cardform);"
		print "#endif"
		print
		mkdir_logs++
		next
	}

	if (in_mkdir && $0 ~ /^\treturn mcman_ioerrcode\(r\);/) {
		print "#if defined(BUILDING_VMCMAN)"
		print "\tprintf(\"vmcman: mc_mkdir return raw=%d mapped=%d\\n\", r, mcman_ioerrcode(r));"
		print "#endif"
		print
		mkdir_logs++
		next
	}

	if (in_open && $0 ~ /^\tmcman_unit2card\(f->unit\);/) {
		print
		print "#if defined(BUILDING_VMCMAN)"
		print "\tif ((flags & (sceMcFileCreateFile | sceMcFileCreateDir)) != 0)"
		print "\t\tprintf(\"vmcman: mc_open request unit=%d port=%d slot=%d name='%s' flags=0x%x cardtype=%d cardform=%d\\n\", f->unit, mcman_mc_port, mcman_mc_slot, filename, flags, mcman_devinfos[mcman_mc_port][mcman_mc_slot].cardtype, mcman_devinfos[mcman_mc_port][mcman_mc_slot].cardform);"
		print "#endif"
		open_logs++
		next
	}

	if (in_open && $0 ~ /^\t\tr = McOpen\(mcman_mc_port, mcman_mc_slot, filename, flags\);/) {
		print
		print "#if defined(BUILDING_VMCMAN)"
		print "\t\tif ((flags & (sceMcFileCreateFile | sceMcFileCreateDir)) != 0)"
		print "\t\t\tprintf(\"vmcman: mc_open McOpen name='%s' flags=0x%x ret=%d\\n\", filename, flags, r);"
		print "#endif"
		open_logs++
		next
	}

	if (in_mkdir && $0 ~ /^\tmcman_unit2card\(f->unit\);/) {
		print
		print "#if defined(BUILDING_VMCMAN)"
		print "\tprintf(\"vmcman: mc_mkdir request unit=%d port=%d slot=%d dirname='%s' cardtype=%d cardform=%d\\n\", f->unit, mcman_mc_port, mcman_mc_slot, dirname, mcman_devinfos[mcman_mc_port][mcman_mc_slot].cardtype, mcman_devinfos[mcman_mc_port][mcman_mc_slot].cardform);"
		print "#endif"
		mkdir_logs++
		next
	}

	if (in_mkdir && $0 ~ /^\t\tr = McOpen\(mcman_mc_port, mcman_mc_slot, dirname, 0x40\);/) {
		print
		print "#if defined(BUILDING_VMCMAN)"
		print "\t\tprintf(\"vmcman: mc_mkdir McOpen dirname='%s' ret=%d\\n\", dirname, r);"
		print "#endif"
		mkdir_logs++
		next
	}

	print
}

END {
	if (detect_patches < 6 || mkdir_logs < 4 || open_logs < 3)
		exit 1
}
