/* Host adapter for exercising the real save manager and VMC transaction code. */
#ifndef SAVE_TEST_PLATFORM_H
#define SAVE_TEST_PLATFORM_H
#define LAUNCHELF_H
#define FILER_SHARED_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
typedef int64_t s64;
#define MAX_PATH 1025
#define MAX_NAME 256
#define MAX_ENTRY 2048
#define FIO_O_RDONLY 1
#define FIO_O_WRONLY 2
#define FIO_O_RDWR 3
#define FIO_O_APPEND 0x100
#define FIO_O_CREAT 0x200
#define FIO_O_TRUNC 0x400
#define FIO_O_EXCL 0x800
#define FIO_S_IFDIR 0x1000
#define FIO_S_IFREG 0x2000
#define FIO_S_IRUSR 0x100
#define FIO_S_IWUSR 0x80
#define sceMcFileAttrSubdir 0x20
#define sceMcResSucceed 0
#define sceMcResChangedCard -1
#define PM_NORMAL 0
#define PM_PSU_BACKUP 1
#define PM_PSU_RESTORE 2
typedef struct
{
	uint8_t _Create[8], _Modify[8];
	uint32_t FileSizeByte;
	uint16_t AttrFile, Reserve1;
	uint32_t Reserve2, PdaAplNo;
	uint8_t EntryName[32];
} sceMcTblGetDir;
typedef struct
{
	char name[MAX_NAME];
	uint8_t title[66];
	sceMcTblGetDir stats;
} FILEINFO;
typedef struct
{
	uint32_t mode, attr, size;
	uint8_t ctime[8], atime[8], mtime[8];
	uint32_t hisize, private_0, private_1, private_2;
} iox_stat_t;
extern int vmcMounted[2], size_valid, time_valid, fileMode;
int openPathForRead(const char *, char *, size_t);
int genFixPath(const char *, char *);
int genGetStat(const char *, iox_stat_t *);
int genOpen(const char *, int);
int genRead(int, void *, int);
int genWrite(int, void *, int);
int genClose(int);
s64 genLseek(int, s64, int);
int genRemove(char *);
int genRmdir(char *);
int genMkdir(const char *, int);
int genCmpFileExt(const char *, const char *);
int getDir(const char *, FILEINFO *);
int fileXioRename(const char *, const char *);
int fileXioChStat(const char *, iox_stat_t *, int);
int mcGetInfo(int, int, int *, int *, int *);
int mcSync(int, int *, int *);
int mcSetFileInfo(int, int, const char *, const sceMcTblGetDir *, int);
int keyboard(char *, int);
#endif
