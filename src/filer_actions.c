#include "launchelf.h"
#include "filer_actions.h"
#include "filer_shared.h"
#include "gui_hdd0_format.h"
#include "init.h"
#include "main_title.h"

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

static int filerNameContainsTuna(const char *name)
{
	int i;

	if (name == NULL)
		return 0;
	for (i = 0; name[i] != '\0'; i++) {
		if (name[i + 1] == '\0' || name[i + 2] == '\0' || name[i + 3] == '\0')
			break;
		if ((name[i] == 'T' || name[i] == 't') && (name[i + 1] == 'U' || name[i + 1] == 'u') &&
		    (name[i + 2] == 'N' || name[i + 2] == 'n') && (name[i + 3] == 'A' || name[i + 3] == 'a'))
			return 1;
	}

	return 0;
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
enum {
	CUSTOM_DATE_HOUR,
	CUSTOM_DATE_MINUTE,
	CUSTOM_DATE_SECOND,
	CUSTOM_DATE_YEAR,
	CUSTOM_DATE_MONTH,
	CUSTOM_DATE_DAY,
	CUSTOM_DATE_FIELD_COUNT
};

enum {
	TIMESTAMP_ORGANIZE_MANUAL,
	TIMESTAMP_ORGANIZE_AZ,
	TIMESTAMP_ORGANIZE_ZA,
	TIMESTAMP_ORGANIZE_SAS,
	TIMESTAMP_ORGANIZE_COUNT
};

#define SAS_TIMESTAMP_CATEGORY_SECONDS 86400
#define SAS_TIMESTAMP_RANK_WIDTH 48
#define SAS_TIMESTAMP_BASE 40
#define TIMESTAMP_ORGANIZE_SECONDS_BETWEEN_FOLDERS 60

static FILEINFO timestamp_folders[MAX_ENTRY];
static sceMcStDateTime timestamp_original[MAX_ENTRY];

static int isMemoryCardRootPath(const char *path)
{
	return (path != NULL && (!strcmp(path, "mc0:/") || !strcmp(path, "mc1:/")));
}

static int isHddCommonPath(const char *path)
{
	static const char common_partition[] = "__common";
	const char *partition;
	size_t partition_len = sizeof(common_partition) - 1;

	if (path == NULL || strncmp(path, "hdd0:/", 6))
		return 0;

	partition = path + 6;
	return (!strncmp(partition, common_partition, partition_len) &&
	        (partition[partition_len] == '/' || partition[partition_len] == '\0'));
}

int filerCanOrganizeFolderTimestamps(const char *path)
{
	return isMemoryCardRootPath(path) || isHddCommonPath(path);
}

int filerCanSetCustomTimestamp(const char *path, const FILEINFO *file)
{
	if (file == NULL || !(file->stats.AttrFile & sceMcFileAttrSubdir) ||
	    !strcmp(file->name, ".") || !strcmp(file->name, ".."))
		return 0;

	return filerCanOrganizeFolderTimestamps(path);
}

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

static int isCustomFolderTimestampInRange(int year, int month, int day, int hour, int minute, int second)
{
	if (year < 1 || year > 2099 || month < 1 || month > 12 || day < 1 || day > getCustomDateDaysInMonth(year, month) ||
	    hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
		return 0;

	return 1;
}

static int convertCustomTimestampToLocalTime(sceMcStDateTime *timestamp)
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;

	if (timestamp == NULL)
		return 0;

	year = timestamp->Year;
	month = timestamp->Month;
	day = timestamp->Day;
	hour = timestamp->Hour;
	minute = timestamp->Min;
	second = timestamp->Sec;
	if (!menuTitleConvertTimestampToLocalTime(&year, &month, &day, &hour, &minute, &second))
		return 0;

	timestamp->Year = year;
	timestamp->Month = month;
	timestamp->Day = day;
	timestamp->Hour = hour;
	timestamp->Min = minute;
	timestamp->Sec = second;
	return 1;
}

static int convertCustomTimestampFromLocalTime(sceMcStDateTime *timestamp)
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;

	if (timestamp == NULL)
		return 0;

	year = timestamp->Year;
	month = timestamp->Month;
	day = timestamp->Day;
	hour = timestamp->Hour;
	minute = timestamp->Min;
	second = timestamp->Sec;
	if (!menuTitleConvertTimestampFromLocalTime(&year, &month, &day, &hour, &minute, &second))
		return 0;

	timestamp->Year = year;
	timestamp->Month = month;
	timestamp->Day = day;
	timestamp->Hour = hour;
	timestamp->Min = minute;
	timestamp->Sec = second;
	return 1;
}

static void normalizeCustomFolderTimestamp(sceMcStDateTime *timestamp, int reserve_tuna_date)
{
	if (timestamp->Year < 1)
		timestamp->Year = 1;
	else if (timestamp->Year > 2099)
		timestamp->Year = 2099;
	if (timestamp->Month < 1)
		timestamp->Month = 1;
	else if (timestamp->Month > 12)
		timestamp->Month = 12;
	if (timestamp->Day < 1)
		timestamp->Day = 1;
	else if (timestamp->Day > getCustomDateDaysInMonth(timestamp->Year, timestamp->Month))
		timestamp->Day = getCustomDateDaysInMonth(timestamp->Year, timestamp->Month);
	if (timestamp->Hour > 23)
		timestamp->Hour = 23;
	if (timestamp->Min > 59)
		timestamp->Min = 59;
	if (timestamp->Sec > 59)
		timestamp->Sec = 59;

	/* Set *Tuna Date reserves this timestamp on memory cards only. */
	if (reserve_tuna_date && timestamp->Year == 2099 && timestamp->Month == 12 && timestamp->Day == 31 &&
	    timestamp->Hour == 23 && timestamp->Min == 59 && timestamp->Sec == 59)
		 timestamp->Sec = 58;
}

static void setTunaFolderTimestamp(sceMcStDateTime *timestamp)
{
	timestamp->Resv2 = 0;
	timestamp->Year = 2099;
	timestamp->Month = 12;
	timestamp->Day = 31;
	timestamp->Hour = 23;
	timestamp->Min = 59;
	timestamp->Sec = 59;
}

