BEGIN {
	# Patch PS2SDK's mcman backing source when building it as vmcman.
	# These prints run inside the IOP IRX, so PPCTTY sees them even if EE printf is quiet.
	in_mount = 0
	in_umount = 0
	pending_mount_validate = 0
	pending_umount_validate = 0
}

/^int mcman_iomanx_backing_mount/ {
	in_mount = 1
	in_umount = 0
}

/^int mcman_iomanx_backing_umount/ {
	in_mount = 0
	in_umount = 1
}

/^int mcman_iomanx_backing_getcardspec/ {
	in_mount = 0
	in_umount = 0
}

{
	if (in_mount && $0 ~ /^\tcardinfo->cardsize = cardsize;/) {
		print "\t/* cardinfo->cardsize is total pages; set after superblock validation. */"
		next
	}

	if (in_mount && $0 ~ /^\t\ttotal_pages = superblock.pages_per_cluster \* superblock.blocksize;/) {
		print "\t\ttotal_pages = superblock.pages_per_cluster * superblock.clusters_per_card;"
		print "\t\tprintf(\"vmcman: geometry pagesize=%d pages_per_cluster=%u blocksize=%u clusters_per_card=%u total_pages=%d cardsize=%d flags=0x%02x\\n\","
		print "\t\t       superblock.pagesize, superblock.pages_per_cluster, superblock.blocksize,"
		print "\t\t       superblock.clusters_per_card, total_pages, cardsize, superblock.cardflags);"
		next
	}

	print

	if (in_mount && $0 ~ /^\t\(void\)port;/) {
		print "\tprintf(\"vmcman: mount request port=%d slot=%d file='%s'\\n\", port, slot, filename);"
	}

	if (in_mount && $0 ~ /^\tr = mcman_iomanx_backing_validate\(slot, 0\);/) {
		pending_mount_validate = 1
	}
	else if (pending_mount_validate && $0 ~ /^\tif \(r < 0\) \{/) {
		print "\t\tprintf(\"vmcman: mount validate failed slot=%d ret=%d\\n\", slot, r);"
		pending_mount_validate = 0
	}

	if (in_mount && $0 ~ /^\tfd = iomanX_open\(filename, FIO_O_RDWR, 0\);/) {
		print "\tprintf(\"vmcman: backing open ret=%d file='%s'\\n\", fd, filename);"
	}

	if (in_mount && $0 ~ /^\tif \(fd < 0\) \{/) {
		print "\t\tprintf(\"vmcman: backing open failed ret=%d\\n\", fd);"
	}

	if (in_mount && $0 ~ /^\tcardsize = iomanX_lseek\(fd, 0, FIO_SEEK_END\);/) {
		print "\tprintf(\"vmcman: backing size=%d\\n\", cardsize);"
	}

	if (in_mount && $0 ~ /^\t\tread_result = iomanX_read\(fd, &superblock, sizeof\(superblock\)\);/) {
		print "\t\tprintf(\"vmcman: superblock read=%d expected=%d\\n\", read_result, (int)sizeof(superblock));"
	}

	if (in_mount && $0 ~ /^\t\tif \(read_result != sizeof\(superblock\)\) \{/) {
		print "\t\t\tprintf(\"vmcman: reject superblock read ret=%d\\n\", read_result);"
	}

	if (in_mount && $0 ~ /^\t\tif \(strncmp\(superblock.magic, SUPERBLOCK_MAGIC, 28\) != 0\) \{/) {
		print "\t\t\tprintf(\"vmcman: reject magic first16=%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\\n\","
		print "\t\t\t       ((unsigned char *)superblock.magic)[0], ((unsigned char *)superblock.magic)[1],"
		print "\t\t\t       ((unsigned char *)superblock.magic)[2], ((unsigned char *)superblock.magic)[3],"
		print "\t\t\t       ((unsigned char *)superblock.magic)[4], ((unsigned char *)superblock.magic)[5],"
		print "\t\t\t       ((unsigned char *)superblock.magic)[6], ((unsigned char *)superblock.magic)[7],"
		print "\t\t\t       ((unsigned char *)superblock.magic)[8], ((unsigned char *)superblock.magic)[9],"
		print "\t\t\t       ((unsigned char *)superblock.magic)[10], ((unsigned char *)superblock.magic)[11],"
		print "\t\t\t       ((unsigned char *)superblock.magic)[12], ((unsigned char *)superblock.magic)[13],"
		print "\t\t\t       ((unsigned char *)superblock.magic)[14], ((unsigned char *)superblock.magic)[15]);"
	}

	if (in_mount && $0 ~ /^\t\tif \(superblock.cardtype != sceMcTypePS2\) \{/) {
		print "\t\t\tprintf(\"vmcman: reject cardtype=%u expected=%u\\n\", superblock.cardtype, sceMcTypePS2);"
	}

	if (in_mount && $0 ~ /^\t\telse \{/) {
		print "\t\t\tprintf(\"vmcman: reject size actual=%d no_ecc=%d ecc=%d\\n\","
		print "\t\t\t       cardsize, superblock.pagesize * total_pages,"
		print "\t\t\t       (superblock.pagesize + 0x10) * total_pages);"
	}

	if (in_mount && $0 ~ /^\t\tif \(superblock.pagesize != 512 && superblock.pagesize != 528\) \{/) {
		print "\t\t\tprintf(\"vmcman: reject pagesize=%d\\n\", superblock.pagesize);"
	}

	if (in_mount && $0 ~ /^[[:space:]]*cardinfo->flags = superblock.cardflags;/) {
		print "\t\t\tcardinfo->cardsize = total_pages;"
		print "\t\t\tcardinfo->mounted = 1;"
		print "\t\t\tprintf(\"vmcman: validated has_ecc=%d pagesize=%d blocksize=%u total_pages=%d\\n\","
		print "\t\t\t       cardinfo->has_ecc, cardinfo->pagesize, cardinfo->blocksize, cardinfo->cardsize);"
	}

	if (in_mount && $0 ~ /^\tmcman_probePS2Card\(port, slot\);/) {
		print "\tprintf(\"vmcman: probe complete port=%d slot=%d\\n\", port, slot);"
	}

	if (in_mount && $0 ~ /^\tif \(McGetFormat\(port, slot\) > 0\) \{/) {
		print "\t\tprintf(\"vmcman: mount format check succeeded\\n\");"
	}

	if (in_mount && $0 ~ /^\tif \(McGetFormat\(port, slot\) < 0\) \{/) {
		print "\t\tprintf(\"vmcman: reject format check failed\\n\");"
	}

	if (in_mount && $0 ~ /^cleanup:/) {
		print "\tprintf(\"vmcman: mount cleanup ret=%d\\n\", r);"
	}

	if (in_umount && $0 ~ /^\t\(void\)port;/) {
		print "\tprintf(\"vmcman: umount request port=%d slot=%d\\n\", port, slot);"
	}

	if (in_umount && $0 ~ /^\tr = mcman_iomanx_backing_validate\(slot, 1\);/) {
		pending_umount_validate = 1
	}
	else if (pending_umount_validate && $0 ~ /^\tif \(r < 0\) \{/) {
		print "\t\tprintf(\"vmcman: umount validate failed slot=%d ret=%d\\n\", slot, r);"
		pending_umount_validate = 0
	}

	if (in_umount && $0 ~ /^\tmcman_iomanx_backing_clear_slot\(slot\);/) {
		print "\tprintf(\"vmcman: umount cleared slot=%d\\n\", slot);"
	}

	if (in_umount && $0 ~ /^cleanup:/) {
		print "\tprintf(\"vmcman: umount cleanup ret=%d\\n\", r);"
	}
}
