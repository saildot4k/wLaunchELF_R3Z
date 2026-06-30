//--------------------------------------------------------------
//File name:    elf.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "init.h"
#include <elf.h>
#ifdef XFROM
#include <libsecr.h>
#endif

#define MAX_PATH 1025
#define MBR_PAYLOAD_LOAD_ADDR 0x01000000

extern u8 loader_elf[];
extern int size_loader_elf;

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
	const Elf32_Ehdr *eh;

	if (header_len < SELFMAG)
		return -1;

	/*
	 * Heuristic requested for additional encrypted KELF variants:
	 * byte[0] == 0x01 and byte[2] == 0x00.
	 */
	if (header[0] == 0x01 && header[2] == 0x00)
		return 2;

	if (memcmp(header, ELFMAG, SELFMAG) != 0)
		return -1;

	if (header_len >= (int)sizeof(Elf32_Ehdr)) {
		eh = (const Elf32_Ehdr *)header;
		if ((eh->e_type != ET_EXEC) && (eh->e_type != ET_DYN))
			return -1;
		if (eh->e_machine != EM_MIPS)
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

static int isExplicitHddHandoffPath(const char *path)
{
	return (path != NULL && (isHddBrowserPath(path) || !strncmp(path, "uLE:", 4)));
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
static void RunEmbeddedLoader(int argc, char **argv)
{
	u8 *boot_elf;
	Elf32_Ehdr *eh;
	Elf32_Phdr *eph;
	void *pdata;
	int i;

	/* NB: LOADER.ELF is embedded  */
	boot_elf = (u8 *)loader_elf;
	eh = (Elf32_Ehdr *)boot_elf;
	if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0)
		asm volatile("break\n");
	DPRINTF("RunEmbeddedLoader: loader embedded entry=0x%08x phoff=0x%08x phnum=%u\n",
	        eh->e_entry, eh->e_phoff, eh->e_phnum);

	eph = (Elf32_Phdr *)(boot_elf + eh->e_phoff);

	/* Scan through the ELF's program headers and copy them into RAM, then
									zero out any non-loaded regions.  */
	for (i = 0; i < eh->e_phnum; i++) {
		if (eph[i].p_type != PT_LOAD)
			continue;
		DPRINTF("RunEmbeddedLoader: loader phdr[%d] vaddr=%p offset=0x%08x filesz=0x%08x memsz=0x%08x\n",
		        i, (void *)eph[i].p_vaddr, eph[i].p_offset, eph[i].p_filesz, eph[i].p_memsz);

		pdata = (void *)(boot_elf + eph[i].p_offset);
		memcpy((void *)eph[i].p_vaddr, pdata, eph[i].p_filesz);

		if (eph[i].p_memsz > eph[i].p_filesz)
			memset((void *)(eph[i].p_vaddr + eph[i].p_filesz), 0,
			       eph[i].p_memsz - eph[i].p_filesz);
	}
	if (eh->e_entry == 0 || (eh->e_entry & 0x3) != 0) {
		DPRINTF("RunEmbeddedLoader: invalid embedded loader entry=0x%08x\n", eh->e_entry);
		return;
	}
	/* Let's go.  */
	SifExitRpc();
	FlushCache(0);
	FlushCache(2);

	ExecPS2((void *)eh->e_entry, NULL, argc, argv);
}
//--------------------------------------------------------------
//End of func:  void RunEmbeddedLoader(int argc, char **argv)
//--------------------------------------------------------------
void RunLoaderMemory(const char *arg0, const char *mem_arg, int reboot_iop)
{
#define MEMLOAD_ARGC 3
	char *argv[MEMLOAD_ARGC];
	static char loader_arg[8];

	snprintf(loader_arg, sizeof(loader_arg), "%s", (reboot_iop) ? "-la=ER" : "-la=E");
	argv[0] = (char *)arg0;
	argv[1] = (char *)mem_arg;
	argv[2] = loader_arg;

	RunEmbeddedLoader(MEMLOAD_ARGC, argv);
}
//--------------------------------------------------------------
//End of func:  void RunLoaderMemory(const char *arg0, const char *mem_arg, int reboot_iop)
//--------------------------------------------------------------
#ifdef XFROM
#define WLE_APA_MAGIC 0x00415041
#define WLE_HDD_SECTOR_SIZE 512
#define WLE_MBR_READ_CHUNK_SECTORS 64
#define WLE_OSDMBR_MAGIC "Sony Computer Entertainment Inc."

typedef struct
{
	u32 checksum;
	u32 magic;
	u32 next;
	u32 prev;
	char id[APA_IDMAX];
	char rpwd[APA_PASSMAX];
	char fpwd[APA_PASSMAX];
	u32 start;
	u32 length;
	u16 type;
	u16 flags;
	u32 nsub;
	u8 created[8];
	u32 main;
	u32 number;
	u32 modver;
	u32 padding1[7];
	char padding2[128];
	struct
	{
		char magic[32];
		u32 version;
		u32 nsector;
		u8 created[8];
		u32 osdStart;
		u32 osdSize;
	} mbr;
} wle_apa_mbr_header_t;

static int isXfromMbrLaunchPath(const char *path)
{
	return (path != NULL &&
	        !stricmp(path, "xfrom:/BIEXEC-SYSTEM/xosdmain.elf"));
}

static int isHddSystemMbrLaunchPath(const char *path)
{
	return (isHddPartyPath(path) &&
	        !stricmp(path + 5, "__system:pfs:/BIEXEC-SYSTEM/xosdmain.elf"));
}

static int isRawHddMbrLaunchPath(const char *path)
{
	const char *suffix;

	if (!isHddPartyPath(path))
		return 0;

	suffix = path + 5;
	return (!stricmp(suffix, "__mbr") ||
	        !stricmp(suffix, "/__mbr"));
}

int IsMbrLaunchPath(const char *path)
{
	return (isXfromMbrLaunchPath(path) ||
	        isHddSystemMbrLaunchPath(path) ||
	        isRawHddMbrLaunchPath(path));
}

int MbrLaunchRequiresPsx(const char *path)
{
	return (isXfromMbrLaunchPath(path) || isHddSystemMbrLaunchPath(path));
}

static int getRawHddMbrDevice(const char *path, char *device, size_t device_size)
{
	if (!isRawHddMbrLaunchPath(path) || device == NULL || device_size < 6)
		return -1;

	snprintf(device, device_size, "hdd%c:", path[3]);
	return 0;
}

static int readHddSectors(const char *device, u32 lba, u32 count, void *buf)
{
	u8 transfer_buf[sizeof(hddAtaTransfer_t)] __attribute__((aligned(64)));
	hddAtaTransfer_t *transfer = (hddAtaTransfer_t *)transfer_buf;

	if (device == NULL || buf == NULL || count == 0)
		return -1;

	transfer->lba = lba;
	transfer->size = count;
	return fileXioDevctl(device, APA_DEVCTL_ATA_READ,
	                     transfer, sizeof(transfer_buf),
	                     buf, count * WLE_HDD_SECTOR_SIZE);
}

static int readHddMbrHeader(const char *device, wle_apa_mbr_header_t *header)
{
	u8 sector[WLE_HDD_SECTOR_SIZE] __attribute__((aligned(64)));
	int ret;

	if (header == NULL)
		return -1;

	ret = readHddSectors(device, 0, 1, sector);
	if (ret < 0)
		return -1;

	memcpy(header, sector, sizeof(*header));
	if (header->magic != WLE_APA_MAGIC ||
	    header->type != APA_TYPE_MBR ||
	    strncmp(header->id, "__mbr", APA_IDMAX) ||
	    strncmp(header->mbr.magic, WLE_OSDMBR_MAGIC, sizeof(header->mbr.magic)) ||
	    header->mbr.osdStart == 0 ||
	    header->mbr.osdSize == 0)
		return -1;

	return 0;
}

static int getRawHddMbrPayloadInfo(const char *path, char *device, size_t device_size,
                                   u32 *start_sector, u32 *sector_count)
{
	wle_apa_mbr_header_t header;
	iox_stat_t stat;
	char part_path[16];
	u64 part_start, part_end, payload_end;

	if (getRawHddMbrDevice(path, device, device_size) < 0)
		return -1;

	if (!loadHddModules())
		return -1;

	snprintf(part_path, sizeof(part_path), "%s__mbr", device);
	if (fileXioGetStat(part_path, &stat) < 0 || stat.mode != APA_TYPE_MBR)
		return -1;

	if (readHddMbrHeader(device, &header) < 0)
		return -1;

	part_start = stat.private_5;
	part_end = part_start + stat.size;
	payload_end = (u64)header.mbr.osdStart + header.mbr.osdSize;
	if ((u64)header.mbr.osdStart < part_start || payload_end > part_end)
		return -1;

	*start_sector = header.mbr.osdStart;
	*sector_count = header.mbr.osdSize;
	return 0;
}

int GetHddMbrPayloadSize(const char *path, u32 *payload_size)
{
	char device[6];
	u32 start_sector, sector_count;

	if (payload_size == NULL)
		return -1;

	if (getRawHddMbrPayloadInfo(path, device, sizeof(device), &start_sector, &sector_count) < 0)
		return -1;

	(void)start_sector;
	if (sector_count > (0xFFFFFFFFu / WLE_HDD_SECTOR_SIZE))
		return -1;

	*payload_size = sector_count * WLE_HDD_SECTOR_SIZE;
	return 0;
}

static int isElfPayload(const u8 *payload, int payload_size)
{
	const Elf32_Ehdr *eh;

	if (payload == NULL || payload_size < (int)sizeof(Elf32_Ehdr))
		return 0;

	eh = (const Elf32_Ehdr *)payload;
	return (memcmp(eh->e_ident, ELFMAG, SELFMAG) == 0 &&
	        ((eh->e_type == ET_EXEC) || (eh->e_type == ET_DYN)) &&
	        eh->e_machine == EM_MIPS);
}

static int isLikelyEncryptedPayload(const u8 *payload, int payload_size)
{
	return (payload != NULL && payload_size >= 4 && payload[0] == 0x01 && payload[2] == 0x00);
}

static int getMbrOpenPath(const char *path, char *open_path, size_t open_path_size)
{
	const char *pfs;
	char party[MAX_PATH];
	size_t party_len;
	int party_ix;

	if (path == NULL || open_path == NULL || open_path_size == 0)
		return -1;

	if (!strncmp(path, "xfrom:", 6)) {
		if (!loadFlashModules())
			return -1;
		snprintf(open_path, open_path_size, "%s", path);
		return 0;
	}

	if (strncmp(path, "hdd", 3) || path[3] < '0' || path[3] > '9' || path[4] != ':')
		return -1;

	if (!loadHddModules())
		return -1;

	pfs = strstr(path, ":pfs:");
	if (pfs == NULL)
		return -1;

	party_len = (size_t)(pfs - path);
	if (party_len == 0 || party_len >= sizeof(party))
		return -1;

	memcpy(party, path, party_len);
	party[party_len] = '\0';

	party_ix = mountParty(party);
	if (party_ix < 0)
		return -1;

	snprintf(open_path, open_path_size, "pfs%d:%s", party_ix, pfs + 5);
	return 0;
}

static int readRawHddMbrPayload(const char *path, u8 *payload, u32 payload_capacity, int *payload_size)
{
	char device[6];
	u32 start_sector, sector_count, sector;
	u32 chunk, copied;
	u64 payload_bytes;

	if (payload == NULL || payload_size == NULL)
		return -1;

	if (getRawHddMbrPayloadInfo(path, device, sizeof(device), &start_sector, &sector_count) < 0)
		return -1;

	payload_bytes = (u64)sector_count * WLE_HDD_SECTOR_SIZE;
	if (payload_bytes == 0 ||
	    payload_bytes > payload_capacity ||
	    payload_bytes > 0x7FFFFFFFu)
		return -1;

	copied = 0;
	sector = 0;
	while (sector < sector_count) {
		chunk = sector_count - sector;
		if (chunk > WLE_MBR_READ_CHUNK_SECTORS)
			chunk = WLE_MBR_READ_CHUNK_SECTORS;

		if (readHddSectors(device, start_sector + sector, chunk, payload + copied) < 0)
			return -1;

		copied += chunk * WLE_HDD_SECTOR_SIZE;
		sector += chunk;
	}

	*payload_size = (int)payload_bytes;
	return 0;
}

int PrepareMbrLaunchPayload(const char *path, char *mem_arg, size_t mem_arg_size)
{
	char open_path[MAX_PATH];
	u8 *payload;
	u8 *launch_payload;
	s64 payload_size64;
	u32 ee_mem_end;
	int payload_size;
	int encrypted_payload;
	int fd, total, rd;

	if (path == NULL || mem_arg == NULL || mem_arg_size < 22)
		return -1;

	ee_mem_end = GetMemorySize();
	if (ee_mem_end <= MBR_PAYLOAD_LOAD_ADDR)
		return -1;

	payload = (u8 *)MBR_PAYLOAD_LOAD_ADDR;
	if (isRawHddMbrLaunchPath(path)) {
		if (readRawHddMbrPayload(path, payload, ee_mem_end - MBR_PAYLOAD_LOAD_ADDR, &payload_size) < 0)
			return -1;
	} else {
		if (getMbrOpenPath(path, open_path, sizeof(open_path)) < 0)
			return -1;

		fd = genOpen(open_path, FIO_O_RDONLY);
		if (fd < 0)
			return -1;

		payload_size64 = genLseek(fd, 0, SEEK_END);
		if (payload_size64 <= 0 ||
		    payload_size64 > (s64)(ee_mem_end - MBR_PAYLOAD_LOAD_ADDR)) {
			genClose(fd);
			return -1;
		}
		genLseek(fd, 0, SEEK_SET);

		payload_size = (int)payload_size64;
		total = 0;
		while (total < payload_size) {
			rd = genRead(fd, payload + total, payload_size - total);
			if (rd <= 0)
				break;
			total += rd;
		}
		genClose(fd);

		if (total != payload_size)
			return -1;
	}

	launch_payload = payload;
	encrypted_payload = isLikelyEncryptedPayload(payload, payload_size);
	if (!isElfPayload(payload, payload_size) && encrypted_payload) {
		void *decrypted_payload;

		if (!loadSecrSifModule())
			return -1;

		if (!SecrInit())
			return -1;
		decrypted_payload = SecrDiskBootFile(payload);
		SecrDeinit();

		if (decrypted_payload == NULL)
			return -1;

		launch_payload = (u8 *)decrypted_payload;
		if (launch_payload < payload || launch_payload >= payload + payload_size)
			return -1;
		payload_size -= (int)(launch_payload - payload);
	}

	snprintf(mem_arg, mem_arg_size, "mem:%08X:%08X", (u32)launch_payload, (u32)payload_size);
	return 0;
}
//--------------------------------------------------------------
//End of func:  int PrepareMbrLaunchPayload(const char *path, char *mem_arg, size_t mem_arg_size)
//--------------------------------------------------------------
#endif
//--------------------------------------------------------------
// RunLoaderElf loads LOADER.ELF from program memory and passes
// args of selected ELF and partition to it
// Modified version of loader from Independence
//	(C) 2003 Marcus R. Brown <mrbrown@0xd6.org>
//------------------------------
void RunLoaderElf(char *filename, char *party, const char *selected_path, int exec_kind, int reboot_iop_elf_load)
{
#define ELFLOAD_ARGC 3
	char *argv[ELFLOAD_ARGC], bootpath[256];
	static char exec_target[MAX_PATH];
	static char exec_arg0[MAX_PATH];
	static char loader_arg[8];
	const char *handoff_path = NULL;
#ifdef DVRP
	int dvr_pfs_ix = -1;
	char dvr_pfs_name[10] = "dvr_pfs0:";
#endif

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
#ifdef DVRP
	dvr_pfs_ix = (party != NULL) ? getDVRPPartyMountIndex(party) : -1;
	if (dvr_pfs_ix >= 0)
		dvr_pfs_name[7] = '0' + dvr_pfs_ix;
#endif

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
		if (isExplicitHddHandoffPath(handoff_path))
			argv[1] = (char *)handoff_path;
		else
			argv[1] = bootpath;
#ifdef DVRP
	} else if (dvr_pfs_ix >= 0 && !strncmp(filename, dvr_pfs_name, 9)) {
		if (0 > fileXioMount(dvr_pfs_name, party, FIO_MT_RDONLY)) {
			//Some error occurred, it could be due to something else having used pfs0
			unmountDVRPParty(dvr_pfs_ix);  //So we try unmounting pfs, to try again
			if (0 > fileXioMount(dvr_pfs_name, party, FIO_MT_RDONLY))
				return;  //If it still fails, we have to give up...
		}

		//If a path to a file on PFS is specified, change it to the standard format.
		//dvr_hdd0:partition:pfs:path/to/file
		if (strncmp(filename, dvr_pfs_name, 9) == 0) {
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

	(void)exec_kind;
	snprintf(loader_arg, sizeof(loader_arg), "%s", (reboot_iop_elf_load) ? "-la=AR" : "-la=A");
	argv[2] = loader_arg;
	DPRINTF("RunLoaderElf: loader mode arg='%s'\n", argv[2]);

	RunEmbeddedLoader(ELFLOAD_ARGC, argv);
}
//------------------------------
//End of func:  void RunLoaderElf(char *filename, char *party, const char *selected_path, int exec_kind, int reboot_iop_elf_load)
//--------------------------------------------------------------
//End of file:  elf.c
//--------------------------------------------------------------