static void normalizeCustomLocalTimestamp(sceMcStDateTime *timestamp)
{
	if (timestamp->Year > 2100)
		timestamp->Year = 2100;
	if (timestamp->Month < 1)
		timestamp->Month = 1;
	else if (timestamp->Month > 12)
		timestamp->Month = 12;
	if (timestamp->Day < 1)
		timestamp->Day = 1;
	else if (timestamp->Day > getCustomDateDaysInMonth(timestamp->Year, timestamp->Month))
		timestamp->Day = getCustomDateDaysInMonth(timestamp->Year, timestamp->Month);
	if (timestamp->Hour > 23)
		timestamp->Hour = 23;
	if (timestamp->Min > 59)
		timestamp->Min = 59;
	if (timestamp->Sec > 59)
		timestamp->Sec = 59;
}

static void stepCustomFolderTimestamp(sceMcStDateTime *timestamp, int delta, int reserve_tuna_date)
{
	normalizeCustomFolderTimestamp(timestamp, reserve_tuna_date);

	if (delta > 0) {
		if (timestamp->Year == 2099 && timestamp->Month == 12 && timestamp->Day == 31 &&
		    timestamp->Hour == 23 && timestamp->Min == 59 &&
		    ((reserve_tuna_date && timestamp->Sec >= 58) || (!reserve_tuna_date && timestamp->Sec >= 59)))
			return;

		timestamp->Sec++;
		if (timestamp->Sec < 60)
			return;
		timestamp->Sec = 0;
		timestamp->Min++;
		if (timestamp->Min < 60)
			return;
		timestamp->Min = 0;
		timestamp->Hour++;
		if (timestamp->Hour < 24)
			return;
		timestamp->Hour = 0;
		timestamp->Day++;
		if (timestamp->Day <= getCustomDateDaysInMonth(timestamp->Year, timestamp->Month))
			return;
		timestamp->Day = 1;
		timestamp->Month++;
		if (timestamp->Month <= 12)
			return;
		timestamp->Month = 1;
		timestamp->Year++;
	} else {
		if (timestamp->Year == 1 && timestamp->Month == 1 && timestamp->Day == 1 &&
		    timestamp->Hour == 0 && timestamp->Min == 0 && timestamp->Sec == 0)
			return;

		if (timestamp->Sec > 0) {
			timestamp->Sec--;
			return;
		}
		timestamp->Sec = 59;
		if (timestamp->Min > 0) {
			timestamp->Min--;
			return;
		}
		timestamp->Min = 59;
		if (timestamp->Hour > 0) {
			timestamp->Hour--;
			return;
		}
		timestamp->Hour = 23;
		if (timestamp->Day > 1) {
			timestamp->Day--;
			return;
		}
		if (timestamp->Month > 1)
			timestamp->Month--;
		else {
			timestamp->Month = 12;
			timestamp->Year--;
		}
		timestamp->Day = getCustomDateDaysInMonth(timestamp->Year, timestamp->Month);
	}

	normalizeCustomFolderTimestamp(timestamp, reserve_tuna_date);
}

static int getCustomDateFieldForDisplayPosition(int position, int date_format)
{
	if (position < 3)
		return position;

	switch (date_format) {
		case 1:
			return (position == 3) ? CUSTOM_DATE_MONTH : (position == 4) ? CUSTOM_DATE_DAY : CUSTOM_DATE_YEAR;
		case 2:
			return (position == 3) ? CUSTOM_DATE_DAY : (position == 4) ? CUSTOM_DATE_MONTH : CUSTOM_DATE_YEAR;
		default:
			return (position == 3) ? CUSTOM_DATE_YEAR : (position == 4) ? CUSTOM_DATE_MONTH : CUSTOM_DATE_DAY;
	}
}

static int getCustomDateFieldOffset(int field, int date_format)
{
	switch (field) {
		case CUSTOM_DATE_HOUR:
			return 0;
		case CUSTOM_DATE_MINUTE:
			return 3;
		case CUSTOM_DATE_SECOND:
			return 6;
		case CUSTOM_DATE_YEAR:
			return (date_format == 0) ? 0 : 6;
		case CUSTOM_DATE_MONTH:
			return (date_format == 0) ? 5 : (date_format == 1) ? 0 : 3;
		default:
			return (date_format == 0) ? 8 : (date_format == 1) ? 3 : 0;
	}
}

static void adjustCustomDateField(sceMcStDateTime *timestamp, int field, int delta, int reserve_tuna_date)
{
	sceMcStDateTime local_timestamp;
	int maximum_day;

	local_timestamp = *timestamp;
	if (!convertCustomTimestampToLocalTime(&local_timestamp))
		return;

	switch (field) {
		case CUSTOM_DATE_HOUR:
			local_timestamp.Hour = (local_timestamp.Hour + ((delta > 0) ? 1 : 23)) % 24;
			break;
		case CUSTOM_DATE_MINUTE:
			local_timestamp.Min = (local_timestamp.Min + ((delta > 0) ? 1 : 59)) % 60;
			break;
		case CUSTOM_DATE_SECOND:
			local_timestamp.Sec = (local_timestamp.Sec + ((delta > 0) ? 1 : 59)) % 60;
			break;
		case CUSTOM_DATE_YEAR:
			if (delta > 0)
				local_timestamp.Year = (local_timestamp.Year == 2100) ? 0 : local_timestamp.Year + 1;
			else
				local_timestamp.Year = (local_timestamp.Year == 0) ? 2100 : local_timestamp.Year - 1;
			break;
		case CUSTOM_DATE_MONTH:
			if (delta > 0)
				local_timestamp.Month = (local_timestamp.Month == 12) ? 1 : local_timestamp.Month + 1;
			else
				local_timestamp.Month = (local_timestamp.Month == 1) ? 12 : local_timestamp.Month - 1;
			break;
		default:
			maximum_day = getCustomDateDaysInMonth(local_timestamp.Year, local_timestamp.Month);
			if (delta > 0)
				local_timestamp.Day = (local_timestamp.Day == maximum_day) ? 1 : local_timestamp.Day + 1;
			else
				local_timestamp.Day = (local_timestamp.Day == 1) ? maximum_day : local_timestamp.Day - 1;
			break;
	}

	normalizeCustomLocalTimestamp(&local_timestamp);
	if (!convertCustomTimestampFromLocalTime(&local_timestamp))
		return;
	if (!isCustomFolderTimestampInRange(local_timestamp.Year, local_timestamp.Month, local_timestamp.Day,
	                                    local_timestamp.Hour, local_timestamp.Min, local_timestamp.Sec))
		return;
	normalizeCustomFolderTimestamp(&local_timestamp, reserve_tuna_date);
	*timestamp = local_timestamp;
}

