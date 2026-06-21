//--------------------------------------------------------------
// File name:   popstarter.c
//--------------------------------------------------------------
#include "launchelf.h"

enum POPSTARTER_RESULT {
	POPSTARTER_OK = 1,
	POPSTARTER_ERR_UNSUPPORTED_PATH = -10,
	POPSTARTER_ERR_INVALID_VCD = -11,
	POPSTARTER_ERR_NOT_VCD = -12,
	POPSTARTER_ERR_OPEN_VCD = -13,
	POPSTARTER_ERR_OPEN_POPSTARTER = -15,
	POPSTARTER_ERR_INVALID_POPSTARTER = -17
};

static int isHddDevicePath(const char *path)
{
	return (path != NULL &&
	        !strncmp(path, "hdd", 3) &&
	        path[3] >= '0' && path[3] <= '9' &&
	        path[4] == ':' &&
	        path[5] == '/');
}

static const char *findHddPfsSeparator(const char *path)
{
	const char *p;

	if (path == NULL)
		return NULL;

	for (p = path; *p != '\0'; p++) {
		if (p[0] == ':' &&
		    p[1] != '\0' &&
		    p[2] != '\0' &&
		    p[3] != '\0' &&
		    wle_ascii_tolower((unsigned char)p[1]) == 'p' &&
		    wle_ascii_tolower((unsigned char)p[2]) == 'f' &&
		    wle_ascii_tolower((unsigned char)p[3]) == 's' &&
		    p[4] == ':')
			return p;
	}

	return NULL;
}

static int splitHddPfsPath(const char *path, char *hdd_device, size_t hdd_device_size,
                           char *partition, size_t partition_size, const char **subpath)
{
	const char *partition_start;
	const char *pfs_sep;
	size_t partition_len;

	if (path == NULL || hdd_device == NULL || hdd_device_size < 6 ||
	    partition == NULL || partition_size == 0 || subpath == NULL)
		return 0;
	if (strncmp(path, "hdd", 3) || path[3] < '0' || path[3] > '9')
		return 0;

	if (path[4] != ':' || path[5] == '/')
		return 0;
	partition_start = path + 5;

	pfs_sep = findHddPfsSeparator(partition_start);
	if (pfs_sep == NULL)
		return 0;

	partition_len = pfs_sep - partition_start;
	if (partition_len == 0 || partition_len >= partition_size)
		return 0;

	snprintf(hdd_device, hdd_device_size, "hdd%c:", path[3]);
	memcpy(partition, partition_start, partition_len);
	partition[partition_len] = '\0';
	*subpath = pfs_sep + 5;
	return 1;
}

static int splitDevicePath(const char *path, char *device, size_t device_size, const char **suffix)
{
	const char *colon;
	size_t len;

	if (path == NULL || device == NULL || device_size == 0 || suffix == NULL)
		return 0;

	colon = strchr(path, ':');
	if (colon == NULL)
		return 0;

	len = colon - path + 1;
	if (len >= device_size)
		return 0;

	memcpy(device, path, len);
	device[len] = '\0';
	*suffix = colon + 1;
	return 1;
}

static int deviceMatchesBase(const char *device, const char *base)
{
	size_t len;

	if (device == NULL || base == NULL)
		return 0;

	len = strlen(base);
	if (strncmp(device, base, len))
		return 0;
	if (device[len] == ':')
		return 1;
	if (device[len] >= '0' && device[len] <= '9' && device[len + 1] == ':')
		return 1;
	return 0;
}

static int prefixCaseCmp(const char *value, const char *prefix, size_t prefix_len)
{
	size_t i;

	for (i = 0; i < prefix_len; i++) {
		if (wle_ascii_tolower((unsigned char)value[i]) != wle_ascii_tolower((unsigned char)prefix[i]))
			return 1;
	}
	return 0;
}

static int isFatStylePopstarterDevice(const char *device)
{
	if (deviceMatchesBase(device, "mass") || deviceMatchesBase(device, "usb"))
		return 1;
#ifdef MMCE
	if (deviceMatchesBase(device, "mmce"))
		return 1;
#endif
#ifdef MX4SIO
	if (deviceMatchesBase(device, "mx4sio"))
		return 1;
#endif
#ifdef EXFAT
	if (deviceMatchesBase(device, "ata"))
		return 1;
#endif
	return 0;
}

