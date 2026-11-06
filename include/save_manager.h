#ifndef SAVE_MANAGER_H
#define SAVE_MANAGER_H

#include "launchelf.h"
#include "save_formats.h"

int saveReadFile(const char *path, uint8_t **data, size_t *size);
int saveWriteNew(const char *path, const uint8_t *data, size_t size);
int saveCardType(const char *path);
int saveCardRoot(const char *path);
int saveInspect(const char *path, const FILEINFO *file);
int saveCollect(const char *path, const FILEINFO *file, save_archive_t *save);
int saveImport(const char *source, const char *destination);
int saveBackup(const char *source, const FILEINFO *file, const char *destination, int format);
/* filename includes its extension; NULL opens the custom filename prompt. */
int saveBackupCard(const char *source, const FILEINFO *files, int count, const char *destination, int format, const char *filename);
int saveCheckCardCopy(const char *source, const char *destination, const FILEINFO *file, int mode);

/* EE PS1 virtual cards share vmc0:/vmc1:/ with the IOP PS2 VMC driver. */
int saveVmcSlot(const char *path);
int saveVmcMount(int slot, const char *path); /* 1: PS1 mounted, 0: try PS2, <0: invalid */
void saveVmcUnmount(int slot);
int saveVmcList(const char *path, FILEINFO *files, int max);
int saveVmcStat(const char *path, iox_stat_t *stat);
int saveVmcFree(const char *path);
int saveVmcOpen(const char *path, int flags);
int saveVmcFd(int fd);
int saveVmcRead(int fd, void *data, int size);
int saveVmcWrite(int fd, const void *data, int size);
s64 saveVmcSeek(int fd, s64 where, int how);
int saveVmcClose(int fd);
void saveVmcAbort(int fd);
int saveVmcDelete(const char *path);
int saveVmcRename(const char *path, const char *name);
int saveVmcPut(const char *path, const save_archive_t *save);

#endif
