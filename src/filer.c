//--------------------------------------------------------------
//File name:   filer.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "filer_copy.h"
#include "gui_hdd0_format.h"
#include "gui_sort.h"
#include "filer_shared.h"
#include "main_startup.h"

#define MC_ATTR_norm_folder 0x8427  //Normal folder on PS2 MC
#define MC_ATTR_norm_file 0x8497    //file (PS2/PS1) on PS2 MC

static void clearMcTable(sceMcTblGetDir *mcT)
{
	memset((void *)mcT, 0, sizeof(sceMcTblGetDir));
}

enum {
	TRANSFER_STORAGE_DEFAULT = 0,
	TRANSFER_STORAGE_MMCE,
	TRANSFER_STORAGE_MX4SIO
};

enum {
	TRANSFER_PSX_HDD_NONE = 0,
	TRANSFER_PSX_HDD_ATA,
	TRANSFER_PSX_HDD_DVR
};

#if defined(ETH) || defined(UDPFS)
extern int host_error;
#endif

static int transferStorageGateForPath(const char *path)
{
	if (path == NULL)
		return TRANSFER_STORAGE_DEFAULT;
#ifdef MMCE
	if (!strncmp(path, "mmce", 4))
		return TRANSFER_STORAGE_MMCE;
#endif
#ifdef MX4SIO
	if (!strncmp(path, "mx4sio", 6))
		return TRANSFER_STORAGE_MX4SIO;
#endif
	return TRANSFER_STORAGE_DEFAULT;
}

static int transferPsxHddGateForPath(const char *path)
{
	if (path == NULL)
		return TRANSFER_PSX_HDD_NONE;
#ifdef DVRP
	if (!strncmp(path, "dvr_hdd", 7) || !strncmp(path, "dvr_pfs", 7))
		return TRANSFER_PSX_HDD_DVR;
#endif
	if (!strncmp(path, "hdd", 3) || !strncmp(path, "pfs", 3))
		return TRANSFER_PSX_HDD_ATA;
	return TRANSFER_PSX_HDD_NONE;
}

int ensurePathDeviceStackReady(const char *path)
{
	if (path == NULL || path[0] == '\0')
		return TRUE;
	if (!strncmp(path, "mc", 2) || !strncmp(path, "vmc", 3))
		return TRUE;
	if (!strncmp(path, "mass", 4) || !strncmp(path, "usb", 3)) {
		loadUsbModules();
		return (USB_mass_loaded != 0);
	}
#ifdef MMCE
	if (!strncmp(path, "mmce", 4)) {
		return loadMmceModules();
	}
#endif
#ifdef MX4SIO
	if (!strncmp(path, "mx4sio", 6))
		return loadMx4sioModules();
#endif
#ifdef EXFAT
	if (!strncmp(path, "ata", 3)) {
		return loadAtaModules();
	}
#endif
	if (!strncmp(path, "hdd", 3) || !strncmp(path, "pfs", 3)) {
		return loadHddModules();
	}
#ifdef DVRP
	if (!strncmp(path, "dvr_hdd", 7) || !strncmp(path, "dvr_pfs", 7)) {
		return loadDVRPHddModules();
	}
#endif
	if (!strncmp(path, "cdfs", 4)) {
		loadCdModules();
		return TRUE;
	}
#ifdef ETH
	if (!strncmp(path, "host", 4)) {
		initHOST();
		return !host_error;
	}
#endif
#ifdef UDPFS
	if (!strncmp(path, "udpfs", 5)) {
		return load_udpfs();
	}
#endif
	return TRUE;
}

int prepareTransferDeviceStacks(const char *src_path, const char *dst_path)
{
	int src_storage = transferStorageGateForPath(src_path);
	int dst_storage = transferStorageGateForPath(dst_path);
	int src_psx_hdd = transferPsxHddGateForPath(src_path);
	int dst_psx_hdd = transferPsxHddGateForPath(dst_path);

	if (src_storage != TRANSFER_STORAGE_DEFAULT &&
	    dst_storage != TRANSFER_STORAGE_DEFAULT &&
	    src_storage != dst_storage)
		return TRANSFER_STACK_INCOMPATIBLE;

	if (src_psx_hdd != TRANSFER_PSX_HDD_NONE &&
	    dst_psx_hdd != TRANSFER_PSX_HDD_NONE &&
	    src_psx_hdd != dst_psx_hdd)
		return TRANSFER_STACK_INCOMPATIBLE;

	if (!ensurePathDeviceStackReady(src_path))
		return TRANSFER_STACK_LOAD_FAILED;
	if (!ensurePathDeviceStackReady(dst_path))
		return TRANSFER_STACK_LOAD_FAILED;
	return TRANSFER_STACK_READY;
}

int PasteProgress_f = 0;   //Flags progress report having been made in Pasting
int PasteMode;             //Top-level PasteMode flag
int PM_flag[MAX_RECURSE];  //PasteMode flag for each 'copy' recursion level
int PM_file[MAX_RECURSE];  //PasteMode attribute file descriptors

char mountedParty[MOUNT_LIMIT][MAX_NAME];
int latestMount = -1;
int vmcMounted[2] = {0, 0};                          //flags true for mounted VMC false for unmounted
int vmc_PartyIndex[2] = {-1, -1};                    //PFS index for each VMC, unless -1
int Party_vmcIndex[MOUNT_LIMIT] = {-1, -1, -1, -1};  //VMC for each PFS, unless -1
unsigned char *elisaFnt = NULL;
int elisa_failed = FALSE;  //Set at failure to load font, cleared at browser entry
u64 freeSpace;
int mcfreeSpace;
int mctype_PSx;  //dlanor: Needed for proper scaling of mcfreespace
int vfreeSpace;  //flags validity of freespace value
int browser_cut;
int nclipFiles, nmarks, nparties;
#ifdef DVRP
int ndvrpparties;
char mountedDVRPParty[MOUNT_LIMIT][MAX_NAME];
int latestDVRPMount = -1;
#endif

int file_show = 1;  //dlanor: 0==name_only, 1==name+size+time, 2==title+size+time
int file_sort = 1;  //dlanor: 0==none, 1==name, 2==title, 3==mtime
int size_valid = 0;
int time_valid = 0;
char parties[MAX_PARTITIONS][MAX_PART_NAME+1];
char clipPath[MAX_PATH], LastDir[MAX_NAME], marks[MAX_ENTRY];
FILEINFO clipFiles[MAX_ENTRY];
int fileMode = FIO_S_IRUSR | FIO_S_IWUSR | FIO_S_IXUSR | FIO_S_IRGRP | FIO_S_IWGRP | FIO_S_IXGRP | FIO_S_IROTH | FIO_S_IWOTH | FIO_S_IXOTH;

#ifndef FILEOP_TRACE
#define FILEOP_TRACE 1
#endif

char cnfmode_extU[CNFMODE_CNT][4] = {
    "*",    // cnfmode FALSE
    "ELF",  // cnfmode TRUE
    "IRX",  // cnfmode USBD_IRX_CNF
    "IRX",  // cnfmode USBKBD_IRX_CNF
    "KBD",  // cnfmode KBDMAP_FILE_CNF
    "CNF",  // cnfmode CNF_PATH_CNF
    "*",    // cnfmode TEXT_CNF
    "",     // cnfmode DIR_CNF
    "IRX",  // cnfmode USBMASS_IRX_CNF
    "LNG",  // cnfmode LANG_CNF
    "FNT",  // cnfmode FONT_CNF
    "*"     // cnfmode SAVE_CNF
};

