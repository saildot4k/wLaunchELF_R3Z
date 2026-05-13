//--------------------------------------------------------------
//File name:    elf.c
//--------------------------------------------------------------
#include "launchelf.h"

#define MAX_PATH 1025

extern u8 loader_elf[];
extern int size_loader_elf;

// ELF-loading stuff
#define ELF_MAGIC 0x464c457f
#define KELF_MAGIC 0x464c454b
#define XLF_MAGIC 0x00464c58
#define ELF_PT_LOAD 1

//------------------------------
typedef struct
{
	u8 ident[16];  // struct definition for ELF object header
	u16 type;
	u16 machine;
	u32 version;
	u32 entry;
	u32 phoff;
	u32 shoff;
	u32 flags;
	u16 ehsize;
	u16 phentsize;
	u16 phnum;
	u16 shentsize;
	u16 shnum;
	u16 shstrndx;
} elf_header_t;
//------------------------------
typedef struct
{
	u32 type;  // struct definition for ELF program section header
	u32 offset;
	void *vaddr;
	u32 paddr;
	u32 filesz;
	u32 memsz;
	u32 flags;
	u32 align;
} elf_pheader_t;

static void normalizeLaunchArgPath(const char *in_path, char *out_path);

static int openExecPathForRead(const char *path, char *resolved_path)
{
	char alt_path[MAX_PATH];
	char *sep;
	int fd;

	normalizeLaunchArgPath(path, resolved_path);
	fd = genOpen(resolved_path, O_RDONLY);
	if (fd >= 0)
		return fd;

	/* Compatibility fallback for stacks that still accept "mass:foo" style. */
	strcpy(alt_path, resolved_path);
	sep = strchr(alt_path, ':');
	if (sep && sep[1] == '/') {
		memmove(sep + 1, sep + 2, strlen(sep + 2) + 1);
		fd = genOpen(alt_path, O_RDONLY);
		if (fd >= 0) {
			strcpy(resolved_path, alt_path);
			return fd;
		}
	}

	return -1;
}
//--------------------------------------------------------------
//End of data declarations
//--------------------------------------------------------------
//Start of function code
//--------------------------------------------------------------
// checkELFheader Tests for valid ELF file
// Modified version of loader from Independence
//	(C) 2003 Marcus R. Brown <mrbrown@0xd6.org>
//--------------------------------------------------------------
int checkELFheader(char *path)
{
	elf_header_t elf_head;
	u8 *boot_elf = (u8 *)&elf_head;
	elf_header_t *eh = (elf_header_t *)boot_elf;
	int fd, ret, read_bytes;
	char fullpath[MAX_PATH], openpath[MAX_PATH], tmp[MAX_PATH], *p;
	u32 magic;

	strcpy(fullpath, path);
	if (!strncmp(fullpath, "cdfs", 4))
		loadCdModules();
#ifdef MMCE
	if (!strncmp(fullpath, "mmce", 4))
		loadMmceModules();
#endif
#ifdef MX4SIO
	if (!strncmp(fullpath, "mx4sio", 6) && !mx4sio_driver_running && !loadMx4sioModules())
		goto error;
#endif
#ifdef EXFAT
	if (!strncmp(fullpath, "ata", 3))
		loadAtaModules();
#endif
	if (!strncmp(fullpath, "mc", 2) 
		|| !strncmp(fullpath, "vmc", 3)
		|| !strncmp(fullpath, "rom", 3)
		|| !strncmp(fullpath, "cdrom", 5)
#ifdef MMCE
		|| !strncmp(fullpath, "mmce", 4)
#endif
#ifdef XFROM
		|| (console_is_PSX && !strncmp(fullpath, "xfrom", 5))
#endif
		|| !strncmp(fullpath, "cdfs", 4)) {
		;  //fullpath is already correct
	} else if (!strncmp(fullpath, "hdd0:", 5)) {
		p = &path[5];
		if (*p == '/')
			p++;
		sprintf(tmp, "hdd0:%s", p);
		p = strchr(tmp, '/');
		sprintf(fullpath, "pfs0:%s", p);
		*p = 0;
		if ((ret = mountParty(tmp)) < 0)
			goto error;
		fullpath[3] += ret;
#ifdef DVRP
	} else if (!strncmp(fullpath, "dvr_hdd0:", 9)) {
		if (!console_is_PSX)
			goto error;
		p = &path[9];
		if (*p == '/')
			p++;
		sprintf(tmp, "dvr_hdd0:%s", p);
		p = strchr(tmp, '/');
		sprintf(fullpath, "dvr_pfs0:%s", p);
		*p = 0;
		if ((ret = mountDVRPParty(tmp)) < 0)
			goto error;
		fullpath[7] += ret;
#endif
	} else if (!strncmp(fullpath, "usb", 3)) {
		if (genFixPath(path, fullpath) < 0)
			goto error;
	} else if (!strncmp(fullpath, "mass", 4)) {
		char *pathSep;

		pathSep = strchr(path, '/');
		if (pathSep && (pathSep - path < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - path), pathSep + 1);
	} else if (!strncmp(fullpath, "ata", 3)) {
		char *pathSep;

		pathSep = strchr(path, '/');
		if (pathSep && (pathSep - path < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - path), pathSep + 1);
#ifdef MX4SIO
	} else if (!strncmp(fullpath, "mx4sio:", 7)) {
		if (genFixPath(path, fullpath) < 0)
			goto error;
#endif
	} else if (!strncmp(fullpath, "host:", 5)) {
		if (path[5] == '/')
			strcpy(fullpath + 5, path + 6);
	} else {
		return 0;  //return 0 for unrecognized device
	}
	if ((fd = openExecPathForRead(fullpath, openpath)) < 0)
		goto error;

	/*
	 * Some device stacks do not reliably report size via SEEK_END.
	 * Only header bytes are needed, so seek to start and proceed.
	 */
	if (genLseek(fd, 0, SEEK_SET) < 0) {
		genClose(fd);
		if ((fd = openExecPathForRead(fullpath, openpath)) < 0)
			goto error;
	}
	read_bytes = genRead(fd, boot_elf, sizeof(elf_header_t));
	genClose(fd);
	if (read_bytes < (int)sizeof(u32))
		goto error;

	memcpy(&magic, eh->ident, sizeof(magic));
	if (magic == ELF_MAGIC && eh->type == 2)
		return 1;  // successful ELF check
	if (magic == KELF_MAGIC || magic == XLF_MAGIC)
		return 2;  // encrypted KELF/XLF payload

	if (magic != ELF_MAGIC)
		goto error;
	if (eh->type != 2)
		goto error;

	return 1;  //return 1 for successful check
error:
	return -1;  //return -1 for failed check
}
//------------------------------
//End of func:  int checkELFheader(const char *path)
//--------------------------------------------------------------
static void normalizeLaunchArgPath(const char *in_path, char *out_path)
{
	char *sep;

	strcpy(out_path, in_path);
	sep = strchr(out_path, ':');
	if (sep == NULL)
		return;

	// Keep APA/HDD handoff format unchanged (e.g. hdd0:partition:pfs:/path).
	if (!strncmp(out_path, "hdd0:", 5) || !strncmp(out_path, "dvr_hdd0:", 9) ||
	    !strncmp(out_path, "pfs", 3) || !strncmp(out_path, "dvr_pfs", 7))
		return;

	if (sep[1] == 0 || sep[1] == '/' || sep[1] == '\\')
		return;

	memmove(sep + 2, sep + 1, strlen(sep + 1) + 1);
	sep[1] = '/';
}
//--------------------------------------------------------------
// RunLoaderElf loads LOADER.ELF from program memory and passes
// args of selected ELF and partition to it
// Modified version of loader from Independence
//	(C) 2003 Marcus R. Brown <mrbrown@0xd6.org>
//------------------------------
void RunLoaderElf(char *filename, char *party)
{
#define ELFLOAD_ARGC 3
	u8 *boot_elf;
	elf_header_t *eh;
	elf_pheader_t *eph;
	void *pdata;
	int i;
	char *argv[ELFLOAD_ARGC], bootpath[256], launchpath[MAX_PATH];

	if ((!strncmp(party, "hdd0:", 5)) && (!strncmp(filename, "pfs0:", 5))) {
		if (0 > fileXioMount("pfs0:", party, FIO_MT_RDONLY)) {
			//Some error occurred, it could be due to something else having used pfs0
			unmountParty(0);  //So we try unmounting pfs0, to try again
			if (0 > fileXioMount("pfs0:", party, FIO_MT_RDONLY))
				return;  //If it still fails, we have to give up...
		}

		//If a path to a file on PFS is specified, change it to the standard format.
		//hdd0:partition:pfs:path/to/file
		if (strncmp(filename, "pfs0:", 5) == 0) {
			sprintf(bootpath, "%s:pfs:%s", party, &filename[5]);
		} else {
			sprintf(bootpath, "%s:%s", party, filename);
		}

		argv[0] = filename;
		normalizeLaunchArgPath(bootpath, launchpath);
		argv[1] = launchpath;
#ifdef DVRP
	} else if ((!strncmp(party, "dvr_hdd0:", 9)) && (!strncmp(filename, "dvr_pfs0:", 9))) {
		if (0 > fileXioMount("dvr_pfs0:", party, FIO_MT_RDONLY)) {
			//Some error occurred, it could be due to something else having used pfs0
			unmountDVRPParty(0);  //So we try unmounting pfs0, to try again
			if (0 > fileXioMount("dvr_pfs0:", party, FIO_MT_RDONLY))
				return;  //If it still fails, we have to give up...
		}

		//If a path to a file on PFS is specified, change it to the standard format.
		//dvr_hdd0:partition:pfs:path/to/file
		if (strncmp(filename, "dvr_pfs0:", 9) == 0) {
			sprintf(bootpath, "%s:pfs:%s", party, &filename[9]);
		} else {
			sprintf(bootpath, "%s:%s", party, filename);
		}
		argv[0] = filename;
		normalizeLaunchArgPath(bootpath, launchpath);
		argv[1] = launchpath;
#endif
	} else {
		argv[0] = filename;
		normalizeLaunchArgPath(filename, launchpath);
		argv[1] = launchpath;
	}

	/* NB: LOADER.ELF is embedded  */
	boot_elf = (u8 *)loader_elf;
	eh = (elf_header_t *)boot_elf;
	if (_lw((u32)&eh->ident) != ELF_MAGIC)
		asm volatile("break\n");

	eph = (elf_pheader_t *)(boot_elf + eh->phoff);

	/* Scan through the ELF's program headers and copy them into RAM, then
									zero out any non-loaded regions.  */
	for (i = 0; i < eh->phnum; i++) {
		if (eph[i].type != ELF_PT_LOAD)
			continue;

		pdata = (void *)(boot_elf + eph[i].offset);
		memcpy(eph[i].vaddr, pdata, eph[i].filesz);

		if (eph[i].memsz > eph[i].filesz)
			memset(eph[i].vaddr + eph[i].filesz, 0,
			       eph[i].memsz - eph[i].filesz);
	}
	argv[2] = (setting->reboot_iop_elf_load) ? "-r" : "-nr";
	/* Let's go.  */
	SifExitRpc();
	FlushCache(0);
	FlushCache(2);

	ExecPS2((void *)eh->entry, NULL, ELFLOAD_ARGC, argv);
}
//------------------------------
//End of func:  void RunLoaderElf(char *filename, char *party)
//--------------------------------------------------------------
//End of file:  elf.c
//--------------------------------------------------------------
