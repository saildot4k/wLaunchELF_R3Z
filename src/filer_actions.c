#include "launchelf.h"
#include "filer_actions.h"
#include "filer_shared.h"
#include "gui_hdd0_format.h"
#include "init.h"

#define IOCTL_RENAME 0xFEEDC0DE

#ifdef XFROM
static const char *getXfromRelativePath(const char *path)
{
	const char *relative_path = strchr(path, ':');

	if (relative_path == NULL)
		return path;
	relative_path++;
	if (*relative_path == '/')
		relative_path++;
	return relative_path;
}
#endif

enum {
	FILER_CONFLICT_NONE = 0,
	FILER_CONFLICT_FILE,
	FILER_CONFLICT_DIR
};

enum {
	FILER_EXPLOIT_PROTECT_NONE = 0,
	FILER_EXPLOIT_PROTECT_THIS_CONSOLE,
	FILER_EXPLOIT_PROTECT_OTHER_REGION,
	FILER_EXPLOIT_PROTECT_GENERIC
};

static int filerPathConflictType(const char *path)
{
	iox_stat_t stat;
	char dir_path[MAX_PATH];
	int fd, stat_found;

	if (path == NULL || path[0] == '\0')
		return FILER_CONFLICT_NONE;

	stat_found = (genGetStat(path, &stat) >= 0);
	if (stat_found && FIO_S_ISDIR(stat.mode))
		return FILER_CONFLICT_DIR;

	snprintf(dir_path, sizeof(dir_path), "%s", path);
	fd = genDopen(dir_path);
	if (fd >= 0) {
		genDclose(fd);
		return FILER_CONFLICT_DIR;
	}

	if (stat_found)
		return FILER_CONFLICT_FILE;

	fd = genOpen(path, FIO_O_RDONLY);
	if (fd >= 0) {
		genClose(fd);
		return FILER_CONFLICT_FILE;
	}

	return FILER_CONFLICT_NONE;
}

static int filerPathExistsForConflict(const char *path)
{
	return (filerPathConflictType(path) != FILER_CONFLICT_NONE);
}

static int filerMkdirNoOverwrite(const char *path)
{
	int conflict_type;

	conflict_type = filerPathConflictType(path);
	if (conflict_type == FILER_CONFLICT_DIR)
		return -EEXIST;
	if (conflict_type == FILER_CONFLICT_FILE)
		return -1;
	return genMkdir(path, fileMode);
}

static int filerIsMcRootExploitFolderName(const char *name)
{
	if (name == NULL)
		return 0;

	return (!stricmp(name, "OPENTUNA") ||
	        !stricmp(name, "FUNTUNA") ||
	        !stricmp(name, "FORTUNA") ||
	        !stricmp(name, HACK_FOLDER));
}

static int filerIsSystemUpdateFolderName(const char *name)
{
	char region;

	if (name == NULL)
		return 0;
	if (strlen(name) != strlen("BIEXEC-SYSTEM"))
		return 0;
	if (name[0] != 'B' || stricmp(name + 2, "EXEC-SYSTEM"))
		return 0;

	region = name[1];
	return (region == 'I' || region == 'E' || region == 'A' || region == 'C');
}

static void filerBuildFullPath(char *out, size_t out_size, const char *path, const FILEINFO *file)
{
	if (out == NULL || out_size == 0)
		return;

	if (path == NULL)
		path = "";
	if (file != NULL)
		snprintf(out, out_size, "%s%s", path, file->name);
	else
		snprintf(out, out_size, "%s", path);
	out[out_size - 1] = '\0';
}