char cnfmode_extL[CNFMODE_CNT][4] = {
    "*",    // cnfmode FALSE
    "elf",  // cnfmode TRUE
    "irx",  // cnfmode USBD_IRX_CNF
    "irx",  // cnfmode USBKBD_IRX_CNF
    "kbd",  // cnfmode KBDMAP_FILE_CNF
    "cnf",  // cnfmode CNF_PATH_CNF
    "*",    // cnfmode TEXT_CNF
    "",     // cnfmode DIR_CNF
    "irx",  // cnfmode USBMASS_IRX_CNF
    "lng",  // cnfmode LANG_CNF
    "fnt",  // cnfmode FONT_CNF
    "*"     // cnfmode SAVE_CNF
};
#if defined(ETH) || defined(UDPFS)
int host_ready = 0;
int host_error = 0;
int host_elflist = 0;
int host_use_Bsl = 1;  //By default assume that host paths use backslash
#endif
u64 written_size;            //Used for pasting progress report
u64 PasteTime;               //Used for pasting progress report

int PSU_content;  //Used to count PSU content headers for the main header

//USB_mass definitions for multiple drive usage
char USB_mass_ix[10] = {'0', 0, 0, 0, 0, 0, 0, 0, 0, 0};
int USB_mass_max_drives = USB_MASS_MAX_DRIVES;
u64 USB_mass_scan_time = 0;
int USB_mass_scanned = 0;  //0==Not_found_OR_No_Multi 1==found_Multi_mass_once
int USB_mass_loaded = 0;   //0==none, 1==internal, 2==external

static int mapUsbPathToMassPath(const char *usb_path, char *mass_path)
{
	const char *suffix;
	int unit = 0;

	if (strncmp(usb_path, "usb", 3))
		return -1;

	if (usb_path[3] == ':')
		suffix = usb_path + 4;
	else if (usb_path[3] >= '0' && usb_path[3] <= '9' && usb_path[4] == ':') {
		unit = usb_path[3] - '0';
		suffix = usb_path + 5;
	} else
		return -1;

	if (*suffix == 0)
		suffix = "/";

	if (unit == 0)
		snprintf(mass_path, MAX_PATH, "mass:%s", suffix);
	else
		snprintf(mass_path, MAX_PATH, "mass%d:%s", unit, suffix);

	return 0;
}

//char debugs[4096]; //For debug display strings. Comment it out when unused
//--------------------------------------------------------------
//executable code
//--------------------------------------------------------------
int mountParty(const char *party)
{
	int i, j;
	char pfs_str[6];

	for (i = 0; i < MOUNT_LIMIT; i++) {  //Here we check already mounted PFS indexes
		if (!strcmp(party, mountedParty[i]))
			goto return_i;
	}

	for (i = 0, j = -1; i < MOUNT_LIMIT; i++) {  //Here we search for a free PFS index
		if (mountedParty[i][0] == 0) {
			j = i;
			break;
		}
	}

	if (j == -1) {  //Here we search for a suitable PFS index to unmount
		for (i = 0; i < MOUNT_LIMIT; i++) {
			if ((i != latestMount) && (Party_vmcIndex[i] < 0)) {
				j = i;
				break;
			}
		}
		if (j < 0)
			return -1;  //no usable mountpoint available
		unmountParty(j);
	}
	//Here j is the index of a free PFS mountpoint
	//But 'free' only means that the main uLE program isn't using it
	//If the ftp server is running, that may have used the mountpoints

	//RA NB: The old code to reclaim FTP partitions was seriously bugged...

	i = j;
	strcpy(pfs_str, "pfs0:");

	pfs_str[3] = '0' + i;
		if (fileXioMount(pfs_str, party, FIO_MT_RDWR) < 0) {                 //if FTP stole it
			for (i = 0; i < MOUNT_LIMIT; i++) {                               //for loop to kill FTP partition mountpoints
				if ((i != latestMount) && (Party_vmcIndex[i] < 0)) {  //if unneeded by uLE
					unmountParty(i);                                  //unmount partition mountpoint
					pfs_str[3] = '0' + i;                             //prepare to reuse that mountpoint
					if (fileXioMount(pfs_str, party, FIO_MT_RDWR) >= 0)
						break;  //break from the loop on successful mount
				}               //ends if unneeded by uLE
			}                   //ends for loop to kill FTP partition mountpoints
			//Here i indicates what happened above with the following meanings:
			//0..MOUNT_LIMIT-1==Success, MOUNT_LIMIT==Failure
			if (i >= MOUNT_LIMIT)
				return -1;
		}  //ends if clause for mountpoints stolen by FTP
	strcpy(mountedParty[i], party);
return_i:
	latestMount = i;
	return i;
}
//--------------------------------------------------------------
void unmountParty(int party_ix)
{
	char pfs_str[6];

	strcpy(pfs_str, "pfs0:");
	pfs_str[3] += party_ix;
	if (fileXioUmount(pfs_str) < 0)
		return;  //leave variables unchanged if unmount failed (remember true state)
	if (party_ix < MOUNT_LIMIT) {
		mountedParty[party_ix][0] = 0;
	}
	if (latestMount == party_ix)
		latestMount = -1;
}
#ifdef DVRP
//--------------------------------------------------------------
// The above modified for the DVRP.
//--------------------------------------------------------------
int mountDVRPParty(const char *party)
{
	int i;

	for (i = 0; i < MOUNT_LIMIT; i++) {  //Here we check already mounted PFS indexes
		if (!strcmp(party, mountedDVRPParty[i]))
			goto return_i;
	}

	if (strcmp(party, "dvr_hdd0:__xdata") == 0) {
		i = 1;
	} else if (strcmp(party, "dvr_hdd0:__xcontents") == 0) {
		i = 0;
	} else {
		return -1;
	}
	strcpy(mountedDVRPParty[i], party);
return_i:
	latestDVRPMount = i;
	return i;
}
//--------------------------------------------------------------
void unmountDVRPParty(int party_ix)
{
	if (party_ix < MOUNT_LIMIT) {
		mountedDVRPParty[party_ix][0] = 0;
	}
	if (latestDVRPMount == party_ix)
		latestDVRPMount = -1;
}
//--------------------------------------------------------------
#endif
//--------------------------------------------------------------
// unmountAll can unmount all mountpoints from 0 to MOUNT_LIMIT,
// but unlike the individual unmountParty, it will only do so
// for mountpoints indicated as used by the matching string in
// the string array 'mountedParty'.
// From v4.23 this routine is also used to unmount VMC devices
//------------------------------
void unmountAll(void)
{
	char pfs_str[6];
#ifdef DVRP
	char dvr_pfs_str[10];
#endif
	char vmc_str[6];
	int i;

	strcpy(vmc_str, "vmc0:");
	for (i = 0; i < 2; i++) {
		if (vmcMounted[i]) {
			vmc_str[3] = '0' + i;
			fileXioUmount(vmc_str);
			vmcMounted[i] = 0;
			vmc_PartyIndex[i] = -1;
		}
	}

	strcpy(pfs_str, "pfs0:");
	for (i = 0; i < MOUNT_LIMIT; i++) {
		Party_vmcIndex[i] = -1;
		if (mountedParty[i][0] != 0) {
			pfs_str[3] = '0' + i;
			fileXioUmount(pfs_str);
			mountedParty[i][0] = 0;
		}
	}
	latestMount = -1;
#ifdef DVRP
	strcpy(dvr_pfs_str, "dvr_pfs0:");
	for (i = 0; i < MOUNT_LIMIT; i++) {
		if (mountedDVRPParty[i][0] != 0) {
			dvr_pfs_str[7] = '0' + i;
			fileXioUmount(dvr_pfs_str);
			mountedDVRPParty[i][0] = 0;
		}
	}
	latestDVRPMount = -1;
	#endif
}  //ends unmountAll
//--------------------------------------------------------------
static int getHddPartyFromPath(const char *path, char *party, size_t party_size)
{
	const char *start;
	const char *end;

	if (strncmp(path, "hdd0:/", 6))
		return 0;
	start = path + 6;
	if (start[0] == '\0')
		return 0;
	end = strchr(start, '/');
	if (end == NULL)
		end = start + strlen(start);
	if (end <= start)
		return 0;
	snprintf(party, party_size, "hdd0:%.*s", (int)(end - start), start);
	return 1;
}

