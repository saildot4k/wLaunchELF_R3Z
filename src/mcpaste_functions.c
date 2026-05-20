#include "mcpaste_functions.h"
#include <stdio.h>
#include <string.h>

void clear_mcTable(sceMcTblGetDir *mcT)
{
	memset((void *)mcT, 0, sizeof(sceMcTblGetDir));
}

int mcpaste_prepare_backup_dir(const char *out_dir, const sceMcTblGetDir *dir_stats)
{
	char path[MAX_PATH];
	int out_fd;

	if (snprintf(path, sizeof(path), "%s/%s", out_dir, MC_BACKUP_ATTR_FILENAME) >= (int)sizeof(path))
		return -1;
	genRemove(path);
	out_fd = genOpen(path, FIO_O_WRONLY | FIO_O_CREAT);
	if (out_fd < 0)
		return -1;
	if (genWrite(out_fd, (void *)dir_stats, 64) != 64) {
		genClose(out_fd);
		return -1;
	}
	return out_fd;
}

int mcpaste_prepare_restore_dir(const char *in_dir, sceMcTblGetDir *dir_stats_out)
{
	char path[MAX_PATH];
	int in_fd;

	if (snprintf(path, sizeof(path), "%s/%s", in_dir, MC_BACKUP_ATTR_FILENAME) >= (int)sizeof(path))
		return -1;
#if defined(ETH) || defined(UDPFS)
	if (!strncmp(path, "host:/", 6))
		makeHostPath(path, path);
#endif
	in_fd = genOpen(path, FIO_O_RDONLY);
	if (in_fd < 0)
		return -1;
	if (genRead(in_fd, (void *)dir_stats_out, 64) != 64) {
		genClose(in_fd);
		return -1;
	}
	return in_fd;
}

int mcpaste_apply_entry_stats_to_mc(const char *out_dir, const sceMcTblGetDir *stats, int set_mode)
{
	char path[MAX_PATH];
	int dummy;

	if (strncmp(out_dir, "mc", 2))
		return -1;
	if (snprintf(path, sizeof(path), "%s%.32s", out_dir, (const char *)stats->EntryName) >= (int)sizeof(path))
		return -1;

	mcGetInfo(path[2] - '0', 0, &dummy, &dummy, &dummy);  //Wakeup call
	mcSync(0, NULL, &dummy);
	mcSetFileInfo(path[2] - '0', 0, &path[4], (sceMcTblGetDir *)stats, set_mode);
	mcSync(0, NULL, &dummy);
	return 0;
}

int mcpaste_finalize_restore_dir(int in_fd, const char *out_dir, int set_mode)
{
	sceMcTblGetDir stats;
	int size;

	for (size = 64; size == 64;) {
		size = genRead(in_fd, (void *)&stats, 64);
		if (size == 64)
			mcpaste_apply_entry_stats_to_mc(out_dir, &stats, set_mode);
	}
	genClose(in_fd);
	return 0;
}

int mcpaste_write_file_stats(int out_fd, const sceMcTblGetDir *stats)
{
	return (genWrite(out_fd, (void *)stats, 64) == 64) ? 0 : -1;
}

int mcpaste_is_attr_file(const char *name)
{
	return !strcmp(name, MC_BACKUP_ATTR_FILENAME);
}
