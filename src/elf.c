//--------------------------------------------------------------
//File name:    elf.c
//--------------------------------------------------------------
#include "launchelf.h"
#include <elf-loader.h>

#define MAX_PATH 1025

extern u8 loader_elf[];
extern int size_loader_elf;

// ELF-loading stuff
#define ELF_MAGIC 0x464c457f
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

static int readExecHeader(const char *path, u8 *header, int header_len, int *opened_file)
{
	int fd, rd, total;

	if (opened_file != NULL)
		*opened_file = 0;

	if (path == NULL || path[0] == '\0')
		return -1;

	fd = genOpen(path, FIO_O_RDONLY);
	if (fd < 0)
		return -1;

	if (opened_file != NULL)
		*opened_file = 1;

	/* Some devices do not support SEEK_END reliably. SEEK_SET is enough here. */
	genLseek(fd, 0, SEEK_SET);

	total = 0;
	while (total < header_len) {
		rd = genRead(fd, header + total, header_len - total);
		if (rd <= 0)
			break;
		total += rd;
	}
	genClose(fd);

	return total;
}

static int classifyExecHeader(const u8 *header, int header_len)
{
	u32 magic;

	if (header_len < 4)
		return -1;

	memcpy(&magic, header, sizeof(magic));

	/*
	 * Heuristic requested for additional encrypted KELF variants:
	 * byte[0] == 0x01 and byte[2] == 0x00.
	 */
	if (header[0] == 0x01 && header[2] == 0x00)
		return 2;

	if (magic != ELF_MAGIC)
		return -1;

	/*
	 * Validate key ELF header fields when enough bytes are available:
	 *  e_type at 0x10, e_machine at 0x12.
	 */
	if (header_len >= 20) {
		u16 e_type, e_machine;
		memcpy(&e_type, header + 16, sizeof(e_type));
		memcpy(&e_machine, header + 18, sizeof(e_machine));
		if ((e_type != 2) && (e_type != 3))
			return -1;
		if (e_machine != 8)
			return -1;
	}

	return 1;
}

static int classifyExecByExtension(const char *path)
{
	if (genCmpFileExt(path, "KELF") || genCmpFileExt(path, "XLF"))
		return 2;
	if (genCmpFileExt(path, "ELF"))
		return 1;

	return -1;
}

static int parseUsbMassPathUnit(const char *path, const char *prefix, int prefix_len, int *unit, const char **suffix)
{
	if (strncmp(path, prefix, prefix_len))
		return 0;

	if (path[prefix_len] == ':') {
		*unit = 0;
		*suffix = path + prefix_len + 1;
		return 1;
	}
	if (path[prefix_len] >= '0' && path[prefix_len] <= '9' && path[prefix_len + 1] == ':') {
		*unit = path[prefix_len] - '0';
		*suffix = path + prefix_len + 2;
		return 1;
	}

	return 0;
}

static int isHddPartyPath(const char *path)
{
	return (!strncmp(path, "hdd", 3) && path[3] >= '0' && path[3] <= '9' && path[4] == ':');
}

static int isHddBrowserPath(const char *path)
{
	return (isHddPartyPath(path) && path[5] == '/');
}

static const char *normalizeExecArg0Path(const char *path, char *buffer, size_t buffer_size)
{
	const char *suffix;
	int unit = 0;

	if (path == NULL || path[0] == '\0' || buffer == NULL || buffer_size == 0)
		return path;

	if (!parseUsbMassPathUnit(path, "usb", 3, &unit, &suffix) &&
	    !parseUsbMassPathUnit(path, "mass", 4, &unit, &suffix))
		return path;

	if (*suffix == '\0')
		suffix = "/";

	if (*suffix != '/')
		snprintf(buffer, buffer_size, "mass%d:/%s", unit, suffix);
	else
		snprintf(buffer, buffer_size, "mass%d:%s", unit, suffix);

	return buffer;
}