static int compareCustomDateEditorFolders(const FILEINFO *left, const FILEINFO *right)
{
	const sceMcStDateTime *left_time = &left->stats._Modify;
	const sceMcStDateTime *right_time = &right->stats._Modify;

	if (left_time->Year != right_time->Year)
		return (left_time->Year > right_time->Year) ? -1 : 1;
	if (left_time->Month != right_time->Month)
		return (left_time->Month > right_time->Month) ? -1 : 1;
	if (left_time->Day != right_time->Day)
		return (left_time->Day > right_time->Day) ? -1 : 1;
	if (left_time->Hour != right_time->Hour)
		return (left_time->Hour > right_time->Hour) ? -1 : 1;
	if (left_time->Min != right_time->Min)
		return (left_time->Min > right_time->Min) ? -1 : 1;
	if (left_time->Sec != right_time->Sec)
		return (left_time->Sec > right_time->Sec) ? -1 : 1;

	return stricmp(left->name, right->name);
}

static int customDateEditorTimestampEqual(const sceMcStDateTime *left, const sceMcStDateTime *right)
{
	return (left->Year == right->Year && left->Month == right->Month && left->Day == right->Day &&
	        left->Hour == right->Hour && left->Min == right->Min && left->Sec == right->Sec);
}

static void sortCustomDateEditorFolders(FILEINFO *folders, sceMcStDateTime *original_timestamps, int count)
{
	FILEINFO folder;
	sceMcStDateTime original_timestamp;
	int i, j;

	for (i = 1; i < count; i++) {
		folder = folders[i];
		original_timestamp = original_timestamps[i];
		for (j = i; j > 0 && compareCustomDateEditorFolders(&folder, &folders[j - 1]) < 0; j--) {
			folders[j] = folders[j - 1];
			original_timestamps[j] = original_timestamps[j - 1];
		}
		folders[j] = folder;
		original_timestamps[j] = original_timestamp;
	}
}

static int loadCustomDateEditorFolders(const char *path, FILEINFO *folders, sceMcStDateTime *original_timestamps)
{
	int count;
	int i;
	int folder_count;

	count = getDir(path, folders);
	if (count < 0)
		return 0;

	for (i = folder_count = 0; i < count; i++) {
		if (!(folders[i].stats.AttrFile & sceMcFileAttrSubdir))
			continue;
		if (!strcmp(folders[i].name, ".") || !strcmp(folders[i].name, ".."))
			continue;
		if (folder_count != i)
			folders[folder_count] = folders[i];
		original_timestamps[folder_count] = folders[folder_count].stats._Modify;
		folder_count++;
	}

	return folder_count;
}

static int updateCustomDateEditorFolders(FILEINFO *folders, sceMcStDateTime *original_timestamps, int folder_count, const char *edited_name, const sceMcStDateTime *timestamp)
{
	int i;

	for (i = 0; i < folder_count; i++) {
		if (!stricmp(folders[i].name, edited_name)) {
			folders[i].stats._Modify = *timestamp;
			break;
		}
	}
	if (i == folder_count)
		return -1;

	sortCustomDateEditorFolders(folders, original_timestamps, folder_count);
	for (i = 0; i < folder_count; i++) {
		if (!stricmp(folders[i].name, edited_name))
			return i;
	}

	return -1;
}

static int moveCustomDateEditorNextToFolder(FILEINFO *folders, sceMcStDateTime *original_timestamps, int folder_count, const char *edited_name,
	                                            sceMcStDateTime *timestamp, int newer, int reserve_tuna_date)
{
	int editing_index;
	int reference_index;

	editing_index = updateCustomDateEditorFolders(folders, original_timestamps, folder_count, edited_name, timestamp);
	if (editing_index < 0)
		return 0;

	reference_index = editing_index + (newer ? -1 : 1);
	if (reference_index < 0 || reference_index >= folder_count)
		return 0;

	*timestamp = folders[reference_index].stats._Modify;
	stepCustomFolderTimestamp(timestamp, newer ? 1 : -1, reserve_tuna_date);
	return 1;
}

static int selectCustomDateEditorFolder(FILEINFO *folders, sceMcStDateTime *original_timestamps, int folder_count, char *selected_name,
	                                      size_t selected_name_size, sceMcStDateTime *timestamp, int direction)
{
	int editing_index;
	int next_index;

	editing_index = updateCustomDateEditorFolders(folders, original_timestamps, folder_count, selected_name, timestamp);
	if (editing_index < 0)
		return 0;

	next_index = editing_index + direction;
	if (next_index < 0 || next_index >= folder_count)
		return 0;

	snprintf(selected_name, selected_name_size, "%s", folders[next_index].name);
	*timestamp = folders[next_index].stats._Modify;
	return 1;
}

static int getCustomDateEditorListTop(int selected, int count, int rows)
{
	int center_row;
	int max_top;
	int top;

	if (rows <= 0 || count <= rows)
		return 0;

	center_row = (rows - 1) / 2;
	max_top = count - rows;
	top = selected - center_row;
	if (top < 0)
		return 0;
	if (top > max_top)
		return max_top;

	return top;
}

static void formatCustomDateEditorTimestamp(char *dst, size_t dst_size, const sceMcStDateTime *timestamp, int use_12h, int date_format)
{
	char date_text[16];
	char time_text[16];
	sceMcStDateTime local_timestamp;

	local_timestamp = *timestamp;
	convertCustomTimestampToLocalTime(&local_timestamp);
	menuTitleFormatClockTime(time_text, sizeof(time_text), local_timestamp.Hour, local_timestamp.Min, local_timestamp.Sec, use_12h);
	menuTitleFormatClockDate(date_text, sizeof(date_text), local_timestamp.Year, local_timestamp.Month, local_timestamp.Day, date_format);
	snprintf(dst, dst_size, "%s %s", time_text, date_text);
}