static int clipboardUsesHddParty(const char *party)
{
	char clipParty[MAX_NAME];

	if (nclipFiles <= 0 || clipPath[0] == '\0')
		return 0;
	if (!getHddPartyFromPath(clipPath, clipParty, sizeof(clipParty)))
		return 0;
	return (strcmp(clipParty, party) == 0);
}

static void unmountHddPartiesNotNeededByClipboard(void)
{
	int i;

	for (i = 0; i < MOUNT_LIMIT; i++) {
		if (mountedParty[i][0] == 0)
			continue;
		/*
		 * Keep the source partition mounted only when clipboard entries still come
		 * from that partition. Otherwise release mounts while at hdd0:/.
		 */
		if (clipboardUsesHddParty(mountedParty[i]))
			continue;
		if (Party_vmcIndex[i] >= 0)
			continue;
		unmountParty(i);
	}
}
//--------------------------------------------------------------
void invalidatePartitionCaches(void)
{
	nparties = 0;
#ifdef DVRP
	ndvrpparties = 0;
#endif
}
//--------------------------------------------------------------
int readMC(const char *path, FILEINFO *info, int max)
{
	static sceMcTblGetDir mcDir[MAX_ENTRY] __attribute__((aligned(64)));
	char dir[MAX_PATH];
	int i, j, ret;

	mcSync(0, NULL, NULL);

	mcGetInfo(path[2] - '0', 0, &mctype_PSx, NULL, NULL);
	mcSync(0, NULL, &ret);
	if (mctype_PSx == 2)  //PS2 MC ?
		time_valid = 1;
	size_valid = 1;

	strcpy(dir, &path[4]);
	strcat(dir, "*");
	mcGetDir(path[2] - '0', 0, dir, 0, MAX_ENTRY - 2, mcDir);
	mcSync(0, NULL, &ret);

	for (i = j = 0; i < ret; i++) {
		if (mcDir[i].AttrFile & sceMcFileAttrSubdir &&
		    (!strcmp((char *)mcDir[i].EntryName, ".") || !strcmp((char *)mcDir[i].EntryName, "..")))
			continue;  //Skip pseudopaths "." and ".."
		strcpy(info[j].name, (char *)mcDir[i].EntryName);
		info[j].stats = mcDir[i];
		j++;
	}

	return j;
}
//------------------------------
//endfunc readMC
//--------------------------------------------------------------
#ifndef LIBCDVD_LEGACY
int readCD(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t record;
	int n = 0, dd = -1;
	u64 wait_start;

	if (sceCdGetDiskType() <= SCECdUNKNOWN) {
		wait_start = Timer();
		while ((Timer() < wait_start + 500) && !uLE_cdDiscValid()) {
			if (cdmode == SCECdNODISC)
				return 0;
		}
		if (cdmode == SCECdNODISC)
			return 0;
		if ((cdmode < SCECdPSCD) || (cdmode > SCECdPS2DVD)) {
			uLE_cdStop();
			return 0;
		}
	}
	if ((dd = fileXioDopen(path)) < 0)
		goto exit;  //exit if error opening directory
	while (fileXioDread(dd, &record) > 0) {
		if ((FIO_S_ISDIR(record.stat.mode)) && (!strcmp(record.name, ".") || !strcmp(record.name, "..")))
			continue;  //Skip entry if pseudo-folder "." or ".."
		strcpy(info[n].name, record.name);
		clearMcTable(&info[n].stats);
		if (FIO_S_ISDIR(record.stat.mode)) {
			info[n].stats.AttrFile = MC_ATTR_norm_folder;
		} else if (FIO_S_ISREG(record.stat.mode)) {
			info[n].stats.AttrFile = MC_ATTR_norm_file;
			info[n].stats.FileSizeByte = record.stat.size;
			info[n].stats.Reserve2 = 0;
		} else
			continue;  //Skip entry which is neither a file nor a folder
		snprintf((char *)info[n].stats.EntryName, 32, "%.31s", info[n].name);
		memcpy((void *)&info[n].stats._Create, record.stat.ctime, 8);
		memcpy((void *)&info[n].stats._Modify, record.stat.mtime, 8);
		n++;
		if (n == max)
			break;
	}  //ends while
	size_valid = 1;

exit:
	if (dd >= 0)
		fileXioDclose(dd);  //Close directory if opened above
	return n;
}
#else
#include "snippets/readCD.h"
#endif
//------------------------------
//endfunc readCD
//--------------------------------------------------------------
void setPartyList(void)
{
	iox_dirent_t dirEnt;
	int hddFd;

	nparties = 0;

	if ((hddFd = fileXioDopen("hdd0:")) < 0)
		return;
	while (fileXioDread(hddFd, &dirEnt) > 0) {
		if (nparties >= MAX_PARTITIONS)
			break;
		if ((dirEnt.stat.attr != ATTR_MAIN_PARTITION) || (dirEnt.stat.mode != FS_TYPE_PFS))
			continue;

		//Patch this to see if new CB versions use valid PFS format
		//NB: All CodeBreaker versions up to v9.3 use invalid formats
		/*	if(!strncmp(dirEnt.name, "PP.",3)){
			int len = strlen(dirEnt.name);
			if(!strcmp(dirEnt.name+len-4, ".PCB"))
				continue;
		}

		if(!strncmp(dirEnt.name, "__", 2) &&
			strcmp(dirEnt.name, "__boot") &&
			strcmp(dirEnt.name, "__net") &&
			strcmp(dirEnt.name, "__system") &&
			strcmp(dirEnt.name, "__sysconf") &&
			strcmp(dirEnt.name, "__contents") &&   // this is where PSBBN used to store it's downloaded contents. Adding it is useful.
			strcmp(dirEnt.name, "__common"))
			continue;
	*/
		{
			size_t len = 0;
			while ((len < MAX_PART_NAME) && (dirEnt.name[len] != '\0')) {
				len++;
			}
			memcpy(parties[nparties], dirEnt.name, len);
			parties[nparties][len] = '\0';
			nparties++;
		}
	}
	fileXioDclose(hddFd);
}
#ifdef DVRP
void setDVRPPartyList(void)
{
	iox_dirent_t dirEnt;
	int hddFd;

	ndvrpparties = 0;

	if ((hddFd = fileXioDopen("dvr_hdd0:")) < 0)
		return;
	while (fileXioDread(hddFd, &dirEnt) > 0) {
		if (ndvrpparties >= MAX_PARTITIONS)
			break;
		if ((dirEnt.stat.attr != ATTR_MAIN_PARTITION) || (dirEnt.stat.mode != FS_TYPE_PFS))
			continue;

		//Patch this to see if new CB versions use valid PFS format
		//NB: All CodeBreaker versions up to v9.3 use invalid formats
		/*	if(!strncmp(dirEnt.name, "PP.",3)){
			int len = strlen(dirEnt.name);
			if(!strcmp(dirEnt.name+len-4, ".PCB"))
				continue;
		}
		if(!strncmp(dirEnt.name, "__", 2) &&
			strcmp(dirEnt.name, "__boot") &&
			strcmp(dirEnt.name, "__net") &&
			strcmp(dirEnt.name, "__system") &&
			strcmp(dirEnt.name, "__sysconf") &&
			strcmp(dirEnt.name, "__contents") &&   // this is where PSBBN used to store it's downloaded contents. Adding it is useful.
			strcmp(dirEnt.name, "__common"))
			continue;
	*/
		{
			size_t len = 0;
			while ((len < MAX_PART_NAME) && (dirEnt.name[len] != '\0')) {
				len++;
			}
			memcpy(parties[ndvrpparties], dirEnt.name, len);
			parties[ndvrpparties][len] = '\0';
			ndvrpparties++;
		}
	}
	fileXioDclose(hddFd);
}
//--------------------------------------------------------------
#endif

