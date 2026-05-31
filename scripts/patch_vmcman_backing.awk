function leading_ws(line) {
	match(line, /^[[:space:]]*/)
	return substr(line, 1, RLENGTH)
}

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

	if ($0 ~ /^[[:space:]]*iomanX_lseek\(cardinfo->fd, page \* effective_page_size, FIO_SEEK_SET\);/) {
		indent = leading_ws($0)
		print indent "if (iomanX_lseek(cardinfo->fd, page * effective_page_size, FIO_SEEK_SET) < 0) {"
		print indent "\tprintf(\"vmcman: backing seek failed page=%d size=%d\\n\", page, effective_page_size);"
		print indent "\treturn 1;"
		print indent "}"
		next
	}

	if ($0 ~ /^[[:space:]]*iomanX_write\(cardinfo->fd, buf, effective_page_size\);/) {
		indent = leading_ws($0)
		print indent "if (iomanX_write(cardinfo->fd, buf, effective_page_size) != effective_page_size) {"
		print indent "\tprintf(\"vmcman: erase write failed page=%d size=%d\\n\", page, effective_page_size);"
		print indent "\treturn 1;"
		print indent "}"
		next
	}

	if ($0 ~ /^[[:space:]]*iomanX_write\(cardinfo->fd, pagebuf, 512\);/) {
		indent = leading_ws($0)
		print indent "if (iomanX_write(cardinfo->fd, pagebuf, 512) != 512) {"
		print indent "\tprintf(\"vmcman: page write failed page=%d\\n\", page);"
		print indent "\treturn 1;"
		print indent "}"
		next
	}

	if ($0 ~ /^[[:space:]]*iomanX_write\(cardinfo->fd, eccbuf, 16\);/) {
		indent = leading_ws($0)
		print indent "if (iomanX_write(cardinfo->fd, eccbuf, 16) != 16) {"
		print indent "\tprintf(\"vmcman: ecc write failed page=%d\\n\", page);"
		print indent "\treturn 1;"
		print indent "}"
		next
	}

	if ($0 ~ /^[[:space:]]*iomanX_read\(cardinfo->fd, pagebuf, 512\);/) {
		indent = leading_ws($0)
		print indent "if (iomanX_read(cardinfo->fd, pagebuf, 512) != 512) {"
		print indent "\tprintf(\"vmcman: page read failed page=%d\\n\", page);"
		print indent "\treturn 1;"
		print indent "}"
		next
	}

	if ($0 ~ /^[[:space:]]*iomanX_read\(cardinfo->fd, eccbuf, 16\);/) {
		indent = leading_ws($0)
		print indent "if (iomanX_read(cardinfo->fd, eccbuf, 16) != 16) {"
		print indent "\tprintf(\"vmcman: ecc read failed page=%d\\n\", page);"
		print indent "\treturn 1;"
		print indent "}"
		next
	}

	print

	if (in_mount && $0 ~ /^[[:space:]]*cardinfo->flags = superblock.cardflags;/) {
		print "\t\t\tcardinfo->cardsize = total_pages;"
		print "\t\t\tcardinfo->mounted = 1;"
	}
}