static void drawCustomDateEditorFolderRow(const FILEINFO *folder, int x, int y, int details_column, int use_12h, int date_format, int selected)
{
	char details_text[48];
	char folder_name[MAX_NAME + 2];
	char timestamp_text[32];
	int color;
	int name_end;
	int name_limit;

	color = setting->color[selected ? COLOR_SELECT : COLOR_TEXT];
	name_limit = (details_column - 1) * FONT_WIDTH;
	snprintf(folder_name, sizeof(folder_name), "%s/", folder->name);
	name_end = name_limit / 7 - 1;
	if (name_end > 1 && strlen(folder_name) > name_end) {
		folder_name[name_end - 1] = '~';
		folder_name[name_end] = '\0';
	}

	formatCustomDateEditorTimestamp(timestamp_text, sizeof(timestamp_text), &folder->stats._Modify, use_12h, date_format);
	snprintf(details_text, sizeof(details_text), "    - B %s", timestamp_text);
	printXY(folder_name, x + 4, y, color, TRUE, name_limit);
	printXY(details_text, x + 4 + details_column * FONT_WIDTH, y, color, TRUE, 0);
	if (!setting->FB_NoIcons) {
		drawChar(ICON_FOLDER, x - 3 - FONT_WIDTH, y, setting->color[COLOR_GRAPH1]);
		drawChar(ICON_FOLDER + 1, x - 3, y, setting->color[COLOR_GRAPH1]);
	}
}

static int editCustomFolderTimestamp(const FILEINFO *file, FILEINFO *folders, sceMcStDateTime *original_timestamps, int folder_count,
	                                   sceMcStDateTime *timestamp, int reserve_tuna_date)
{
	char selected_name[MAX_NAME];
	char tooltip[MAX_PATH];
	int use_12h;
	int date_format;
	int display_position = 0;
	int event = 1;
	int post_event = 0;
	int details_column;
	int editing_index;
	int field;
	int folder_rows;
	int i;
	int list_top;
	int list_end_y;
	int timestamp_x;
	int x, y, y0, y1;

	if (file == NULL || folders == NULL || original_timestamps == NULL || folder_count <= 0 || timestamp == NULL)
		return 0;

	snprintf(selected_name, sizeof(selected_name), "%s", file->name);
	if (updateCustomDateEditorFolders(folders, original_timestamps, folder_count, selected_name, timestamp) < 0)
		return 0;

	while (1) {
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_LEFT) {
				display_position = (display_position == 0) ? CUSTOM_DATE_FIELD_COUNT - 1 : display_position - 1;
				event |= 2;
			} else if (new_pad & PAD_RIGHT) {
				display_position = (display_position + 1) % CUSTOM_DATE_FIELD_COUNT;
				event |= 2;
			} else if (new_pad & PAD_CROSS) {
				menuTitleGetClockFormat(NULL, &date_format);
				adjustCustomDateField(timestamp, getCustomDateFieldForDisplayPosition(display_position, date_format), 1, reserve_tuna_date);
				event |= 2;
			} else if (new_pad & PAD_CIRCLE) {
				menuTitleGetClockFormat(NULL, &date_format);
				adjustCustomDateField(timestamp, getCustomDateFieldForDisplayPosition(display_position, date_format), -1, reserve_tuna_date);
				event |= 2;
			} else if (new_pad & PAD_UP) {
				if (selectCustomDateEditorFolder(folders, original_timestamps, folder_count, selected_name, sizeof(selected_name), timestamp, -1))
					event |= 2;
			} else if (new_pad & PAD_DOWN) {
				if (selectCustomDateEditorFolder(folders, original_timestamps, folder_count, selected_name, sizeof(selected_name), timestamp, 1))
					event |= 2;
			} else if (new_pad & PAD_L1) {
				if (moveCustomDateEditorNextToFolder(folders, original_timestamps, folder_count, selected_name, timestamp, TRUE, reserve_tuna_date))
					event |= 2;
			} else if (new_pad & PAD_R1) {
				if (moveCustomDateEditorNextToFolder(folders, original_timestamps, folder_count, selected_name, timestamp, FALSE, reserve_tuna_date))
					event |= 2;
			} else if (new_pad & PAD_START) {
				updateCustomDateEditorFolders(folders, original_timestamps, folder_count, selected_name, timestamp);
				return 1;
			} else if (new_pad & PAD_TRIANGLE) {
				return 0;
			}
		}

		if (event || post_event) {
			menuTitleGetClockFormat(&use_12h, &date_format);
			snprintf(tooltip, sizeof(tooltip), "\xFF" "<\xFF" ":" ":%s \xFF" "1:%s \xFF" "0:%s L1:%s R1:%s START:%s \xFF" "3:%s",
			         LNG(Select), LNG(Add), LNG(Subtract), LNG(Up), LNG(Down), LNG(Set), LNG(Return));
			editing_index = updateCustomDateEditorFolders(folders, original_timestamps, folder_count, selected_name, timestamp);
			details_column = use_12h ? 41 : 44;
			list_end_y = Menu_end_y;
			folder_rows = (list_end_y - Menu_start_y) / FONT_HEIGHT - 2;
			if (folder_rows < 1)
				folder_rows = 1;
			list_top = (editing_index >= 0) ? getCustomDateEditorListTop(editing_index, folder_count, folder_rows) : 0;

			clrScr(setting->color[COLOR_BACKGR]);
			setScrTmp(LNG(Set_Custom_Date), tooltip);
			x = Menu_start_x;
			y = Menu_start_y;
			for (i = list_top; i < folder_count && i < list_top + folder_rows; i++) {
				if (i == editing_index)
					y += FONT_HEIGHT;
				drawCustomDateEditorFolderRow(&folders[i], x, y, details_column, use_12h, date_format, i == editing_index);
				if (i == editing_index) {
					char time_text[16];
					sceMcStDateTime local_timestamp;

					local_timestamp = *timestamp;
					convertCustomTimestampToLocalTime(&local_timestamp);
					menuTitleFormatClockTime(time_text, sizeof(time_text), local_timestamp.Hour, local_timestamp.Min, local_timestamp.Sec, use_12h);
					field = getCustomDateFieldForDisplayPosition(display_position, date_format);
					timestamp_x = x + 4 + details_column * FONT_WIDTH + strlen("    - B ") * FONT_WIDTH;
					if (field < CUSTOM_DATE_YEAR)
						timestamp_x += getCustomDateFieldOffset(field, date_format) * FONT_WIDTH;
					else
						timestamp_x += (strlen(time_text) + 1 + getCustomDateFieldOffset(field, date_format)) * FONT_WIDTH;
					drawChar(UP_ARROW, timestamp_x, y + FONT_HEIGHT, setting->color[COLOR_SELECT]);
				}
				y += FONT_HEIGHT;
				if (i == editing_index)
					y += FONT_HEIGHT;
			}
			if (folder_count > folder_rows) {
				drawFrame(SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 8, Frame_start_y,
				          SCREEN_WIDTH - SCREEN_MARGIN, list_end_y + 4, setting->color[COLOR_FRAME]);
				y0 = (list_end_y - Menu_start_y + 8) * ((double)list_top / folder_count);
				y1 = (list_end_y - Menu_start_y + 8) * ((double)(list_top + folder_rows) / folder_count);
				drawOpSprite(setting->color[COLOR_FRAME],
				             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 6, y0 + Menu_start_y - 4,
				             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 2, y1 + Menu_start_y - 4);
			}
		}
		drawScr();
		post_event = event;
		event = 0;
	}
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