//--------------------------------------------------------------
// The following group of file handling functions are used to allow
// the main program to access files without having to deal with the
// difference between device-specific needs directly in each call.
// Even so, special paths are assumed to be already prepared so
// as to be accepted by fileXio calls (so HDD file access use "pfs")
// Generic functions for this purpose that have been added so far are:
// genInit(void), genLimObjName(uLE_path, reserve),
// genFixPath(uLE_path, gen_path),
// genOpen(path, mode), genClose(fd), genDopen(path), genDclose(fd),
// genLseek(fd,where,how), genRead(fd,buf,size), genWrite(fd,buf,size)
// genRemove(path), genRmdir(path)
//--------------------------------------------------------------
//------------------------------
//endfunc genLimObjName
//--------------------------------------------------------------
int genFixPath(const char *inp_path, char *gen_path)
{
	char uLE_path[MAX_PATH], loc_path[MAX_PATH], party[MAX_NAME], *p;
	char *pathSep;
	int part_ix;

	part_ix = 99;  //Assume valid non-HDD path
	if (!uLE_related(uLE_path, inp_path))
		part_ix = -99;           //Assume invalid uLE_related path
	strcpy(gen_path, uLE_path);  //Assume no path patching needed
	pathSep = strchr(uLE_path, '/');

	if (!strncmp(uLE_path, "cdfs", 4)) {  //if using CD or DVD disc path
		LCDVD_FLUSHCACHE();
		LCDVD_DISKREADY(0);
		//end of clause for using a CD or DVD path

	} else if (!strncmp(uLE_path, "usb", 3)) {  //if using USB alias path
		char mass_path[MAX_PATH];
		char *mass_sep;

		loadUsbModules();
		if (mapUsbPathToMassPath(uLE_path, mass_path) < 0) {
			part_ix = -1;
		} else {
			strcpy(gen_path, mass_path);
			mass_sep = strchr(gen_path, '/');
			if (mass_sep && (mass_sep - gen_path < 7) && mass_sep[-1] == ':')
				strcpy(gen_path + (mass_sep - gen_path), mass_sep + 1);
		}
		//end of clause for using a USB alias path

	} else if (!strncmp(uLE_path, "mass", 4)) {  //if using USB mass: path
		loadUsbModules();
		if (pathSep && (pathSep - uLE_path < 7) && pathSep[-1] == ':')
			strcpy(gen_path + (pathSep - uLE_path), pathSep + 1);
		//end of clause for using a USB mass: path

	}
#if defined(ETH) || defined(UDPFS)
	else if (!strncmp(uLE_path, "host", 4) || !strncmp(uLE_path, "udpfs", 5)) {
		makeHostPath(gen_path, uLE_path);
	}
#endif
	else if (!strncmp(uLE_path, "ata", 3)) {  //if using ATA BDM path
#ifdef EXFAT
		loadAtaModules();
#endif
		if (pathSep && (pathSep - uLE_path < 7) && pathSep[-1] == ':')
			strcpy(gen_path + (pathSep - uLE_path), pathSep + 1);

#ifdef MX4SIO
	} else if (!strncmp(uLE_path, "mx4sio", 6)) {
		if (!mx4sio_driver_running && !loadMx4sioModules())
			part_ix = -1;
		if ((part_ix >= 0) && pathSep && (pathSep - uLE_path < 10) && pathSep[-1] == ':')
			strcpy(gen_path + (pathSep - uLE_path), pathSep + 1);
#endif
#ifdef MMCE
	} else if (!strncmp(uLE_path, "mmce", 4)) {
		loadMmceModules();
#endif
	} else if (!strncmp(uLE_path, "hdd0:/", 6)) {  //If using HDD path
		//Get path on HDD unit, LaunchELF's format (e.g. hdd0:/partition/path/to/file)
		strcpy(loc_path, uLE_path + 6);
		if ((p = strchr(loc_path, '/')) != NULL) {
			//Extract path to file within partition. Make a new path, relative to the filesystem root.
			//hdd0:/partition/path/to/file becomes pfs0:/path/to/file.
			sprintf(gen_path, "pfs0:%s", p);
			*p = 0;  //null-terminate the block device path (hdd0:/partition).
		} else {
			//Otherwise, default to /
			strcpy(gen_path, "pfs0:/");
		}
		//Generate standard path to the block device (i.e. hdd0:/partition results in hdd0:partition)
		sprintf(party, "hdd0:%s", loc_path);
		if (nparties == 0) {
			//No partitions recognized? Load modules & populate partition list.
			loadHddModules();
			setPartyList();
		}
		//Mount the partition.
		if ((part_ix = mountParty(party)) >= 0)
			gen_path[3] = part_ix + '0';
#ifdef DVRP
	} else if (!strncmp(uLE_path, "dvr_hdd0:/", 10)) {  //If using DVRP HDD path
		if (!console_is_PSX) {
			part_ix = -1;
			genLimObjName(gen_path, 0);
			return part_ix;
		}
		//Get path on DVR HDD unit, LaunchELF's format (e.g. dvr_hdd0:/partition/path/to/file)
		strcpy(loc_path, uLE_path + 10);
		if ((p = strchr(loc_path, '/')) != NULL) {
			//Extract path to file within partition. Make a new path, relative to the filesystem root.
			//dvr_hdd0:/partition/path/to/file becomes dvr_pfs0:/path/to/file.
			sprintf(gen_path, "dvr_pfs0:%s", p);
			*p = 0;  //null-terminate the block device path (dvr_hdd0:/partition).
		} else {
			//Otherwise, default to /
			strcpy(gen_path, "dvr_pfs0:/");
		}
		//Generate standard path to the block device (i.e. dvr_hdd0:/partition results in hdd0:partition)
		sprintf(party, "dvr_hdd0:%s", loc_path);
		if (ndvrpparties == 0) {
			//No partitions recognized? Load modules & populate partition list.
			loadDVRPHddModules();
			setDVRPPartyList();
		}
		//Mount the partition.
		if ((part_ix = mountDVRPParty(party)) >= 0)
			gen_path[7] = part_ix + '0';
		//end of clause for using an HDD path
#endif
	}
	genLimObjName(gen_path, 0);
	return part_ix;
	//non-HDD Path => 99, Good HDD Path => 0-3, Bad Path => negative
}
//------------------------------
//endfunc genFixPath
//--------------------------------------------------------------
//--------------------------------------------------------------
int readVMC(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t dirbuf;
	char dir[MAX_PATH];
	int i = 0, fd;

	strcpy(dir, path);
	if ((fd = fileXioDopen(dir)) < 0)
		return 0;

	while (fileXioDread(fd, &dirbuf) > 0) {
		//		if(dirbuf.stat.mode & FIO_S_IFDIR &&  //NB: normal usage (non-vmcfs)
		if (dirbuf.stat.mode & sceMcFileAttrSubdir &&  //NB: nonstandard usage of vmcfs
		    (!strcmp(dirbuf.name, ".") || !strcmp(dirbuf.name, "..")))
			continue;  //Skip pseudopaths "." and ".."

		strcpy(info[i].name, dirbuf.name);
		clearMcTable(&info[i].stats);
		//		if(dirbuf.stat.mode & FIO_S_IFDIR){  //NB: normal usage (non-vmcfs)
		//			info[i].stats.attrFile = MC_ATTR_norm_folder;
		//		}
		if (dirbuf.stat.mode & sceMcFileAttrSubdir) {  //NB: vmcfs usage
			info[i].stats.AttrFile = dirbuf.stat.mode;
		}
		//		else if(dirbuf.stat.mode & FIO_S_IFREG){  //NB: normal usage (non-vmcfs)
		//			info[i].stats.attrFile = MC_ATTR_norm_file;
		//			info[i].stats.fileSizeByte = dirbuf.stat.size;
		//			info[i].stats.unknown4[0] = dirbuf.stat.hisize;
		//		}
		else if (dirbuf.stat.mode & sceMcFileAttrFile) {  //NB: vmcfs usage
			info[i].stats.AttrFile = dirbuf.stat.mode;
			info[i].stats.FileSizeByte = dirbuf.stat.size;
			info[i].stats.Reserve2 = dirbuf.stat.hisize;
		} else
			continue;  //Skip entry which is neither a file nor a folder
		snprintf((char *)info[i].stats.EntryName, 32, "%.31s", info[i].name);
		memcpy((void *)&info[i].stats._Create, dirbuf.stat.ctime, 8);
		memcpy((void *)&info[i].stats._Modify, dirbuf.stat.mtime, 8);
		i++;
		if (i == max)
			break;
	}

	fileXioDclose(fd);

	size_valid = 1;
	time_valid = 1;

	return i;
}
//------------------------------
//endfunc readVMC
//--------------------------------------------------------------
int readHDD(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t dirbuf;
	char party[MAX_PATH], dir[MAX_PATH];
	int i = 0, fd, ret;

	if (nparties == 0) {
		loadHddModules();
		setPartyList();
	}

	if (!strcmp(path, "hdd0:/")) {
		unmountHddPartiesNotNeededByClipboard();
		for (i = 0; i < nparties; i++) {
			strcpy(info[i].name, parties[i]);
			info[i].stats.AttrFile = MC_ATTR_norm_folder;
		}
		return nparties;
	}

	getHddParty(path, NULL, party, dir);
	ret = mountParty(party);
	if (ret < 0)
		return 0;
	dir[3] = ret + '0';

	if ((fd = fileXioDopen(dir)) < 0)
		return 0;

	while (fileXioDread(fd, &dirbuf) > 0) {
		if (dirbuf.stat.mode & FIO_S_IFDIR &&
		    (!strcmp(dirbuf.name, ".") || !strcmp(dirbuf.name, "..")))
			continue;  //Skip pseudopaths "." and ".."

		strcpy(info[i].name, dirbuf.name);
		clearMcTable(&info[i].stats);
		if (dirbuf.stat.mode & FIO_S_IFDIR) {
			info[i].stats.AttrFile = MC_ATTR_norm_folder;
		} else if (dirbuf.stat.mode & FIO_S_IFREG) {
			info[i].stats.AttrFile = MC_ATTR_norm_file;
			info[i].stats.FileSizeByte = dirbuf.stat.size;
			info[i].stats.Reserve2 = dirbuf.stat.hisize;
		} else
			continue;  //Skip entry which is neither a file nor a folder
		snprintf((char *)info[i].stats.EntryName, 32, "%.31s", info[i].name);
		memcpy((void *)&info[i].stats._Create, dirbuf.stat.ctime, 8);
		memcpy((void *)&info[i].stats._Modify, dirbuf.stat.mtime, 8);
		i++;
		if (i == max)
			break;
	}

	fileXioDclose(fd);

	size_valid = 1;
	time_valid = 1;

	return i;
}
//------------------------------
//endfunc readHDD
//--------------------------------------------------------------
#ifdef DVRP
int readHDDDVRP(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t dirbuf;
	char party[MAX_PATH], dir[MAX_PATH];
	int i = 0, fd, ret;

	if (!console_is_PSX)
		return 0;

	if (ndvrpparties == 0) {
		loadDVRPHddModules();
		setDVRPPartyList();
	}

	if (!strcmp(path, "dvr_hdd0:/")) {
		for (i = 0; i < ndvrpparties; i++) {
			strcpy(info[i].name, parties[i]);
			info[i].stats.AttrFile = MC_ATTR_norm_folder;
		}
		return ndvrpparties;
	}

	getHddDVRPParty(path, NULL, party, dir);
	ret = mountDVRPParty(party);
	if (ret < 0)
		return 0;
	dir[7] = ret + '0';

	if ((fd = fileXioDopen(dir)) < 0)
		return 0;

	while (fileXioDread(fd, &dirbuf) > 0) {
		if (dirbuf.stat.mode & FIO_S_IFDIR &&
		    (!strcmp(dirbuf.name, ".") || !strcmp(dirbuf.name, "..")))
			continue;  //Skip pseudopaths "." and ".."

		strcpy(info[i].name, dirbuf.name);
		clearMcTable(&info[i].stats);
		if (dirbuf.stat.mode & FIO_S_IFDIR) {
			info[i].stats.AttrFile = MC_ATTR_norm_folder;
		} else if (dirbuf.stat.mode & FIO_S_IFREG) {
			info[i].stats.AttrFile = MC_ATTR_norm_file;
			info[i].stats.FileSizeByte = dirbuf.stat.size;
			info[i].stats.Reserve2 = dirbuf.stat.hisize;
		} else
			continue;  //Skip entry which is neither a file nor a folder
		snprintf((char *)info[i].stats.EntryName, 32, "%.31s", info[i].name);
		memcpy((void *)&info[i].stats._Create, dirbuf.stat.ctime, 8);
		memcpy((void *)&info[i].stats._Modify, dirbuf.stat.mtime, 8);
		i++;
		if (i == max)
			break;
	}

	fileXioDclose(fd);

	size_valid = 1;
	time_valid = 1;

	return i;
}
//------------------------------
//endfunc readHDDDVRP
//--------------------------------------------------------------
#endif