static int filerGetFirstPathComponent(const char *full_path, char *component, size_t component_size)
{
	const char *p;
	const char *slash;
	size_t len;

	if (full_path == NULL || component == NULL || component_size == 0)
		return 0;

	component[0] = '\0';
	p = strchr(full_path, ':');
	if (p == NULL)
		return 0;
	p++;
	if (*p == '/')
		p++;
	if (*p == '\0')
		return 0;

	slash = strchr(p, '/');
	len = (slash != NULL) ? (size_t)(slash - p) : strlen(p);
	if (len == 0)
		return 0;
	if (len >= component_size)
		len = component_size - 1;

	memcpy(component, p, len);
	component[len] = '\0';
	return 1;
}

static int filerGetExploitProtectionType(const char *path, const FILEINFO *file, char *folder, size_t folder_size)
{
	char full_path[MAX_PATH];
	char root_folder[40];
	int is_mc;
	int is_xfrom;

	if (folder != NULL && folder_size > 0)
		folder[0] = '\0';
	if (path == NULL)
		return FILER_EXPLOIT_PROTECT_NONE;

	is_mc = (!strncmp(path, "mc0:", 4) || !strncmp(path, "mc1:", 4));
	is_xfrom = (!strncmp(path, "xfrom:", 6));
	if (!is_mc && !is_xfrom)
		return FILER_EXPLOIT_PROTECT_NONE;

	filerBuildFullPath(full_path, sizeof(full_path), path, file);
	if (!filerGetFirstPathComponent(full_path, root_folder, sizeof(root_folder)))
		return FILER_EXPLOIT_PROTECT_NONE;

	if (folder != NULL && folder_size > 0) {
		snprintf(folder, folder_size, "%s", root_folder);
		folder[folder_size - 1] = '\0';
	}

	if (is_mc) {
		if (filerIsSystemUpdateFolderName(root_folder)) {
			if (root_folder[1] == rough_region || (console_is_PSX && !stricmp(root_folder, "BIEXEC-SYSTEM")))
				return FILER_EXPLOIT_PROTECT_THIS_CONSOLE;
			return FILER_EXPLOIT_PROTECT_OTHER_REGION;
		}
		if (filerIsMcRootExploitFolderName(root_folder))
			return FILER_EXPLOIT_PROTECT_GENERIC;
	} else if (is_xfrom && !stricmp(root_folder, "BIEXEC-SYSTEM")) {
		return console_is_PSX ? FILER_EXPLOIT_PROTECT_THIS_CONSOLE : FILER_EXPLOIT_PROTECT_GENERIC;
	}

	if (folder != NULL && folder_size > 0)
		folder[0] = '\0';
	return FILER_EXPLOIT_PROTECT_NONE;
}

int filerIsExploitProtectedPath(const char *path, const FILEINFO *file)
{
	return (filerGetExploitProtectionType(path, file, NULL, 0) != FILER_EXPLOIT_PROTECT_NONE);
}

static int filerConfirmExploitAction(const char *path, const FILEINFO *file, const char *action)
{
	char folder[40];
	char msg[256];
	int protection_type;

	protection_type = filerGetExploitProtectionType(path, file, folder, sizeof(folder));
	if (protection_type == FILER_EXPLOIT_PROTECT_NONE)
		return 1;

	if (protection_type == FILER_EXPLOIT_PROTECT_THIS_CONSOLE) {
		snprintf(msg, sizeof(msg), "%s\n%s\n%s ?",
		         folder, LNG(Exploit_Folder_This_Console_Warning), action);
	} else if (protection_type == FILER_EXPLOIT_PROTECT_OTHER_REGION) {
		snprintf(msg, sizeof(msg), "%s\n%s\n%s ?",
		         folder, LNG(Exploit_Folder_Other_Region_Warning), action);
	} else {
		snprintf(msg, sizeof(msg), "%s\n%s\n%s ?",
		         folder, LNG(Exploit_Folder_Warning), action);
	}

	return ynDialog(msg);
}

int filerConfirmExploitDelete(const char *path, const FILEINFO *file)
{
	return filerConfirmExploitAction(path, file, LNG(Delete));
}