static int tryCheckExecPath(const char *path, int *opened_any)
{
	u8 header[256];
	int opened_file = 0;
	int header_size;
	int kind;

	header_size = readExecHeader(path, header, sizeof(header), &opened_file);
	if (opened_file && opened_any != NULL)
		*opened_any = 1;

	if (header_size < 0)
		return -1;  // open failure
	if (header_size < 4)
		return 0;  // open success but not enough data

	kind = classifyExecHeader(header, header_size);
	return kind;
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
	int ret, kind, opened_any, fallback_kind;
	char fullpath[MAX_PATH], tmp[MAX_PATH];

	if (path == NULL || path[0] == '\0')
		return -1;

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

	opened_any = 0;

	/* 1) Try raw path first. */
	kind = tryCheckExecPath(path, &opened_any);
	if (kind > 0)
		return kind;

	/* 2) Try host path without the extra slash after device separator. */
	if (!strncmp(path, "host:/", 6)) {
		snprintf(tmp, sizeof(tmp), "host:%s", path + 6);
		kind = tryCheckExecPath(tmp, &opened_any);
		if (kind > 0)
			return kind;
	}

	/* 3) Try generic fixed path (includes HDD partition mount/translation). */
	ret = genFixPath(path, fullpath);
	if ((ret >= -99) && strcmp(fullpath, path)) {
		kind = tryCheckExecPath(fullpath, &opened_any);
		if (kind > 0)
			return kind;
	}

	/* 4) Resolve generic memory card alias explicitly. */
	if (!strncmp(path, "mc:/", 4)) {
		snprintf(tmp, sizeof(tmp), "mc0:%s", path + 3);
		kind = tryCheckExecPath(tmp, &opened_any);
		if (kind > 0)
			return kind;
		snprintf(tmp, sizeof(tmp), "mc1:%s", path + 3);
		kind = tryCheckExecPath(tmp, &opened_any);
		if (kind > 0)
			return kind;
	}

	/*
	 * 5) Last-resort fallback:
	 * if the file opens but header read is unreliable, trust known executable
	 * extensions to avoid false negatives on some device stacks.
	 */
	if (opened_any) {
		fallback_kind = classifyExecByExtension(path);
		if (fallback_kind > 0)
			return fallback_kind;
	}

error:
	return -1;  //return -1 for failed check
}
//------------------------------
//End of func:  int checkELFheader(const char *path)
//--------------------------------------------------------------
// RunLoaderElf loads LOADER.ELF from program memory and passes
// args of selected ELF and partition to it
// Modified version of loader from Independence
//	(C) 2003 Marcus R. Brown <mrbrown@0xd6.org>
//------------------------------
void RunLoaderElf(char *filename, char *party, const char *selected_path, int exec_kind, int reboot_iop_elf_load)
{
#define ELFLOAD_ARGC 3
	u8 *boot_elf;
	elf_header_t *eh;
	elf_pheader_t *eph;
	void *pdata;
	int i;
	char *argv[ELFLOAD_ARGC], bootpath[256];
	static char exec_target[MAX_PATH];
	static char exec_arg0[MAX_PATH];
	const char *handoff_path = NULL;

	if (selected_path != NULL && selected_path[0] != '\0')
		handoff_path = normalizeExecArg0Path(selected_path, exec_arg0, sizeof(exec_arg0));
	snprintf(exec_target, sizeof(exec_target), "%s", filename);
	if (exec_kind == 1 && handoff_path != NULL && !strncmp(handoff_path, "mass", 4))
		snprintf(exec_target, sizeof(exec_target), "%s", handoff_path);
	DPRINTF("RunLoaderElf: exec_kind=%d reboot_iop=%d target='%s' handoff='%s' party='%s'\n",
	        exec_kind, reboot_iop_elf_load, filename,
	        (handoff_path != NULL) ? handoff_path : "",
	        (party != NULL) ? party : "");
	DPRINTF("RunLoaderElf: loader target='%s'\n", exec_target);

	if (exec_kind == 1) {
#ifdef DVRP
		const int is_dvr_pfs = ((!strncmp(party, "dvr_hdd0:", 9)) && (!strncmp(filename, "dvr_pfs0:", 9)));
#endif
		const int is_hdd_pfs = (isHddPartyPath(party) && (!strncmp(filename, "pfs0:", 5)));
		int ret;

		if (is_hdd_pfs) {
			if (0 > fileXioMount("pfs0:", party, FIO_MT_RDONLY)) {
				unmountParty(0);
				if (0 > fileXioMount("pfs0:", party, FIO_MT_RDONLY))
					return;
			}
#ifdef DVRP
		} else if (is_dvr_pfs) {
			if (0 > fileXioMount("dvr_pfs0:", party, FIO_MT_RDONLY)) {
				unmountDVRPParty(0);
				if (0 > fileXioMount("dvr_pfs0:", party, FIO_MT_RDONLY))
					return;
			}
#endif
		}

		DPRINTF("RunLoaderElf: elf-loader2 target='%s' party='%s'\n", exec_target, party);
		ret = LoadELFFromFileWithPartition(exec_target, (party != NULL && party[0] != '\0') ? party : NULL, 0, NULL);
		if (ret < 0 && strcmp(exec_target, filename) != 0) {
			DPRINTF("RunLoaderElf: elf-loader2 retry legacy target='%s' party='%s'\n", filename, party);
			ret = LoadELFFromFileWithPartition(filename, (party != NULL && party[0] != '\0') ? party : NULL, 0, NULL);
		}
		DPRINTF("RunLoaderElf: elf-loader2 returned %d\n", ret);
		exit(126);
	}

	if (isHddPartyPath(party) && (!strncmp(filename, "pfs0:", 5))) {
		if (0 > fileXioMount("pfs0:", party, FIO_MT_RDONLY)) {
			//Some error occurred, it could be due to something else having used pfs0
			unmountParty(0);  //So we try unmounting pfs0, to try again
			if (0 > fileXioMount("pfs0:", party, FIO_MT_RDONLY))
				return;  //If it still fails, we have to give up...
		}

		//If a path to a file on PFS is specified, change it to the standard format.
		//hddN:partition:pfs:path/to/file
		if (strncmp(filename, "pfs0:", 5) == 0) {
			sprintf(bootpath, "%s:pfs:%s", party, &filename[5]);
		} else {
			sprintf(bootpath, "%s:%s", party, filename);
		}

		argv[0] = exec_target;
		if ((handoff_path != NULL) && isHddBrowserPath(handoff_path))
			argv[1] = (char *)handoff_path;
		else
			argv[1] = bootpath;
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
		argv[0] = exec_target;
		if ((handoff_path != NULL) && !strncmp(handoff_path, "dvr_hdd0:/", 10))
			argv[1] = (char *)handoff_path;
		else
			argv[1] = bootpath;
#endif
	} else {
		argv[0] = exec_target;
		argv[1] = (char *)((handoff_path != NULL) ? handoff_path : filename);
	}

	/* NB: LOADER.ELF is embedded  */
	boot_elf = (u8 *)loader_elf;
	eh = (elf_header_t *)boot_elf;
	if (_lw((u32)&eh->ident) != ELF_MAGIC)
		asm volatile("break\n");
	DPRINTF("RunLoaderElf: loader embedded entry=0x%08x phoff=0x%08x phnum=%u\n",
	        eh->entry, eh->phoff, eh->phnum);

	eph = (elf_pheader_t *)(boot_elf + eh->phoff);

	/* Scan through the ELF's program headers and copy them into RAM, then
									zero out any non-loaded regions.  */
	for (i = 0; i < eh->phnum; i++) {
		if (eph[i].type != ELF_PT_LOAD)
			continue;
		DPRINTF("RunLoaderElf: loader phdr[%d] vaddr=%p offset=0x%08x filesz=0x%08x memsz=0x%08x\n",
		        i, eph[i].vaddr, eph[i].offset, eph[i].filesz, eph[i].memsz);

		pdata = (void *)(boot_elf + eph[i].offset);
		memcpy(eph[i].vaddr, pdata, eph[i].filesz);

		if (eph[i].memsz > eph[i].filesz)
			memset(eph[i].vaddr + eph[i].filesz, 0,
			       eph[i].memsz - eph[i].filesz);
	}
	if (exec_kind == 2)
		argv[2] = (reboot_iop_elf_load) ? "-er" : "-enr";
	else
		argv[2] = (reboot_iop_elf_load) ? "-r" : "-nr";
	DPRINTF("RunLoaderElf: loader mode arg='%s'\n", argv[2]);
	if (eh->entry == 0 || (eh->entry & 0x3) != 0) {
		DPRINTF("RunLoaderElf: invalid embedded loader entry=0x%08x\n", eh->entry);
		return;
	}
	/* Let's go.  */
	SifExitRpc();
	FlushCache(0);
	FlushCache(2);

	ExecPS2((void *)eh->entry, NULL, ELFLOAD_ARGC, argv);
}
//------------------------------
//End of func:  void RunLoaderElf(char *filename, char *party, const char *selected_path, int exec_kind, int reboot_iop_elf_load)
//--------------------------------------------------------------
//End of file:  elf.c
//--------------------------------------------------------------