static int startsWithPopsDir(const char *suffix, const char **relative_vcd)
{
	if (suffix == NULL || relative_vcd == NULL)
		return 0;

	if (*suffix == '/')
		suffix++;
	if (prefixCaseCmp(suffix, "POPS/", 5))
		return 0;
	if (suffix[5] == '\0')
		return 0;

	*relative_vcd = suffix + 5;
	return 1;
}

static void replaceFinalExtWithElf(char *path)
{
	size_t len;

	len = strlen(path);
	if (len >= 4)
		strcpy(path + len - 4, ".elf");
}

static int buildArg(char *arg, size_t arg_size, const char *prefix, const char *name)
{
	int len;

	len = snprintf(arg, arg_size, "%s%s", prefix, name);
	if (len < 0 || len >= (int)arg_size)
		return 0;
	replaceFinalExtWithElf(arg);
	return 1;
}

static int buildHddPartitionArg(char *arg, size_t arg_size, const char *partition)
{
	int len;

	len = snprintf(arg, arg_size, "uLE:%s.elf", partition);
	return (len > 0 && len < (int)arg_size);
}

static int openHddPfsPathForRead(const char *path, char *opened_path, size_t opened_path_size)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	char party[MAX_NAME];
	char fixed_path[MAX_PATH];
	const char *subpath;
	int pfs_ix;
	int fd;

	if (!splitHddPfsPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath))
		return -1;

	if (*subpath == '\0')
		subpath = "/";
	if (*subpath != '/')
		return -1;

	loadHddModules();
	snprintf(party, sizeof(party), "%s%s", hdd_device, partition);
	pfs_ix = mountParty(party);
	if (pfs_ix < 0)
		return -1;

	snprintf(fixed_path, sizeof(fixed_path), "pfs%d:%s", pfs_ix, subpath);
	fd = genOpen(fixed_path, FIO_O_RDONLY);
	if (fd >= 0 && opened_path != NULL && opened_path_size > 0)
		snprintf(opened_path, opened_path_size, "%s", fixed_path);
	return fd;
}

static int openPathForRead(const char *path, char *opened_path, size_t opened_path_size)
{
	char fixed_path[MAX_PATH];
	int fd;

	if (path == NULL || path[0] == '\0')
		return -1;

	fd = openHddPfsPathForRead(path, opened_path, opened_path_size);
	if (fd >= 0)
		return fd;

	if (genFixPath(path, fixed_path) >= 0) {
		fd = genOpen(fixed_path, FIO_O_RDONLY);
		if (fd >= 0) {
			if (opened_path != NULL && opened_path_size > 0)
				snprintf(opened_path, opened_path_size, "%s", fixed_path);
			return fd;
		}
	}

	if (!strncmp(path, "mc:/", 4)) {
		snprintf(fixed_path, sizeof(fixed_path), "mc0:%s", path + 3);
		fd = genOpen(fixed_path, FIO_O_RDONLY);
		if (fd >= 0) {
			if (opened_path != NULL && opened_path_size > 0)
				snprintf(opened_path, opened_path_size, "%s", fixed_path);
			return fd;
		}

		snprintf(fixed_path, sizeof(fixed_path), "mc1:%s", path + 3);
		fd = genOpen(fixed_path, FIO_O_RDONLY);
		if (fd >= 0) {
			if (opened_path != NULL && opened_path_size > 0)
				snprintf(opened_path, opened_path_size, "%s", fixed_path);
			return fd;
		}
	}

	if (opened_path != NULL && opened_path_size > 0)
		snprintf(opened_path, opened_path_size, "%s", path);
	return -1;
}

static int validateVcd(const char *path)
{
	unsigned char datacheck[32] __attribute__((aligned(16)));
	s64 size;
	int fd;

	fd = openPathForRead(path, NULL, 0);
	if (fd < 0)
		return POPSTARTER_ERR_OPEN_VCD;

	size = genLseek(fd, 0, SEEK_END);
	if (size < 1102672) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}

	genLseek(fd, 0, SEEK_SET);
	if (genRead(fd, datacheck, 4) != 4) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}
	if (datacheck[0] != 0x41 || datacheck[1] != 0x00 || datacheck[2] != 0xA0 || datacheck[3] != 0x00) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}

	if (genLseek(fd, 1024, SEEK_SET) < 0) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}
	if (genRead(fd, datacheck, 16) != 16) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}
	if (datacheck[0] != 0x6B || datacheck[1] != 0x48 || datacheck[2] != 0x6E ||
	    datacheck[8] != datacheck[12] ||
	    datacheck[9] != datacheck[13] ||
	    datacheck[10] != datacheck[14] ||
	    datacheck[11] != datacheck[15]) {
		genClose(fd);
		return POPSTARTER_ERR_INVALID_VCD;
	}

	genClose(fd);
	return POPSTARTER_OK;
}