int filerConfirmExploitModify(const char *path, const FILEINFO *file)
{
	return filerConfirmExploitAction(path, file, LNG(Modify));
}

u64 getFileSize(const char *path, const FILEINFO *file)
{
	iox_stat_t stat;
	u64 size, filesize;
	FILEINFO files[MAX_ENTRY];
	char dir[MAX_PATH], party[MAX_NAME];
	int nfiles, i, ret;

	if (!ensurePathDeviceStackReady(path))
		return 0;

	if (file->stats.AttrFile & sceMcFileAttrSubdir) {  //Folder object to size up
		sprintf(dir, "%s%s/", path, file->name);
		nfiles = getDir(dir, files);
		for (i = size = 0; i < nfiles; i++) {
			filesize = getFileSize(dir, &files[i]);  //recurse for each object in folder
			if (filesize < 0)
				return -1;
			else
				size += filesize;
		}
	} else {  //File object to size up
		if (!strncmp(path, "hdd", 3)) {
			getHddParty(path, file, party, dir);
			ret = mountParty(party);
			if (ret < 0)
				return 0;
			dir[3] = ret + '0';
#ifdef DVRP
		} else if (!strncmp(path, "dvr_hdd", 7)) {
			getHddDVRPParty(path, file, party, dir);
			ret = mountDVRPParty(party);
			if (ret < 0)
				return 0;
			dir[7] = ret + '0';
#endif
		} else
			sprintf(dir, "%s%s", path, file->name);
#if defined(ETH) || defined(UDPFS)
		if (!strncmp(dir, "host:/", 6))
			makeHostPath(dir, dir);
#endif
		fileXioGetStat(dir, &stat);
		size = stat.size;
	}
	return size;
}
//------------------------------
//endfunc getFileSize
//--------------------------------------------------------------
//
#define CUSTOM_DATE_TEXT_LEN 19

static int isCustomDateLeapYear(int year)
{
	return ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)));
}

static int getCustomDateDaysInMonth(int year, int month)
{
	static const int days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	if (month == 2)
		return days_per_month[month - 1] + isCustomDateLeapYear(year);
	return days_per_month[month - 1];
}

static int parseCustomDateValue(const char *text, int start, int digits)
{
	int i;
	int value = 0;

	for (i = 0; i < digits; i++) {
		if (text[start + i] < '0' || text[start + i] > '9')
			return -1;
		value = value * 10 + text[start + i] - '0';
	}

	return value;
}

