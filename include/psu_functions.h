#ifndef PSU_FUNCTIONS_H
#define PSU_FUNCTIONS_H

#include "launchelf.h"
#include "psu_types.h"

void clear_psu_header(psu_header *psu);
void pad_psu_header(psu_header *psu);
int psu_backup_create_root_image(const char *path, const mcT_header *folder_stats, int *out_fd, int *psu_content);
int psu_backup_finalize_root_image(int out_fd, const mcT_header *folder_stats, int psu_content);
int psu_backup_begin_file_entry(int psu_fd, const mcT_header *file_stats, int *psu_pad_size, int *psu_content);
int psu_backup_write_padding(int psu_fd, int psu_pad_size);
int psu_restore_open_root_image(const char *path, FILEINFO *folder_file, int *in_fd, int *psu_content);
int psu_restore_read_file_entry(int psu_fd, FILEINFO *file, int *psu_pad_size);
int psu_restore_apply_entry_stats_to_mc(const char *out_dir, const sceMcTblGetDir *stats, int set_mode);

#endif