static int prepareHddLaunch(const char *path, char *arg, size_t arg_size, char *default_popstarter, size_t default_popstarter_size)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	const char *partition_start;
	const char *subpath;
	const char *name;
	size_t partition_len;
	int is_pops_partition;

	if (isHddDevicePath(path)) {
		memcpy(hdd_device, path, 5);
		hdd_device[5] = '\0';
		partition_start = path + 6;
		subpath = strchr(partition_start, '/');
		if (subpath == NULL)
			return POPSTARTER_ERR_UNSUPPORTED_PATH;

		partition_len = subpath - partition_start;
		if (partition_len == 0 || partition_len > MAX_PART_NAME)
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
		memcpy(partition, partition_start, partition_len);
		partition[partition_len] = '\0';
	} else if (!splitHddPfsPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath)) {
		return 0;
	}

	if (subpath == NULL || subpath[0] == '\0' || subpath[0] != '/')
		return POPSTARTER_ERR_UNSUPPORTED_PATH;

	is_pops_partition = !strncmp(partition, "__.POPS", 7);
	if (is_pops_partition) {
		name = strrchr(subpath, '/');
		if (name == NULL || !buildArg(arg, arg_size, "uLE:", name))
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
	} else {
		if (strncmp(partition, "__.", 3) && strncmp(partition, "PP.", 3))
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
		if (stricmp(subpath, "/IMAGE0.VCD"))
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
		if (!buildHddPartitionArg(arg, arg_size, partition))
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
	}

	snprintf(default_popstarter, default_popstarter_size, "%s__common:pfs:/POPS/POPS.ELF", hdd_device);
	return POPSTARTER_OK;
}

static int prepareFatStyleLaunch(const char *path, char *arg, size_t arg_size, char *default_popstarter, size_t default_popstarter_size)
{
	char device[16];
	const char *suffix;
	const char *relative_vcd;

	if (!splitDevicePath(path, device, sizeof(device), &suffix))
		return 0;
	if (!isFatStylePopstarterDevice(device))
		return 0;
	if (!startsWithPopsDir(suffix, &relative_vcd))
		return POPSTARTER_ERR_UNSUPPORTED_PATH;
	if (!buildArg(arg, arg_size, "uLE:XX.", relative_vcd))
		return POPSTARTER_ERR_UNSUPPORTED_PATH;

	snprintf(default_popstarter, default_popstarter_size, "%s/POPS/POPSTARTER.ELF", device);
	return POPSTARTER_OK;
}

static int prepareLaunch(const char *path, char *arg, size_t arg_size, char *popstarter_path, size_t popstarter_path_size)
{
	char default_popstarter[MAX_PATH];
	int ret;

	if (!IsPopstarterVcdPath(path))
		return POPSTARTER_ERR_NOT_VCD;

	default_popstarter[0] = '\0';
	ret = prepareHddLaunch(path, arg, arg_size, default_popstarter, sizeof(default_popstarter));
	if (ret == 0)
		ret = prepareFatStyleLaunch(path, arg, arg_size, default_popstarter, sizeof(default_popstarter));
	if (ret <= 0)
		return (ret == 0) ? POPSTARTER_ERR_UNSUPPORTED_PATH : ret;

	if (setting != NULL && setting->popstarter_file[0] != '\0')
		snprintf(popstarter_path, popstarter_path_size, "%s", setting->popstarter_file);
	else
		snprintf(popstarter_path, popstarter_path_size, "%s", default_popstarter);
	return POPSTARTER_OK;
}

static int checkExecutablePath(const char *path, int *exec_kind)
{
	char tmp[MAX_PATH];
	int kind;

	if (path == NULL || path[0] == '\0' || exec_kind == NULL)
		return 0;

	snprintf(tmp, sizeof(tmp), "%s", path);
	kind = checkELFheader(tmp);
	if (kind <= 0)
		return 0;

	*exec_kind = kind;
	return 1;
}

