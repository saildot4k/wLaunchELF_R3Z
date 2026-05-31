BEGIN {
	# Patch PS2SDK vmcman backing support for full-size VMC images.
	in_mount = 0
}

/^int mcman_iomanx_backing_mount/ {
	in_mount = 1
}

/^int mcman_iomanx_backing_umount/ {
	in_mount = 0
}

/^int mcman_iomanx_backing_getcardspec/ {
	in_mount = 0
}

{
	if (in_mount && $0 ~ /^\tcardinfo->cardsize = cardsize;/) {
		print "\t/* cardinfo->cardsize is total pages; set after superblock validation. */"
		next
	}

	if (in_mount && $0 ~ /^\t\ttotal_pages = superblock.pages_per_cluster \* superblock.blocksize;/) {
		print "\t\ttotal_pages = superblock.pages_per_cluster * superblock.clusters_per_card;"
		next
	}

	print

	if (in_mount && $0 ~ /^[[:space:]]*cardinfo->flags = superblock.cardflags;/) {
		print "\t\t\tcardinfo->cardsize = total_pages;"
		print "\t\t\tcardinfo->mounted = 1;"
	}
}