static int setHddCommonFolderTimestamp(const char *path, const FILEINFO *file, const sceMcStDateTime *timestamp, char *message)
{
	iox_stat_t stat;
	char party[MAX_NAME], hdddir[MAX_PATH];
	int result;

	if (!isHddCommonPath(path) || file == NULL || timestamp == NULL)
		return -1;

	if (!ensurePathDeviceStackReady(path) || getHddParty(path, NULL, party, hdddir) < 0) {
		snprintf(message, MAX_PATH, "error, unable to access folder [%s].", file->name);
		return -1;
	}
	result = mountParty(party);
	if (result < 0) {
		snprintf(message, MAX_PATH, "error [%d], folder [%s].", result, file->name);
		return result;
	}
	hdddir[3] = result + '0';
	strcat(hdddir, file->name);

	memset(&stat, 0, sizeof(stat));
	memcpy(stat.ctime, timestamp, sizeof(stat.ctime));
	memcpy(stat.atime, timestamp, sizeof(stat.atime));
	memcpy(stat.mtime, timestamp, sizeof(stat.mtime));
	result = fileXioChStat(hdddir, &stat, FIO_CST_CT | FIO_CST_AT | FIO_CST_MT);

	if (result == 0)
		snprintf(message, MAX_PATH, "success, folder [%s] timestamp updated.", file->name);
	else
		snprintf(message, MAX_PATH, "error [%d], folder [%s].", result, file->name);

	return result;
}

static void drawTimestampProgress(int completed, int total, const char *folder_name)
{
	char status[MAX_PATH + 32];
	char percent[16];
	int x = SCREEN_MARGIN + FONT_WIDTH;
	int box_w = SCREEN_WIDTH - x * 2;
	int box_h = FONT_HEIGHT * 5 + 24;
	int y = (SCREEN_HEIGHT - box_h) / 2;
	int bar_x = x + 8;
	int bar_y = y + FONT_HEIGHT * 2;
	int bar_w = box_w - 16;
	int bar_h = FONT_HEIGHT + 4;
	int fill_w;
	int percent_value;

	if (total <= 0)
		total = 1;
	if (completed > total)
		completed = total;
	fill_w = (int)(((u64)completed * (u64)(bar_w - 2)) / (u64)total);
	percent_value = (completed * 100) / total;
	snprintf(percent, sizeof(percent), "%d%%", percent_value);
	snprintf(status, sizeof(status), LNG(Updating_Timestamp), (folder_name != NULL) ? folder_name : "");

	clrScr(setting->color[COLOR_BACKGR]);
	setScrTmp(LNG(Set_Custom_Date), "");
	drawPopSprite(setting->color[COLOR_BACKGR], x, y, x + box_w, y + box_h);
	drawFrame(x, y, x + box_w, y + box_h, setting->color[COLOR_FRAME]);
	drawFrame(bar_x, bar_y, bar_x + bar_w, bar_y + bar_h, setting->color[COLOR_FRAME]);
	if (fill_w > 0)
		drawSprite(setting->color[COLOR_SELECT], bar_x + 1, bar_y + 1, bar_x + 1 + fill_w, bar_y + bar_h - 1);
	printXY(percent, bar_x + (bar_w - (int)strlen(percent) * FONT_WIDTH) / 2, bar_y + 3, setting->color[COLOR_TEXT], TRUE, 0);
	printXY(status, x + 8, bar_y + bar_h + FONT_HEIGHT, setting->color[COLOR_TEXT], TRUE, box_w - 16);
	drawScr();
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
	drawTimestampProgress(0, 1, file->name);
	setMemoryCardFolderTimestamp(path, file, &timestamp, message);
	drawTimestampProgress(1, 1, file->name);
}

