#ifndef MCPASTE_FUNCTIONS_H
#define MCPASTE_FUNCTIONS_H

#include "launchelf.h"

#define MC_BACKUP_ATTR_FILENAME "PS2_MC_Backup_Attributes.BUP.bin"

void clear_mcTable(sceMcTblGetDir *mcT);
int mcpaste_prepare_backup_dir(const char *out_dir, const sceMcTblGetDir *dir_stats);
int mcpaste_prepare_restore_dir(const char *in_dir, sceMcTblGetDir *dir_stats_out);
int mcpaste_finalize_restore_dir(int in_fd, const char *out_dir, int set_mode);
int mcpaste_apply_entry_stats_to_mc(const char *out_dir, const sceMcTblGetDir *stats, int set_mode);
int mcpaste_write_file_stats(int out_fd, const sceMcTblGetDir *stats);
int mcpaste_is_attr_file(const char *name);

#endif