void scan_USB_mass(void)
{
	int i;
	iox_stat_t chk_stat;
	char mass_path[8] = "mass0:/";

	loadUsbModules();
	if (!USB_mass_loaded)
		return;

	if ((USB_mass_max_drives < 2)  //No need for dynamic lists with only one drive
	    || (USB_mass_scanned && ((Timer() - USB_mass_scan_time) < 5000)))
		return;

	for (i = 0; i < USB_mass_max_drives; i++) {
		mass_path[4] = '0' + i;
		if (fileXioGetStat(mass_path, &chk_stat) < 0) {
			USB_mass_ix[i] = 0;
			continue;
		}
		USB_mass_ix[i] = '0' + i;
		USB_mass_scanned = 1;
		USB_mass_scan_time = Timer();
	}  //ends for loop
}
//------------------------------
//endfunc scan_USB_mass
//--------------------------------------------------------------
int readGENERIC(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t record;
	int n = 0, dd = -1;

	if ((dd = fileXioDopen(path)) < 0)
		goto exit;  //exit if error opening directory
	while (fileXioDread(dd, &record) > 0) {
		if ((FIO_S_ISDIR(record.stat.mode)) && (!strcmp(record.name, ".") || !strcmp(record.name, "..")))
			continue;  //Skip entry if pseudo-folder "." or ".."

		strcpy(info[n].name, record.name);
		clearMcTable(&info[n].stats);
		if (FIO_S_ISDIR(record.stat.mode)) {
			info[n].stats.AttrFile = MC_ATTR_norm_folder;
		} else if (FIO_S_ISREG(record.stat.mode)) {
			info[n].stats.AttrFile = MC_ATTR_norm_file;
			info[n].stats.FileSizeByte = record.stat.size;
			info[n].stats.Reserve2 = record.stat.hisize;
		} else {
			DPRINTF("%s:UNKNOWN_FILEMODE:('%s', 0x%x, siz:%d, attr:%d)\n", record.name, record.stat.mode, record.stat.size, record.stat.attr);
			continue;  //Skip entry which is neither a file nor a folder
		}
		snprintf((char *)info[n].stats.EntryName, 32, "%.31s", info[n].name);
		memcpy((void *)&info[n].stats._Create, record.stat.ctime, 8);
		memcpy((void *)&info[n].stats._Modify, record.stat.mtime, 8);
		n++;
		if (n == max)
			break;
	}  //ends while
	size_valid = 1;
	time_valid = 1;

exit:
	if (dd >= 0)
		fileXioDclose(dd);  //Close directory if opened above
	return n;
}
//------------------------------
//endfunc readGENERIC
//--------------------------------------------------------------
#if defined(ETH) || defined(UDPFS)
char *makeHostPath(char *dp, char *sp)
{
#ifdef ETH
	int i;
	const char *src;
	char tmp[MAX_PATH];

	if (!strncmp(sp, "host:", 5)) {
		src = sp + 5;
		if (*src == '/')
			src++;
		snprintf(tmp, sizeof(tmp), "host:%s", src);

		if (!host_use_Bsl)
			return strcpy(dp, tmp);

		strcpy(dp, tmp);
		for (i = 5; dp[i] != '\0'; i++)
			if (dp[i] == '/')
				dp[i] = '\\';
		return dp;
	}
#endif
#ifdef UDPFS
	if (!strncmp(sp, "udpfs:", 6))
		goto copy_passthrough;
#endif

copy_passthrough:
	if (dp != sp)
		strcpy(dp, sp);
	return dp;
}
#endif
//--------------------------------------------------------------
char *makeFslPath(char *dp, char *sp)
{
	int i;
	char ch;

	for (i = 0; i < MAX_PATH - 1; i++) {
		ch = sp[i];
		if (ch == '\\')
			dp[i] = '/';
		else
			dp[i] = ch;
		if (!ch)
			break;
	}
	return dp;
}
//--------------------------------------------------------------
#if defined(ETH) || defined(UDPFS)
#ifdef UDPFS
static void initUDPFS(void)
{
	int fd;

	load_udpfs();
	host_error = 0;
	host_elflist = 0;
	if ((fd = fileXioDopen("udpfs:/")) >= 0)
		fileXioDclose(fd);
	else
		host_error = 1;
	host_ready = 1;
}
#endif