int time_manip_custom(const char *path, const FILEINFO *file, char *message)
{
	FILEINFO selected_file;
	const PS2TIME *current_timestamp;
	sceMcStDateTime timestamp;
	int current_year;
	int folder_count;
	int selected_index;
	int total;
	int completed;
	int updated;
	int failed;
	int i;
	int result;
	int reserve_tuna_date;

	if (!filerCanOrganizeFolderTimestamps(path))
		return -1;
	reserve_tuna_date = isMemoryCardRootPath(path);
	folder_count = loadCustomDateEditorFolders(path, timestamp_folders, timestamp_original);
	if (folder_count <= 0) {
		snprintf(message, MAX_PATH, "Unable to load folders.");
		return -1;
	}
	for (i = 0; i < folder_count; i++) {
		if (filerNameContainsTuna(timestamp_folders[i].name))
			setTunaFolderTimestamp(&timestamp_folders[i].stats._Modify);
	}

	selected_index = -1;
	if (file != NULL && (file->stats.AttrFile & sceMcFileAttrSubdir) && strcmp(file->name, ".") && strcmp(file->name, "..")) {
		for (i = 0; i < folder_count; i++) {
			if (!stricmp(timestamp_folders[i].name, file->name)) {
				selected_index = i;
				break;
			}
		}
	}
	if (selected_index < 0)
		selected_index = 0;
	selected_file = timestamp_folders[selected_index];

	current_timestamp = (const PS2TIME *)&selected_file.stats._Modify;
	current_year = current_timestamp->year;
	if (!isCustomFolderTimestampInRange(current_year, current_timestamp->month, current_timestamp->day,
	                                    current_timestamp->hour, current_timestamp->min, current_timestamp->sec)) {
		timestamp.Resv2 = 0;
		timestamp.Year = 2000;
		timestamp.Month = 1;
		timestamp.Day = 1;
		timestamp.Hour = 0;
		timestamp.Min = 0;
		timestamp.Sec = 0;
	} else {
		timestamp.Resv2 = 0;
		timestamp.Year = current_year;
		timestamp.Month = current_timestamp->month;
		timestamp.Day = current_timestamp->day;
		timestamp.Hour = current_timestamp->hour;
		timestamp.Min = current_timestamp->min;
		timestamp.Sec = current_timestamp->sec;
	}
	normalizeCustomFolderTimestamp(&timestamp, reserve_tuna_date);

	if (!editCustomFolderTimestamp(&selected_file, timestamp_folders, timestamp_original, folder_count, &timestamp, reserve_tuna_date))
		return 0;

	for (i = 0; i < folder_count; i++) {
		if (filerNameContainsTuna(timestamp_folders[i].name))
			setTunaFolderTimestamp(&timestamp_folders[i].stats._Modify);
	}

	total = 0;
	for (i = 0; i < folder_count; i++) {
		if (!customDateEditorTimestampEqual(&timestamp_folders[i].stats._Modify, &timestamp_original[i]))
			total++;
	}
	if (total > 0)
		drawTimestampProgress(0, total, NULL);

	updated = 0;
	failed = 0;
	completed = 0;
	for (i = 0; i < folder_count; i++) {
		if (customDateEditorTimestampEqual(&timestamp_folders[i].stats._Modify, &timestamp_original[i]))
			continue;
		drawTimestampProgress(completed, total, timestamp_folders[i].name);

		if (reserve_tuna_date)
			result = setMemoryCardFolderTimestamp(path, &timestamp_folders[i], &timestamp_folders[i].stats._Modify, message);
		else
			result = setHddCommonFolderTimestamp(path, &timestamp_folders[i], &timestamp_folders[i].stats._Modify, message);
		completed++;
		drawTimestampProgress(completed, total, timestamp_folders[i].name);
		if (result == 0)
			updated++;
		else
			failed++;
	}

	if (failed > 0) {
		snprintf(message, MAX_PATH, "%d folder timestamp(s) updated, %d failed.", updated, failed);
		return updated > 0 ? 1 : -1;
	}
	if (updated == 0) {
		snprintf(message, MAX_PATH, "No folder timestamps changed.");
		return 0;
	}

	snprintf(message, MAX_PATH, "%d folder timestamp(s) updated.", updated);
	return 1;
}

static int selectTimestampOrganizationMode(void)
{
	const char *items[TIMESTAMP_ORGANIZE_COUNT] = {
		LNG(Manual_Edit),
		"A-Z",
		"Z-A",
		LNG(SAS_Timestamps),
	};
	int selection = TIMESTAMP_ORGANIZE_MANUAL;
	int event = 1;
	int post_event = 0;
	int i;
	int label_width;
	int popup_x1;
	int popup_y1;
	int popup_x2;
	int popup_y2;
	int x;
	int y;

	label_width = strlen(LNG(Set_Custom_Date));
	for (i = 0; i < TIMESTAMP_ORGANIZE_COUNT; i++) {
		if ((int)strlen(items[i]) > label_width)
			label_width = strlen(items[i]);
	}
	popup_x1 = (SCREEN_WIDTH - (label_width + 5) * FONT_WIDTH) / 2;
	popup_x2 = SCREEN_WIDTH - popup_x1;
	popup_y1 = (SCREEN_HEIGHT - (TIMESTAMP_ORGANIZE_COUNT + 2) * FONT_HEIGHT) / 2;
	popup_y2 = popup_y1 + (TIMESTAMP_ORGANIZE_COUNT + 2) * FONT_HEIGHT;
	x = popup_x1 + FONT_WIDTH;
	y = popup_y1 + FONT_HEIGHT / 2;

	while (1) {
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP) {
				selection--;
				if (selection < 0)
					selection = TIMESTAMP_ORGANIZE_COUNT - 1;
				event = 1;
			} else if (new_pad & PAD_DOWN) {
				selection++;
				if (selection >= TIMESTAMP_ORGANIZE_COUNT)
					selection = 0;
				event = 1;
			} else if ((new_pad & PAD_TRIANGLE) || (!swapKeys && (new_pad & PAD_CROSS)) || (swapKeys && (new_pad & PAD_CIRCLE))) {
				return -1;
			} else if ((swapKeys && (new_pad & PAD_CROSS)) || (!swapKeys && (new_pad & PAD_CIRCLE))) {
				return selection;
			}
		}

		if (event || post_event) {
			drawPopSprite(setting->color[COLOR_BACKGR], popup_x1, popup_y1, popup_x2, popup_y2);
			drawFrame(popup_x1, popup_y1, popup_x2, popup_y2, setting->color[COLOR_FRAME]);
			printXY(LNG(Set_Custom_Date), x + FONT_WIDTH, y, setting->color[COLOR_SELECT], TRUE, 0);
			for (i = 0; i < TIMESTAMP_ORGANIZE_COUNT; i++) {
				printXY(items[i], x + FONT_WIDTH * 2, y + (i + 1) * FONT_HEIGHT, setting->color[COLOR_TEXT], TRUE, 0);
			}
			drawChar(LEFT_CUR, x, y + (selection + 1) * FONT_HEIGHT, setting->color[COLOR_SELECT]);
		}
		drawScr();
		post_event = event;
		event = 0;
	}
}