static int prepareHddPfsElfLaunch(const char *path, char *fullpath, size_t fullpath_size,
                                  char *party, size_t party_size, int *exec_kind)
{
	char hdd_device[6];
	char partition[MAX_PART_NAME + 1];
	char browser_path[MAX_PATH];
	const char *subpath;

	if (!splitHddPfsPath(path, hdd_device, sizeof(hdd_device), partition, sizeof(partition), &subpath))
		return 0;
	if (subpath == NULL || subpath[0] == '\0')
		subpath = "/";
	if (subpath[0] != '/')
		return POPSTARTER_ERR_UNSUPPORTED_PATH;

	snprintf(browser_path, sizeof(browser_path), "%s/%s%s", hdd_device, partition, subpath);
	if (!checkExecutablePath(browser_path, exec_kind))
		return POPSTARTER_ERR_INVALID_POPSTARTER;

	snprintf(party, party_size, "%s%s", hdd_device, partition);
	snprintf(fullpath, fullpath_size, "pfs0:%s", subpath);
	return POPSTARTER_OK;
}

static int preparePopstarterElfLaunch(const char *path, char *fullpath, size_t fullpath_size, char *party, size_t party_size, int *exec_kind)
{
	char tmp[MAX_PATH];
	char *p;
	int fd;
	int ret;

	if (path == NULL || path[0] == '\0' || fullpath == NULL || fullpath_size == 0 ||
	    party == NULL || party_size == 0 || exec_kind == NULL)
		return POPSTARTER_ERR_OPEN_POPSTARTER;

	fullpath[0] = '\0';
	party[0] = '\0';

	fd = openPathForRead(path, NULL, 0);
	if (fd < 0)
		return POPSTARTER_ERR_OPEN_POPSTARTER;
	genClose(fd);

	ret = prepareHddPfsElfLaunch(path, fullpath, fullpath_size, party, party_size, exec_kind);
	if (ret != 0)
		return ret;

	if (!strncmp(path, "mc:/", 4)) {
		snprintf(tmp, sizeof(tmp), "mc0:%s", path + 3);
		if (checkExecutablePath(tmp, exec_kind)) {
			snprintf(fullpath, fullpath_size, "%s", tmp);
			return POPSTARTER_OK;
		}

		snprintf(tmp, sizeof(tmp), "mc1:%s", path + 3);
		if (checkExecutablePath(tmp, exec_kind)) {
			snprintf(fullpath, fullpath_size, "%s", tmp);
			return POPSTARTER_OK;
		}
		return POPSTARTER_ERR_INVALID_POPSTARTER;
	}

	if (isHddDevicePath(path)) {
		loadHddModules();
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(party, party_size, "hdd%c:%s", path[3], path + 6);
		p = strchr(party, '/');
		if (p == NULL)
			return POPSTARTER_ERR_UNSUPPORTED_PATH;
		snprintf(fullpath, fullpath_size, "pfs0:%s", p);
		*p = '\0';
		return POPSTARTER_OK;
	}

#ifdef MX4SIO
	if (!strncmp(path, "mx4sio", 6)) {
		if (!mx4sio_driver_running && !loadMx4sioModules())
			return POPSTARTER_ERR_OPEN_POPSTARTER;
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "%s", path);
		return POPSTARTER_OK;
	}
#endif

#ifdef MMCE
	if (!strncmp(path, "mmce", 4)) {
		loadMmceModules();
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "%s", path);
		return POPSTARTER_OK;
	}
#endif

	if (!strncmp(path, "usb", 3)) {
		char *pathSep;

		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		if (genFixPath(path, fullpath) < 0)
			return POPSTARTER_ERR_OPEN_POPSTARTER;
		pathSep = strchr(fullpath, '/');
		if (pathSep && (pathSep - fullpath < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - fullpath), pathSep + 1);
		return POPSTARTER_OK;
	}

	if (!strncmp(path, "ata", 3)) {
		char *pathSep;

#ifdef EXFAT
		loadAtaModules();
#endif
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		if (!strncmp(path, "ata:", 4))
			snprintf(fullpath, fullpath_size, "ata0:%s", path + 4);
		else
			snprintf(fullpath, fullpath_size, "%s", path);
		pathSep = strchr(fullpath, '/');
		if (pathSep && (pathSep - fullpath < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - fullpath), pathSep + 1);
		return POPSTARTER_OK;
	}

	if (!strncmp(path, "mass", 4)) {
		char *pathSep;

		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "%s", path);
		pathSep = strchr(path, '/');
		if (pathSep && (pathSep - path < 7) && pathSep[-1] == ':')
			strcpy(fullpath + (pathSep - path), pathSep + 1);
		return POPSTARTER_OK;
	}