void initHOST(void)
{
#ifdef ETH
	int fd;

	load_ps2host();
	host_error = 0;
	if ((fd = fileXioOpen("host:elflist.txt", FIO_O_RDONLY, 0)) >= 0) {
		fileXioClose(fd);
		host_elflist = 1;
	} else {
		host_elflist = 0;
		if ((fd = fileXioDopen("host:")) >= 0)
			fileXioDclose(fd);
		else
			host_error = 1;
	}
	host_ready = 1;
#elif defined(UDPFS)
	initUDPFS();
#endif
}
//--------------------------------------------------------------
int readHOST(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t hostcontent;
	int hfd, rv, hostcount = 0;
	char host_path[MAX_PATH];
	char Win_path[MAX_PATH];
#if defined(ETH) && defined(UDPFS)
	const int use_udpfs = !strncmp(path, "udpfs", 5);
#endif

#if defined(ETH) && defined(UDPFS)
	if (use_udpfs)
		initUDPFS();
	else
		initHOST();
#elif defined(UDPFS)
	initUDPFS();
#else
	initHOST();
#endif
	snprintf(host_path, MAX_PATH, "%s", path);

#ifdef ETH
#ifdef UDPFS
	if (!use_udpfs && !strncmp(path, "host:/", 6))
		strcpy(host_path + 5, path + 6);
	if (!use_udpfs && host_elflist && !strcmp(host_path, "host:")) {
#else
	if (!strncmp(path, "host:/", 6))
		strcpy(host_path + 5, path + 6);
	if (host_elflist && !strcmp(host_path, "host:")) {
#endif
		int size, contentptr;
		char *elflisttxt, elflistchar;
		char host_next[MAX_PATH];

		if ((hfd = fileXioOpen("host:elflist.txt", FIO_O_RDONLY, 0)) < 0)
			return 0;
		if ((size = fileXioLseek(hfd, 0, SEEK_END)) <= 0) {
			fileXioClose(hfd);
			return 0;
		}
		elflisttxt = (char *)memalign(64, size);
		fileXioLseek(hfd, 0, SEEK_SET);
		fileXioRead(hfd, elflisttxt, size);
		fileXioClose(hfd);
		contentptr = 0;
		for (rv = 0; rv <= size; rv++) {
			elflistchar = elflisttxt[rv];
			if ((elflistchar == 0x0a) || (rv == size)) {
				host_next[contentptr] = 0;
				snprintf(host_path, sizeof(host_path), "host:%.*s", (int)(sizeof(host_path) - sizeof("host:")), host_next);
				clearMcTable(&info[hostcount].stats);
				if ((hfd = fileXioOpen(makeHostPath(Win_path, host_path), FIO_O_RDONLY, 0)) >= 0) {
					fileXioClose(hfd);
					info[hostcount].stats.AttrFile = MC_ATTR_norm_file;
					makeFslPath(info[hostcount++].name, host_next);
				} else if ((hfd = fileXioDopen(Win_path)) >= 0) {
					fileXioDclose(hfd);
					info[hostcount].stats.AttrFile = MC_ATTR_norm_folder;
					makeFslPath(info[hostcount++].name, host_next);
				}
				contentptr = 0;
				if (hostcount > max)
					break;
			} else if ((elflistchar != 0x0d) && (contentptr < (MAX_PATH - 1))) {
				host_next[contentptr++] = elflistchar;
			}
		}
		free(elflisttxt);
		return hostcount - 1;
	}
#endif

	if ((hfd = fileXioDopen(makeHostPath(Win_path, host_path))) < 0)
		return 0;
	strcpy(host_path, Win_path);
	while ((rv = fileXioDread(hfd, &hostcontent))) {
		if (strcmp(hostcontent.name, ".") && strcmp(hostcontent.name, "..")) {
			size_valid = 1;
			time_valid = 1;
			snprintf(Win_path, MAX_PATH - 1, "%s%s", host_path, hostcontent.name);
			strcpy(info[hostcount].name, hostcontent.name);
			clearMcTable(&info[hostcount].stats);

			if (!(hostcontent.stat.mode & FIO_S_IFDIR))  //if not a directory
				info[hostcount].stats.AttrFile = MC_ATTR_norm_file;
			else
				info[hostcount].stats.AttrFile = MC_ATTR_norm_folder;

			info[hostcount].stats.FileSizeByte = hostcontent.stat.size;
			info[hostcount].stats.Reserve2 = hostcontent.stat.hisize;  //taking an unused(?) unknown for the high bits
			memcpy((void *)&info[hostcount].stats._Create, hostcontent.stat.ctime, 8);
			info[hostcount].stats._Create.Year += 1900;
			memcpy((void *)&info[hostcount].stats._Modify, hostcontent.stat.mtime, 8);
			info[hostcount].stats._Modify.Year += 1900;
			hostcount++;
			if (hostcount >= max)
				break;
		}
	}
	fileXioDclose(hfd);
	strcpy(info[hostcount].name, "\0");
	return hostcount;
}
#endif
//------------------------------
//endfunc readHOST
//--------------------------------------------------------------
int getDir(const char *path, FILEINFO *info)
{
	int max = MAX_ENTRY - 2;
	int n;

	if (!strncmp(path, "mc", 2))
		n = readMC(path, info, max);
	else if (!strncmp(path, "hdd", 3))
		n = readHDD(path, info, max);
#ifdef DVRP
	else if (console_is_PSX && !strncmp(path, "dvr_hdd", 7))
		n = readHDDDVRP(path, info, max);
#endif
	else if (!strncmp(path, "mass", 4)) {
		if (!USB_mass_scanned)
			scan_USB_mass();
		n = readGENERIC(path, info, max);
	}
	else if (!strncmp(path, "usb", 3)) {
		char mass_path[MAX_PATH];

		if (mapUsbPathToMassPath(path, mass_path) < 0)
			return 0;
		if (!USB_mass_scanned)
			scan_USB_mass();
		n = readGENERIC(mass_path, info, max);
	}
	else if (!strncmp(path, "ata", 3)) {
#ifdef EXFAT
		u64 wait_start;
		u64 backoff_start;
		int dd;
		int is_ata_root;
		int wait_budget_ms;
		int ata0_ready;
		int path_ready;

		loadAtaModules();
		is_ata_root = (!strcmp(path, "ata:/") || !strcmp(path, "ata:"));
		wait_budget_ms = is_ata_root ? 2000 : 750;
#endif
		n = readGENERIC(path, info, max);
#ifdef EXFAT
		// First browse after loading ATA_BD can race module/device readiness.
		// Retry briefly so ata:/ populates on first open instead of requiring re-entry.
		if (n == 0 && wait_budget_ms > 0) {
			if (is_ata_root)
				n = readGENERIC("ata0:/", info, max);
			if (n == 0) {
				wait_start = Timer();
				while (Timer() < wait_start + wait_budget_ms) {
					path_ready = 0;
					ata0_ready = 0;

					dd = fileXioDopen(path);
					if (dd >= 0) {
						fileXioDclose(dd);
						path_ready = 1;
					}

					// bdmfs exposes typed volumes as ata0:, ata1:, etc.
					// Probe ata0:/ too, to wait for first mount to settle.
					if (is_ata_root) {
						dd = fileXioDopen("ata0:/");
						if (dd >= 0) {
							fileXioDclose(dd);
							ata0_ready = 1;
						}
					}

					if (path_ready || ata0_ready) {
						n = 0;
						if (path_ready)
							n = readGENERIC(path, info, max);
						if ((n == 0) && is_ata_root && ata0_ready)
							n = readGENERIC("ata0:/", info, max);
						if (n > 0)
							break;
						if (!is_ata_root)
							break;
					}

					// Avoid tight spinning while IOP-side BDM mount events settle.
					backoff_start = Timer();
					while (Timer() < backoff_start + 100) {
					}
				}
			}
		}
#endif
	}
	else if (!strncmp(path, "cdfs", 4)) {
		loadCdModules();
		n = readCD(path, info, max);
	}
#if defined(ETH) || defined(UDPFS)
#ifdef ETH
	else if (!strncmp(path, "host", 4))
		n = readHOST(path, info, max);
#endif
#ifdef UDPFS
	else if (!strncmp(path, "udpfs", 5))
		n = readHOST(path, info, max);
#endif
#endif
#ifdef MMCE
	else if (!strncmp(path, "mmce", 4)) {
		loadMmceModules();
		n = readGENERIC(path, info, max);
	}
#endif
#ifdef MX4SIO
	else if (!strncmp(path, "mx4sio", 6)) {
		char indexed_path[MAX_PATH];
		char indexed_path_alt[MAX_PATH];
		char path_alt[MAX_PATH];
		const char *suffix;
		u64 wait_start;
		u64 backoff_start;
		int is_root;
		int wait_budget_ms;
		int path_ready;
		int indexed_ready;
		int dd;
		int has_indexed_path = 0;
		int has_indexed_path_alt = 0;
		int has_path_alt = 0;

		if (!mx4sio_driver_running)
			if (!loadMx4sioModules())
				return 0;

		is_root = (!strcmp(path, "mx4sio:/") || !strcmp(path, "mx4sio:"));
		wait_budget_ms = is_root ? 2000 : 750;

		indexed_path[0] = '\0';
		indexed_path_alt[0] = '\0';
		path_alt[0] = '\0';

		if (is_root) {
			if (!strcmp(path, "mx4sio:"))
				strcpy(path_alt, "mx4sio:/");
			else
				strcpy(path_alt, "mx4sio:");
			has_path_alt = 1;
		}

		n = readGENERIC(path, info, max);
		if ((n == 0) && has_path_alt)
			n = readGENERIC(path_alt, info, max);

		if (path[6] == ':') {
			suffix = path + 7;
			if (*suffix == '\0') {
				/*
				 * Some stacks accept "mx4sio0:", others need "mx4sio0:/".
				 * Probe both forms for first-open compatibility.
				 */
				strcpy(indexed_path, "mx4sio0:");
				strcpy(indexed_path_alt, "mx4sio0:/");
				has_indexed_path = 1;
				has_indexed_path_alt = 1;
			} else {
				/* Try indexed typed-device form for stacks that require explicit unit number. */
				snprintf(indexed_path, sizeof(indexed_path), "mx4sio0:%s", suffix);
				has_indexed_path = 1;
				if (!strcmp(suffix, "/")) {
					strcpy(indexed_path_alt, "mx4sio0:");
					has_indexed_path_alt = 1;
				}
			}
		}

		if ((n == 0) && has_indexed_path)
			n = readGENERIC(indexed_path, info, max);
		if ((n == 0) && has_indexed_path_alt)
			n = readGENERIC(indexed_path_alt, info, max);

		/* First browse after loading MX4SIO can race mount readiness; retry briefly. */
		if (n == 0 && wait_budget_ms > 0) {
			wait_start = Timer();
			while (Timer() < wait_start + wait_budget_ms) {
				path_ready = 0;
				indexed_ready = 0;

				dd = fileXioDopen(path);
				if (dd >= 0) {
					fileXioDclose(dd);
					path_ready = 1;
				}

				if (indexed_path[0] != '\0') {
					dd = fileXioDopen(indexed_path);
					if (dd >= 0) {
						fileXioDclose(dd);
						indexed_ready = 1;
					}
				}
				if (!indexed_ready && indexed_path_alt[0] != '\0') {
					dd = fileXioDopen(indexed_path_alt);
					if (dd >= 0) {
						fileXioDclose(dd);
						indexed_ready = 1;
					}
				}

				if (path_ready || indexed_ready) {
					n = 0;
					if (path_ready)
						n = readGENERIC(path, info, max);
					if (n == 0 && has_path_alt)
						n = readGENERIC(path_alt, info, max);
					if (n == 0 && indexed_ready) {
						n = readGENERIC(indexed_path, info, max);
						if (n == 0 && indexed_path_alt[0] != '\0')
							n = readGENERIC(indexed_path_alt, info, max);
					}
					if (n > 0 || !is_root)
						break;
				}

				backoff_start = Timer();
				while (Timer() < backoff_start + 100) {
				}
			}
		}
	}
#endif
#ifdef XFROM
	else if (!strncmp(path, "xfrom", 5)) {
		if (!console_is_PSX)
			return 0;
		loadFlashModules();
		n = readGENERIC(path, info, max);
	}
#endif
	else if (!strncmp(path, "vmc", 3))
		n = readVMC(path, info, max);
	else
		return 0;

	return n;
}
//--------------------------------------------------------------
int setFileList(const char *path, const char *ext, FILEINFO *files, int cnfmode)
{
	int nfiles, i, j, ret, allow_usb_devices, hide_hdd_ata_devices;

	size_valid = 0;
	time_valid = 0;
	nfiles = 0;
	if (path[0] == 0) {
		//-- Start case for browser root pseudo folder with device links --
		if (USB_mass_scanned)  //if mass drives were scanned in earlier browsing
			scan_USB_mass();   //then allow another scan here (timer dependent)
		allow_usb_devices = ((cnfmode != USBD_IRX_CNF) && (cnfmode != USBKBD_IRX_CNF) && (cnfmode != USBMASS_IRX_CNF));
		hide_hdd_ata_devices = shouldHideHddAtaDevices();

		strcpy(files[nfiles].name, "mc0:");
		files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		strcpy(files[nfiles].name, "mc1:");
		files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;

		if (allow_usb_devices) {
			strcpy(files[nfiles].name, "mass:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		}

	#ifdef MMCE
			strcpy(files[nfiles].name, "mmce0:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
			strcpy(files[nfiles].name, "mmce1:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
	#endif

	#ifdef MX4SIO
			if (allow_usb_devices) {
				strcpy(files[nfiles].name, "mx4sio:");
				files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
			}
	#endif

		if (!hide_hdd_ata_devices) {
			strcpy(files[nfiles].name, "hdd0:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		}

#ifdef EXFAT
		if (allow_usb_devices && !hide_hdd_ata_devices) {
			strcpy(files[nfiles].name, "ata:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		}
#endif

			strcpy(files[nfiles].name, "cdfs:");
			files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;

#ifdef XFROM
			if (console_is_PSX) {
				strcpy(files[nfiles].name, "xfrom0:");
				files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
			}
#endif

#ifdef DVRP
			if (console_is_PSX) {
				strcpy(files[nfiles].name, "dvr_hdd0:");
				files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
			}
#endif
			if (!cnfmode) {
				//This condition blocks selecting any CONFIG items on PC
				//or in a virtual memory card
#ifdef ETH
				strcpy(files[nfiles].name, "host:");
				files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
#endif
#ifdef UDPFS
				strcpy(files[nfiles].name, "udpfs:");
				files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
#endif
				if (vmcMounted[0]) {
					strcpy(files[nfiles].name, "vmc0:");
					files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
				}
				if (vmcMounted[1]) {
					strcpy(files[nfiles].name, "vmc1:");
					files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
				}
			}
			if (cnfmode < 2) {
				//This condition blocks use of MISC pseudo-device for driver path picks.
				//And allows this device only for launch keys and for normal browsing.
				strcpy(files[nfiles].name, LNG(MISC));
			files[nfiles].stats.AttrFile = sceMcFileAttrSubdir;
			nfiles++;
		}
		for (i = 0; i < nfiles; i++)
			files[i].title[0] = 0;
		vfreeSpace = FALSE;
		//-- End case for browser root pseudo folder with device links --
	} else if (!strcmp(path, setting->Misc)) {
		//-- Start case for MISC command pseudo folder with function links --
		nfiles = 0;
		strcpy(files[nfiles].name, "..");
		files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
		if (cnfmode) {  //Stop recursive FileBrowser entry, only allow it for launch keys
			strcpy(files[nfiles].name, LNG(FileBrowser));
			files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		}
		strcpy(files[nfiles].name, LNG(PS2Browser));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(PS2Disc));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		#ifdef ETH
		strcpy(files[nfiles].name, LNG(PS2Net));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		#endif
		strcpy(files[nfiles].name, LNG(PS2PowerOff));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(HddManager));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(TextEditor));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(Configure));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		//Next 2 lines add an optional font test routine
		strcpy(files[nfiles].name, LNG(ShowFont));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(Debug_Info));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(About_uLE));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(Build_Info));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		strcpy(files[nfiles].name, LNG(OSDSYS));
		files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
		for (i = 0; i < nfiles; i++)
			files[i].title[0] = 0;
		//-- End case for MISC command pseudo folder with function links --
	} else {
		//-- Start case for normal folder with file/folder links --
		strcpy(files[0].name, "..");
		files[0].stats.AttrFile = sceMcFileAttrSubdir;
		nfiles = getDir(path, &files[1]) + 1;
		if (strcmp(ext, "*")) {
			for (i = j = 1; i < nfiles; i++) {
				if (files[i].stats.AttrFile & sceMcFileAttrSubdir)
					files[j++] = files[i];
				else {
					if (genCmpFileExt(files[i].name, ext))
						files[j++] = files[i];
				}
			}
			nfiles = j;
		}
		if ((file_show == 2) || (file_sort == 2)) {
			for (i = 1; i < nfiles; i++) {
				ret = filerGetGameTitle(path, &files[i], files[i].title);
				if (ret < 0)
					files[i].title[0] = 0;
			}
		}
		if (!strcmp(path, "hdd0:/") || !strcmp(path, "dvr_hdd0:/"))
			vfreeSpace = FALSE;
		else if (nfiles > 1)
			sort(&files[1], 0, nfiles - 2);
		//-- End case for normal folder with file/folder links --
	}
	return nfiles;
}
//------------------------------
//endfunc setFileList
//--------------------------------------------------------------
//End of file: filer.c
//--------------------------------------------------------------