static int filerIsAutomaticTimestampExcludedFolder(const char *name)
{
	int i;

	if (name == NULL || strlen(name) < 12)
		return 0;
	if (name[0] != 'B' && name[0] != 'b')
		return 0;
	if (name[1] != 'A' && name[1] != 'a' && name[1] != 'E' && name[1] != 'e' &&
	    name[1] != 'I' && name[1] != 'i' && name[1] != 'C' && name[1] != 'c')
		return 0;
	for (i = 2; i < 6; i++) {
		if (!((name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= 'a' && name[i] <= 'z')))
			return 0;
	}
	if (name[6] != '-')
		return 0;
	for (i = 7; i < 12; i++) {
		if (name[i] < '0' || name[i] > '9')
			return 0;
	}

	return 1;
}

static int compareTimestampFolderNames(const FILEINFO *left, const FILEINFO *right)
{
	return stricmp(left->name, right->name);
}

static void sortTimestampFolderIndexes(int *indexes, int count, int descending)
{
	int index;
	int i;
	int j;

	for (i = 1; i < count; i++) {
		index = indexes[i];
		for (j = i; j > 0; j--) {
			int compare = compareTimestampFolderNames(&timestamp_folders[index], &timestamp_folders[indexes[j - 1]]);

			if ((!descending && compare >= 0) || (descending && compare <= 0))
				break;
			indexes[j] = indexes[j - 1];
		}
		indexes[j] = index;
	}
}

static void setSequentialTimestamp(sceMcStDateTime *timestamp, int position, int reserve_tuna_date)
{
	int i;

	timestamp->Resv2 = 0;
	timestamp->Year = 2099;
	timestamp->Month = 12;
	timestamp->Day = 31;
	timestamp->Hour = 23;
	timestamp->Min = 59;
	timestamp->Sec = 58;
	for (i = 0; i < position * TIMESTAMP_ORGANIZE_SECONDS_BETWEEN_FOLDERS; i++)
		stepCustomFolderTimestamp(timestamp, -1, reserve_tuna_date);
}

static char sasTimestampToUpper(char character)
{
	if (character >= 'a' && character <= 'z')
		return character - ('a' - 'A');
	return character;
}

static void buildSasEffectiveFolderName(const char *name, char *effective, size_t effective_size)
{
	char normalized[MAX_NAME + 1];
	size_t begin;
	size_t end;
	size_t i;

	if (effective_size == 0)
		return;
	effective[0] = '\0';
	if (name == NULL)
		return;

	begin = 0;
	end = strlen(name);
	while (begin < end && name[begin] == ' ')
		begin++;
	while (end > begin && name[end - 1] == ' ')
		end--;
	if (end - begin >= sizeof(normalized))
		end = begin + sizeof(normalized) - 1;
	for (i = 0; begin + i < end; i++)
		normalized[i] = sasTimestampToUpper(name[begin + i]);
	normalized[i] = '\0';

	if (!strcmp(normalized, "OSDXMB") || !strcmp(normalized, "XEBPLUS"))
		snprintf(effective, effective_size, "APP_%s", normalized);
	else if (!strcmp(normalized, "RESTART") || !strcmp(normalized, "POWEROFF"))
		snprintf(effective, effective_size, "RAA_%s", normalized);
	else if (!strcmp(normalized, "NEUTRINO"))
		snprintf(effective, effective_size, "RTE_%s", normalized);
	else if (!strcmp(normalized, "BOOT"))
		snprintf(effective, effective_size, "SYS_BOOT");
	else if (!strcmp(normalized, "EXPLOITS"))
		snprintf(effective, effective_size, "ZZY_EXPLOITS");
	else if (!strcmp(normalized, "BM") || !strcmp(normalized, "MATRIXTEAM") || !strcmp(normalized, "OPL") || !strcmp(normalized, "POPSTARTER"))
		snprintf(effective, effective_size, "ZZZ_%s", normalized);
	else
		snprintf(effective, effective_size, "%s", normalized);
}

static int getSasTimestampCategory(const char *effective)
{
	if (!strncmp(effective, "APP_", 4))
		return 0;
	if (!strcmp(effective, "APPS"))
		return 1;
	if (!strncmp(effective, "PS1_", 4))
		return 2;
	if (!strncmp(effective, "EMU_", 4))
		return 3;
	if (!strncmp(effective, "GME_", 4))
		return 4;
	if (!strncmp(effective, "DST_", 4))
		return 5;
	if (!strncmp(effective, "DBG_", 4))
		return 6;
	if (!strncmp(effective, "RAA_", 4))
		return 7;
	if (!strncmp(effective, "RTE_", 4))
		return 8;
	if (!strncmp(effective, "SYS_", 4) || !strcmp(effective, "SYS"))
		return 10;
	if (!strncmp(effective, "ZZY_", 4))
		return 11;
	if (!strncmp(effective, "ZZZ_", 4))
		return 12;
	return 9;
}

static void buildSasTimestampPayload(const char *effective, int category, char *payload, size_t payload_size)
{
	const char *source = effective;
	size_t i;
	size_t j;

	if (category == 1)
		source = "APPS";
	else if (category != 9) {
		if (!strncmp(effective, "APP_", 4) || !strncmp(effective, "PS1_", 4) || !strncmp(effective, "EMU_", 4) ||
		    !strncmp(effective, "GME_", 4) || !strncmp(effective, "DST_", 4) || !strncmp(effective, "DBG_", 4) ||
		    !strncmp(effective, "RAA_", 4) || !strncmp(effective, "RTE_", 4) || !strncmp(effective, "SYS_", 4) ||
		    !strncmp(effective, "ZZY_", 4) || !strncmp(effective, "ZZZ_", 4))
			source = effective + 4;
	}

	for (i = j = 0; source[i] != '\0' && j + 1 < payload_size; i++) {
		if (source[i] != '-')
			payload[j++] = source[i];
	}
	payload[j] = '\0';
}

static int getSasTimestampCharacterCode(char character)
{
	if (character == ' ')
		return 0;
	if (character >= '0' && character <= '9')
		return character - '0' + 1;
	if (character >= 'A' && character <= 'Z')
		return character - 'A' + 11;
	if (character == '_')
		return 37;
	if (character == '-')
		return 38;
	return 39;
}