static int parseCustomMemoryCardTimestamp(const char *text, sceMcStDateTime *timestamp)
{
	int year, month, day, hour, minute, second;

	if (strlen(text) != CUSTOM_DATE_TEXT_LEN || text[4] != '-' || text[7] != '-' ||
	    text[10] != ' ' || text[13] != ':' || text[16] != ':')
		return -1;

	year = parseCustomDateValue(text, 0, 4);
	month = parseCustomDateValue(text, 5, 2);
	day = parseCustomDateValue(text, 8, 2);
	hour = parseCustomDateValue(text, 11, 2);
	minute = parseCustomDateValue(text, 14, 2);
	second = parseCustomDateValue(text, 17, 2);
	if (year < 1 || year > 2099 || month < 1 || month > 12 || day < 1 || day > getCustomDateDaysInMonth(year, month) ||
	    hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
		return -1;

	timestamp->Resv2 = 0;
	timestamp->Year = year;
	timestamp->Month = month;
	timestamp->Day = day;
	timestamp->Hour = hour;
	timestamp->Min = minute;
	timestamp->Sec = second;
	return 0;
}

static int setMemoryCardFolderTimestamp(const char *path, const FILEINFO *file, const sceMcStDateTime *timestamp, char *message)
{
	static sceMcTblGetDir mc_dir __attribute__((aligned(64)));
	const char *folder_name;
	int result;
	int slot;

	if (path == NULL || file == NULL || timestamp == NULL || path[2] < '0' || path[2] > '1')
		return -1;

	slot = path[2] - '0';
#ifdef TMANIP_MORON
	folder_name = HACK_FOLDER;
#else
	folder_name = file->name;
#endif
	memcpy(&mc_dir, &file->stats, sizeof(mc_dir));
	mc_dir._Modify = *timestamp;
	mc_dir._Create = *timestamp;

	ensureMemoryCardPortAccessible(slot);
	mcSync(0, NULL, NULL);
	result = mcSetFileInfo(slot, 0, folder_name, &mc_dir, 0x02);
	if (result >= 0)
		mcSync(0, NULL, &result);

	if (result == 0)
		snprintf(message, MAX_PATH, "success, folder [%s] Mc Slot [%d].", folder_name, slot);
	else
		snprintf(message, MAX_PATH, "error [%d], folder [%s] Mc Slot [%d].", result, folder_name, slot);

	return result;
}

void time_manip(const char *path, const FILEINFO *file, char *message)
{
	sceMcStDateTime timestamp;

	timestamp.Resv2 = 0;
	timestamp.Sec = 59;
	timestamp.Min = 59;
	timestamp.Hour = 23;
	timestamp.Day = 31;
	timestamp.Month = 12;
	timestamp.Year = 2099;
	setMemoryCardFolderTimestamp(path, file, &timestamp, message);
}

int time_manip_custom(const char *path, const FILEINFO *file, char *message)
{
	const PS2TIME *current_timestamp;
	sceMcStDateTime timestamp;
	char date_text[CUSTOM_DATE_TEXT_LEN + 1];
	int current_year;
	int result;

	if (path == NULL || file == NULL)
		return -1;

	current_timestamp = (const PS2TIME *)&file->stats._Modify;
	current_year = current_timestamp->year;
	if (current_year < 1 || current_year > 2099 || current_timestamp->month < 1 || current_timestamp->month > 12 ||
	    current_timestamp->day < 1 || current_timestamp->day > getCustomDateDaysInMonth(current_year, current_timestamp->month) ||
	    current_timestamp->hour > 23 || current_timestamp->min > 59 || current_timestamp->sec > 59) {
		snprintf(date_text, sizeof(date_text), "2000-01-01 00:00:00");
	} else {
		snprintf(date_text, sizeof(date_text), "%04d-%02d-%02d %02d:%02d:%02d", current_year,
		         current_timestamp->month, current_timestamp->day, current_timestamp->hour,
		         current_timestamp->min, current_timestamp->sec);
	}

	drawMsg("Enter date: YYYY-MM-DD HH:MM:SS");
	if (keyboard(date_text, CUSTOM_DATE_TEXT_LEN) < 0)
		return 0;
	if (parseCustomMemoryCardTimestamp(date_text, &timestamp) < 0) {
		snprintf(message, MAX_PATH, "Invalid date. Use YYYY-MM-DD HH:MM:SS");
		return -1;
	}

	result = setMemoryCardFolderTimestamp(path, file, &timestamp, message);
	return result == 0 ? 1 : -1;
}

void make_title_cfg(const char *path, const FILEINFO *file, char *_msg0)
{
	int fd;
	char title_cfg_buffer[2 * MAX_NAME + 16], ELF_NAME[MAX_NAME];

	snprintf(ELF_NAME, sizeof(ELF_NAME), "%s", file->name);
	ELF_NAME[strlen(ELF_NAME) - 4] = '\0';  //kill extension, we can do this freely without checking string length because feature is only enabled on .ELF files
	snprintf(title_cfg_buffer, sizeof(title_cfg_buffer), "title=%s\nboot=%s", ELF_NAME, file->name);
	char new_title_cfg[MAX_PATH];
	strcpy(new_title_cfg, path);
	strcat(new_title_cfg, "title.cfg");
	if ((fd = genOpen(new_title_cfg, FIO_O_CREAT | FIO_O_WRONLY | FIO_O_TRUNC)) < 0) {
		snprintf(_msg0, MAX_PATH, "Error opening title.cfg");
		return;
	} else {
		genWrite(fd, title_cfg_buffer, strlen(title_cfg_buffer));
		genClose(fd);
	}

}
//------------------------------
//endfunc make_title_cfg
//--------------------------------------------------------------
int delete (const char *path, const FILEINFO *file)
{
	FILEINFO files[MAX_ENTRY];
	char party[MAX_NAME], dir[MAX_PATH], hdddir[MAX_PATH];
	int nfiles, i, ret;

	if (!ensurePathDeviceStackReady(path))
		return -1;

	if (!strncmp(path, "hdd", 3)) {
		if (getHddParty(path, file, party, hdddir) < 0)
			return -1;
		ret = mountParty(party);
		if (ret < 0)
			return -1;
		hdddir[3] = ret + '0';
#ifdef DVRP
	} else if (!strncmp(path, "dvr_hdd", 7)) {
		if (getHddDVRPParty(path, file, party, hdddir) < 0)
			return -1;
		ret = mountDVRPParty(party);
		if (ret < 0)
			return -1;
		hdddir[7] = ret + '0';
#endif
	}
	sprintf(dir, "%s%s", path, file->name);
	genLimObjName(dir, 0);
#if defined(ETH) || defined(UDPFS)
	if (!strncmp(dir, "host:/", 6))
		makeHostPath(dir, dir);
#endif
	if (file->stats.AttrFile & sceMcFileAttrSubdir) {  //Is the object to delete a folder ?
		strcat(dir, "/");
		nfiles = getDir(dir, files);
		for (i = 0; i < nfiles; i++) {
			ret = delete (dir, &files[i]);  //recursively delete contents of folder
			if (ret < 0)
				return -1;
		}
		if (!strncmp(dir, "mc", 2)) {
			ensureMemoryCardPortAccessible(dir[2] - '0');
			mcSync(0, NULL, NULL);
			mcDelete(dir[2] - '0', 0, &dir[4]);
			mcSync(0, NULL, &ret);
#ifdef XFROM
		} else if (!strncmp(dir, "xfrom", 5)) {
			xfromSync(0, NULL, NULL);
			xfromDelete(0, 0, getXfromRelativePath(dir));
			xfromSync(0, NULL, &ret);
#endif
		} else if (!strncmp(path, "hdd", 3) || !strncmp(path, "dvr_hdd", 7)) {
			ret = fileXioRmdir(hdddir);
		} else if (!strncmp(path, "vmc", 3)) {
			ret = genRmdir(dir);

		} else {  //For all other devices
			sprintf(dir, "%s%s", path, file->name);
			ret = genRmdir(dir);
		}
	} else {  //The object to delete is a file
		if (!strncmp(path, "mc", 2)) {
			ensureMemoryCardPortAccessible(path[2] - '0');
			mcSync(0, NULL, NULL);
			mcDelete(dir[2] - '0', 0, &dir[4]);
			mcSync(0, NULL, &ret);
#ifdef XFROM
		} else if (!strncmp(path, "xfrom", 5)) {
			xfromSync(0, NULL, NULL);
			xfromDelete(0, 0, getXfromRelativePath(dir));
			xfromSync(0, NULL, &ret);
#endif
		} else if (!strncmp(path, "hdd", 3) || !strncmp(path, "dvr_hdd", 7)) {
			ret = fileXioRemove(hdddir);
		} else if (!strncmp(path, "vmc", 3)) {
			ret = genRemove(dir);
		} else {  //For all other devices
			ret = genRemove(dir);
		}
	}
	return ret;
}
//--------------------------------------------------------------
int Rename(const char *path, const FILEINFO *file, const char *name)
{
	char party[MAX_NAME], oldPath[MAX_PATH], newPath[MAX_PATH];
	int test, ret = 0;

	if (!ensurePathDeviceStackReady(path))
		return -1;

	if (filerIsExploitProtectedPath(path, file))
		return -EPERM;

	if (!strncmp(path, "hdd", 3)) {
		if (getHddParty(path, NULL, party, oldPath) < 0)
			return -1;
		sprintf(newPath, "%s%s", oldPath, name);
		strcat(oldPath, file->name);

		ret = mountParty(party);
		if (ret < 0)
			return -1;
		oldPath[3] = newPath[3] = ret + '0';
		ret = fileXioRename(oldPath, newPath);
#ifdef DVRP
	} else if (!strncmp(path, "dvr_hdd", 7)) {
		sprintf(party, "dvr_hdd0:%s", &path[10]);
		*strchr(party, '/') = 0;
		sprintf(oldPath, "dvr_pfs0:%s", strchr(&path[10], '/') + 1);
		sprintf(newPath, "%s%s", oldPath, name);
		strcat(oldPath, file->name);

		ret = mountDVRPParty(party);
		if (ret < 0)
			return -1;
		oldPath[7] = newPath[7] = ret + '0';
		ret = fileXioRename(oldPath, newPath);
#endif
	} else if (!strncmp(path, "mc", 2)) {
		ensureMemoryCardPortAccessible(path[2] - '0');
		sprintf(oldPath, "%s%s", path, file->name);
		sprintf(newPath, "%s%s", path, name);
		if ((test = fileXioDopen(newPath)) >= 0) {  //Does folder of same name exist ?
			fileXioDclose(test);
			ret = -EEXIST;
		} else if ((test = fileXioOpen(newPath, FIO_O_RDONLY, 0)) >= 0) {  //Does file of same name exist ?
			fileXioClose(test);
			ret = -EEXIST;
		} else {  //No file/folder of the same name exists
			mcGetInfo(path[2] - '0', 0, &mctype_PSx, NULL, NULL);
			mcSync(0, NULL, &test);
			if (mctype_PSx == 2)  //PS2 MC ?
				snprintf((char *)file->stats.EntryName, 32, "%.31s", name);
			mcSetFileInfo(path[2] - '0', 0, oldPath + 4, &file->stats, 0x0010);  //Fix file stats
			mcSync(0, NULL, &test);
			if (ret == -4)
				ret = -EEXIST;
			else {  //PS1 MC !
				snprintf((char *)file->stats.EntryName, 32, "%.31s", name);
				mcSetFileInfo(path[2] - '0', 0, oldPath + 4, &file->stats, 0x0010);  //Fix file stats
				mcSync(0, NULL, &test);
				if (ret == -4)
					ret = -EEXIST;
			}
		}
#ifdef XFROM
	} else if (!strncmp(path, "xfrom", 5)) {
		sprintf(oldPath, "%s%s", path, file->name);
		sprintf(newPath, "%s%s", path, name);
		if ((test = fileXioDopen(newPath)) >= 0) {  //Does folder of same name exist ?
			fileXioDclose(test);
			ret = -EEXIST;
		} else if ((test = fileXioOpen(newPath, FIO_O_RDONLY, 0)) >= 0) {  //Does file of same name exist ?
			fileXioClose(test);
			ret = -EEXIST;
		} else {  //No file/folder of the same name exists
			xfromGetInfo(0, 0, &mctype_PSx, NULL, NULL);
			xfromSync(0, NULL, &test);
			if (mctype_PSx == 2)  //PS2 MC ?
				snprintf((char *)file->stats.EntryName, 32, "%.31s", name);
			xfromSetFileInfo(0, 0, getXfromRelativePath(oldPath), &file->stats, 0x0010);  //Fix file stats
			xfromSync(0, NULL, &ret);
			if (ret == -4)
				ret = -EEXIST;
			else if (mctype_PSx != 2) {  //PS1 MC !
				snprintf((char *)file->stats.EntryName, 32, "%.31s", name);
				xfromSetFileInfo(0, 0, getXfromRelativePath(oldPath), &file->stats, 0x0010);  //Fix file stats
				xfromSync(0, NULL, &ret);
				if (ret == -4)
					ret = -EEXIST;
			}
		}
#endif
#if defined(ETH) || defined(UDPFS)
	} else if (!strncmp(path, "host", 4) || !strncmp(path, "udpfs", 5)) {
		snprintf(oldPath, sizeof(oldPath), "%s%s", path, file->name);
		snprintf(newPath, sizeof(newPath), "%s%s", path, name);
		if (filerPathExistsForConflict(newPath)) {
			ret = -EEXIST;
		} else {
			makeHostPath(oldPath, oldPath);
			makeHostPath(newPath, newPath);
			ret = fileXioRename(oldPath, newPath);
		}
#endif
	} else {  //For all other devices
		sprintf(oldPath, "%s%s", path, file->name);
		sprintf(newPath, "%s%s", path, name);
		if (filerPathExistsForConflict(newPath))
			ret = -EEXIST;
		else
			ret = fileXioRename(oldPath, newPath);
	}

	return ret;
}

//--------------------------------------------------------------
int newdir(const char *path, const char *name)
{
	char party[MAX_NAME], dir[MAX_PATH];
	int ret = 0;

	if (!ensurePathDeviceStackReady(path))
		return -1;

	if (!strncmp(path, "hdd", 3)) {
		getHddParty(path, NULL, party, dir);
		ret = mountParty(party);
		if (ret < 0)
			return -1;
		dir[3] = ret + '0';
		strcat(dir, name);
		genLimObjName(dir, 0);
		ret = fileXioMkdir(dir, fileMode);
#ifdef DVRP
	} else if (!strncmp(path, "dvr_hdd", 7)) {
		getHddDVRPParty(path, NULL, party, dir);
		ret = mountDVRPParty(party);
		if (ret < 0)
			return -1;
		dir[7] = ret + '0';
		strcat(dir, name);
		genLimObjName(dir, 0);
		ret = fileXioMkdir(dir, fileMode);
#endif
	} else if (!strncmp(path, "vmc", 3)) {
		strcpy(dir, path);
		strcat(dir, name);
		genLimObjName(dir, 0);
		ret = filerMkdirNoOverwrite(dir);
	} else if (!strncmp(path, "mc", 2)) {
		ensureMemoryCardPortAccessible(path[2] - '0');
		sprintf(dir, "%s%s", path + 4, name);
		genLimObjName(dir, 0);
		mcSync(0, NULL, NULL);
		mcMkDir(path[2] - '0', 0, dir);
		mcSync(0, NULL, &ret);
		if (ret == -4)
			ret = -EEXIST;  //return fileXio error code for pre-existing folder
#ifdef XFROM
	} else if (!strncmp(path, "xfrom", 5)) {
		snprintf(dir, sizeof(dir), "%s%s", getXfromRelativePath(path), name);
		genLimObjName(dir, 0);
		xfromSync(0, NULL, NULL);
		xfromMkDir(0, 0, dir);
		xfromSync(0, NULL, &ret);
		if (ret == -4)
			ret = -EEXIST;  //return fileXio error code for pre-existing folder
#endif
#if defined(ETH) || defined(UDPFS)
	} else if (!strncmp(path, "host", 4) || !strncmp(path, "udpfs", 5)) {
		strcpy(dir, path);
		strcat(dir, name);
		genLimObjName(dir, 0);
		ret = filerMkdirNoOverwrite(dir);
#endif
	} else {  //For all other devices
		strcpy(dir, path);
		strcat(dir, name);
		genLimObjName(dir, 0);
		ret = filerMkdirNoOverwrite(dir);
	}
	return ret;
}
//--------------------------------------------------------------
//End of file: filer_actions.c
//--------------------------------------------------------------