#if defined(ETH) || defined(UDPFS)
	if (!strncmp(path, "host:", 5)) {
#ifdef ETH
		initHOST();
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "host:%s", (path[5] == '/') ? path + 6 : path + 5);
		makeHostPath(fullpath, fullpath);
		return POPSTARTER_OK;
#else
		return POPSTARTER_ERR_UNSUPPORTED_PATH;
#endif
	}

	if (!strncmp(path, "udpfs:", 6)) {
#ifdef UDPFS
		load_udpfs();
		if (!checkExecutablePath(path, exec_kind))
			return POPSTARTER_ERR_INVALID_POPSTARTER;
		snprintf(fullpath, fullpath_size, "%s", path);
		return POPSTARTER_OK;
#else
		return POPSTARTER_ERR_UNSUPPORTED_PATH;
#endif
	}
#endif

	if (!checkExecutablePath(path, exec_kind))
		return POPSTARTER_ERR_INVALID_POPSTARTER;

	snprintf(fullpath, fullpath_size, "%s", path);
	return POPSTARTER_OK;
}

static void setPopstarterMessage(char *message, size_t message_size, int result, const char *vcd_path, const char *popstarter_path)
{
	if (message == NULL || message_size == 0)
		return;

	switch (result) {
		case POPSTARTER_ERR_NOT_VCD:
		case POPSTARTER_ERR_INVALID_VCD:
			snprintf(message, message_size, "This file isn't a POPS VCD: %s", vcd_path);
			break;
		case POPSTARTER_ERR_UNSUPPORTED_PATH:
			snprintf(message, message_size, "POPStarter does not support that VCD path: %s", vcd_path);
			break;
		case POPSTARTER_ERR_OPEN_VCD:
			snprintf(message, message_size, "Cannot open POPS VCD: %s", vcd_path);
			break;
		case POPSTARTER_ERR_OPEN_POPSTARTER:
			snprintf(message, message_size, "Cannot find POPStarter ELF: %s", popstarter_path);
			break;
		case POPSTARTER_ERR_INVALID_POPSTARTER:
			snprintf(message, message_size, "POPStarter ELF is not an ELF: %s", popstarter_path);
			break;
		default:
			snprintf(message, message_size, "POPStarter launch failed: %s", vcd_path);
			break;
	}
	message[message_size - 1] = '\0';
}

int IsPopstarterVcdPath(const char *path)
{
	return (path != NULL && genCmpFileExt(path, "VCD"));
}

int LaunchPopstarterVcd(const char *path, char *message, size_t message_size)
{
	char arg0[MAX_PATH];
	char popstarter_path[MAX_PATH];
	char popstarter_fullpath[MAX_PATH];
	char popstarter_party[MAX_PATH];
	int exec_kind;
	int reboot_iop_elf_load;
	int ret;

	arg0[0] = '\0';
	popstarter_path[0] = '\0';
	popstarter_fullpath[0] = '\0';
	popstarter_party[0] = '\0';
	exec_kind = 1;
	reboot_iop_elf_load = 0;

	ret = prepareLaunch(path, arg0, sizeof(arg0), popstarter_path, sizeof(popstarter_path));
	if (ret < 0)
		goto fail;

	ret = validateVcd(path);
	if (ret < 0)
		goto fail;

	ret = preparePopstarterElfLaunch(popstarter_path, popstarter_fullpath, sizeof(popstarter_fullpath),
	                                 popstarter_party, sizeof(popstarter_party), &exec_kind);
	if (ret < 0)
		goto fail;

	if (setting != NULL)
		reboot_iop_elf_load = setting->reboot_iop_elf_load;
	CleanUpForExec();
	RunLoaderElf(popstarter_fullpath, popstarter_party, arg0, exec_kind, reboot_iop_elf_load);
	return POPSTARTER_OK;

fail:
	setPopstarterMessage(message, message_size, ret, path, popstarter_path);
	return ret;
}

//--------------------------------------------------------------
// End of file: popstarter.c
//--------------------------------------------------------------