static int getSasTimestampSlot(const char *payload)
{
	int digits[SAS_TIMESTAMP_RANK_WIDTH];
	int carry;
	int i;
	size_t length;

	length = strlen(payload);

	for (i = 0; i < SAS_TIMESTAMP_RANK_WIDTH; i++) {
		if ((size_t)i < length)
			digits[i] = getSasTimestampCharacterCode(payload[i]) + 1;
		else
			digits[i] = 0;
	}

	carry = 0;
	for (i = SAS_TIMESTAMP_RANK_WIDTH - 1; i >= 0; i--) {
		int product = digits[i] * SAS_TIMESTAMP_CATEGORY_SECONDS + carry;

		carry = product / SAS_TIMESTAMP_BASE;
	}
	if (carry >= SAS_TIMESTAMP_CATEGORY_SECONDS)
		return SAS_TIMESTAMP_CATEGORY_SECONDS - 1;
	return carry;
}

static void decrementSasTimestampDate(sceMcStDateTime *timestamp)
{
	if (timestamp->Day > 1) {
		timestamp->Day--;
		return;
	}
	if (timestamp->Month > 1)
		timestamp->Month--;
	else {
		timestamp->Month = 12;
		timestamp->Year--;
	}
	timestamp->Day = getCustomDateDaysInMonth(timestamp->Year, timestamp->Month);
}

static void setSasTimestamp(sceMcStDateTime *timestamp, int category, int slot)
{
	int remaining_seconds;
	int days;
	int time_of_day;

	timestamp->Resv2 = 0;
	timestamp->Year = 2099;
	timestamp->Month = 1;
	timestamp->Day = 1;
	timestamp->Hour = 7;
	timestamp->Min = 59;
	timestamp->Sec = 59;

	remaining_seconds = category * SAS_TIMESTAMP_CATEGORY_SECONDS + slot;
	days = remaining_seconds / SAS_TIMESTAMP_CATEGORY_SECONDS;
	remaining_seconds %= SAS_TIMESTAMP_CATEGORY_SECONDS;
	time_of_day = timestamp->Hour * 3600 + timestamp->Min * 60 + timestamp->Sec - remaining_seconds;
	if (time_of_day < 0) {
		time_of_day += SAS_TIMESTAMP_CATEGORY_SECONDS;
		days++;
	}
	while (days-- > 0)
		decrementSasTimestampDate(timestamp);
	timestamp->Hour = time_of_day / 3600;
	timestamp->Min = (time_of_day % 3600) / 60;
	timestamp->Sec = time_of_day % 60;
}

static int time_manip_automatic(const char *path, int mode, char *message)
{
	static int indexes[MAX_ENTRY];
	sceMcStDateTime timestamp;
	char effective[MAX_NAME + 8];
	char payload[MAX_NAME + 8];
	int folder_count;
	int index_count;
	int reserve_tuna_date;
	int total;
	int completed;
	int updated;
	int failed;
	int i;
	int result;
	int category;

	if (!filerCanOrganizeFolderTimestamps(path))
		return -1;

	folder_count = loadCustomDateEditorFolders(path, timestamp_folders, timestamp_original);
	if (folder_count <= 0) {
		snprintf(message, MAX_PATH, "Unable to load folders.");
		return -1;
	}

	index_count = 0;
	for (i = 0; i < folder_count; i++) {
		if (filerNameContainsTuna(timestamp_folders[i].name))
			setTunaFolderTimestamp(&timestamp_folders[i].stats._Modify);
		else if (!filerIsAutomaticTimestampExcludedFolder(timestamp_folders[i].name))
			indexes[index_count++] = i;
	}

	reserve_tuna_date = isMemoryCardRootPath(path);
	if (mode == TIMESTAMP_ORGANIZE_AZ || mode == TIMESTAMP_ORGANIZE_ZA) {
		sortTimestampFolderIndexes(indexes, index_count, mode == TIMESTAMP_ORGANIZE_ZA);
		for (i = 0; i < index_count; i++)
			setSequentialTimestamp(&timestamp_folders[indexes[i]].stats._Modify, i, reserve_tuna_date);
	} else {
		for (i = 0; i < index_count; i++) {
			buildSasEffectiveFolderName(timestamp_folders[indexes[i]].name, effective, sizeof(effective));
			category = getSasTimestampCategory(effective);
			buildSasTimestampPayload(effective, category, payload, sizeof(payload));
			setSasTimestamp(&timestamp_folders[indexes[i]].stats._Modify, category, getSasTimestampSlot(payload));
		}
	}

	total = 0;
	for (i = 0; i < folder_count; i++) {
		if (!customDateEditorTimestampEqual(&timestamp_folders[i].stats._Modify, &timestamp_original[i]))
			total++;
	}
	if (total > 0)
		drawTimestampProgress(0, total, NULL);

	updated = 0;
	failed = 0;
	completed = 0;
	for (i = 0; i < folder_count; i++) {
		if (customDateEditorTimestampEqual(&timestamp_folders[i].stats._Modify, &timestamp_original[i]))
			continue;
		drawTimestampProgress(completed, total, timestamp_folders[i].name);
		if (reserve_tuna_date)
			result = setMemoryCardFolderTimestamp(path, &timestamp_folders[i], &timestamp_folders[i].stats._Modify, message);
		else
			result = setHddCommonFolderTimestamp(path, &timestamp_folders[i], &timestamp_folders[i].stats._Modify, message);
		completed++;
		drawTimestampProgress(completed, total, timestamp_folders[i].name);
		if (result == 0)
			updated++;
		else
			failed++;
	}

	if (failed > 0) {
		snprintf(message, MAX_PATH, "%d folder timestamp(s) updated, %d failed.", updated, failed);
		return updated > 0 ? 1 : -1;
	}
	if (updated == 0) {
		snprintf(message, MAX_PATH, "No folder timestamps changed.");
		return 0;
	}

	snprintf(message, MAX_PATH, "%d folder timestamp(s) updated.", updated);
	return 1;
}

int time_manip_organize(const char *path, const FILEINFO *file, char *message)
{
	int mode;

	if (!filerCanOrganizeFolderTimestamps(path))
		return -1;

	mode = selectTimestampOrganizationMode();
	if (mode < 0)
		return 0;
	if (mode == TIMESTAMP_ORGANIZE_MANUAL)
		return time_manip_custom(path, file, message);

	return time_manip_automatic(path, mode, message);
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
