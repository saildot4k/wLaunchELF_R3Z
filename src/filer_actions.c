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

int filerCanSetCustomTimestamp(const char *path, const FILEINFO *file)
{
	if (file == NULL || !(file->stats.AttrFile & sceMcFileAttrSubdir) ||
	    !strcmp(file->name, ".") || !strcmp(file->name, ".."))
		return 0;

	return isMemoryCardRootPath(path) || isHddCommonPath(path);
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
	int maximum_day;

	switch (field) {
		case CUSTOM_DATE_HOUR:
			timestamp->Hour = (timestamp->Hour + ((delta > 0) ? 1 : 23)) % 24;
			break;
		case CUSTOM_DATE_MINUTE:
			timestamp->Min = (timestamp->Min + ((delta > 0) ? 1 : 59)) % 60;
			break;
		case CUSTOM_DATE_SECOND:
			timestamp->Sec = (timestamp->Sec + ((delta > 0) ? 1 : 59)) % 60;
			break;
		case CUSTOM_DATE_YEAR:
			if (delta > 0)
				timestamp->Year = (timestamp->Year == 2099) ? 1 : timestamp->Year + 1;
			else
				timestamp->Year = (timestamp->Year == 1) ? 2099 : timestamp->Year - 1;
			break;
		case CUSTOM_DATE_MONTH:
			if (delta > 0)
				timestamp->Month = (timestamp->Month == 12) ? 1 : timestamp->Month + 1;
			else
				timestamp->Month = (timestamp->Month == 1) ? 12 : timestamp->Month - 1;
			break;
		default:
			maximum_day = getCustomDateDaysInMonth(timestamp->Year, timestamp->Month);
			if (delta > 0)
				timestamp->Day = (timestamp->Day == maximum_day) ? 1 : timestamp->Day + 1;
			else
				timestamp->Day = (timestamp->Day == 1) ? maximum_day : timestamp->Day - 1;
			break;
	}

	normalizeCustomFolderTimestamp(timestamp, reserve_tuna_date);
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

static void sortCustomDateEditorFolders(FILEINFO *folders, int count)
{
	FILEINFO folder;
	int i, j;

	for (i = 1; i < count; i++) {
		folder = folders[i];
		for (j = i; j > 0 && compareCustomDateEditorFolders(&folder, &folders[j - 1]) < 0; j--)
			folders[j] = folders[j - 1];
		folders[j] = folder;
	}
}

static int loadCustomDateEditorFolders(const char *path, FILEINFO *folders)
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
		if (folder_count != i)
			folders[folder_count] = folders[i];
		folder_count++;
	}

	return folder_count;
}

static int updateCustomDateEditorFolders(FILEINFO *folders, int folder_count, const char *edited_name, const sceMcStDateTime *timestamp)
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

	sortCustomDateEditorFolders(folders, folder_count);
	for (i = 0; i < folder_count; i++) {
		if (!stricmp(folders[i].name, edited_name))
			return i;
	}

	return -1;
}

static int moveCustomDateEditorNextToFolder(FILEINFO *folders, int folder_count, const char *edited_name, sceMcStDateTime *timestamp, int newer, int reserve_tuna_date)
{
	int editing_index;
	int reference_index;

	editing_index = updateCustomDateEditorFolders(folders, folder_count, edited_name, timestamp);
	if (editing_index < 0)
		return 0;

	reference_index = editing_index + (newer ? -1 : 1);
	if (reference_index < 0 || reference_index >= folder_count)
		return 0;

	*timestamp = folders[reference_index].stats._Modify;
	stepCustomFolderTimestamp(timestamp, newer ? 1 : -1, reserve_tuna_date);
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

	menuTitleFormatClockTime(time_text, sizeof(time_text), timestamp->Hour, timestamp->Min, timestamp->Sec, use_12h);
	menuTitleFormatClockDate(date_text, sizeof(date_text), timestamp->Year, timestamp->Month, timestamp->Day, date_format);
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

static int editCustomFolderTimestamp(const char *path, const FILEINFO *file, sceMcStDateTime *timestamp, int reserve_tuna_date)
{
	static FILEINFO folders[MAX_ENTRY];
	char edited_name[MAX_NAME];
	char tooltip[MAX_PATH];
	int use_12h;
	int date_format;
	int display_position = 0;
	int event = 1;
	int post_event = 0;
	int details_column;
	int editing_index;
	int field;
	int folder_count;
	int folder_rows;
	int i;
	int list_top;
	int list_end_y;
	int timestamp_x;
	int x, y, y0, y1;

	if (path == NULL || file == NULL || timestamp == NULL)
		return 0;

	snprintf(edited_name, sizeof(edited_name), "%s", file->name);
	folder_count = loadCustomDateEditorFolders(path, folders);

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
			} else if (new_pad & PAD_L1) {
				if (moveCustomDateEditorNextToFolder(folders, folder_count, edited_name, timestamp, TRUE, reserve_tuna_date))
					event |= 2;
			} else if (new_pad & PAD_R1) {
				if (moveCustomDateEditorNextToFolder(folders, folder_count, edited_name, timestamp, FALSE, reserve_tuna_date))
					event |= 2;
			} else if (new_pad & PAD_START) {
				return 1;
			} else if (new_pad & PAD_TRIANGLE) {
				return 0;
			}
		}

		if (event || post_event) {
			menuTitleGetClockFormat(&use_12h, &date_format);
			snprintf(tooltip, sizeof(tooltip), "\xFF" "<\xFF" ":" ":%s \xFF" "1:%s \xFF" "0:%s L1:%s R1:%s START:%s \xFF" "3:%s",
			         LNG(Select), LNG(Add), LNG(Subtract), LNG(Up), LNG(Down), LNG(Set), LNG(Return));
			editing_index = updateCustomDateEditorFolders(folders, folder_count, edited_name, timestamp);
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

					menuTitleFormatClockTime(time_text, sizeof(time_text), timestamp->Hour, timestamp->Min, timestamp->Sec, use_12h);
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
	int current_year;
	int result;
	int reserve_tuna_date;

	if (!filerCanSetCustomTimestamp(path, file))
		return -1;
	reserve_tuna_date = isMemoryCardRootPath(path);

	current_timestamp = (const PS2TIME *)&file->stats._Modify;
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

	if (!editCustomFolderTimestamp(path, file, &timestamp, reserve_tuna_date))
		return 0;

	if (reserve_tuna_date)
		result = setMemoryCardFolderTimestamp(path, file, &timestamp, message);
	else
		result = setHddCommonFolderTimestamp(path, file, &timestamp, message);
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
