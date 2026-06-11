#include "launchelf.h"
#include "main_history.h"

#include <limits.h>

#define OSD_HISTORY_MAX_ENTRIES 21
#define OSD_HISTORY_FILE_SIZE (OSD_HISTORY_MAX_ENTRIES * sizeof(OSDHistoryEntry))
#define OSD_HISTORY_SET_DATE(year, month, day) (((u16)(year)) << 9 | (((u16)(month) & 0xF) << 5) | ((day) & 0x1F))

typedef struct
{
	char titleID[16];
	u8 launchCount;
	u8 bitmask;
	u8 shiftAmount;
	u8 padding;
	u16 timestamp;
} OSDHistoryEntry;

static char osd_history_path[] = "mcX:/BXDATA-SYSTEM/history";

static int bcdToInt(u8 value)
{
	return ((value >> 4) * 10) + (value & 0x0F);
}

static char getHistoryRegionLetter(void)
{
	uLE_InitializeRegion();

	switch (ROMVER_data[4]) {
		case 'C':
			return 'C';
		case 'E':
			return 'E';
		case 'J':
			return 'I';
		default:
			return 'A';
	}
}

static u16 getHistoryTimestamp(void)
{
	sceCdCLOCK time;

	memset(&time, 0, sizeof(time));
	if (sceCdReadClock(&time) == 0 || time.stat != 0)
		return 0;

	return OSD_HISTORY_SET_DATE(bcdToInt(time.year), bcdToInt(time.month & 0x7F), bcdToInt(time.day));
}

static int ensureHistoryDirectory(int port)
{
	char dir[32];
	int ret;

	snprintf(dir, sizeof(dir), "B%cDATA-SYSTEM", osd_history_path[6]);
	mcSync(0, NULL, NULL);
	mcMkDir(port, 0, dir);
	mcSync(0, NULL, &ret);

	return (ret == 0 || ret == -4) ? 0 : ret;
}

static int appendEvictedHistoryEntry(const OSDHistoryEntry *entry)
{
	char old_path[64];
	int fd;
	int ret;

	snprintf(old_path, sizeof(old_path), "%s.old", osd_history_path);
	fd = genOpen(old_path, FIO_O_WRONLY | FIO_O_CREAT);
	if (fd < 0)
		return fd;

	ret = (genLseek(fd, 0, SEEK_END) >= 0 && genWrite(fd, (void *)entry, sizeof(*entry)) == sizeof(*entry)) ? 0 : -EIO;
	genClose(fd);
	return ret;
}

static void processHistoryList(const char *titleID, OSDHistoryEntry *historyList)
{
	int leastUsedRecordIdx = 0;
	int leastUsedRecordTimestamp = INT_MAX;
	int leastUsedRecordLaunchCount = INT_MAX;
	u8 blankSlots[OSD_HISTORY_MAX_ENTRIES];
	int blankSlotCount = 0;
	int i;

	for (i = 0; i < OSD_HISTORY_MAX_ENTRIES; i++) {
		if (historyList[i].titleID[0] == '\0') {
			blankSlots[blankSlotCount++] = i;
			continue;
		}

		if (historyList[i].launchCount < leastUsedRecordLaunchCount) {
			leastUsedRecordIdx = i;
			leastUsedRecordLaunchCount = historyList[i].launchCount;
		}
		if (leastUsedRecordLaunchCount == historyList[i].launchCount && historyList[i].timestamp < leastUsedRecordTimestamp) {
			leastUsedRecordTimestamp = historyList[i].timestamp;
			leastUsedRecordIdx = i;
		}

		if (!strncmp(historyList[i].titleID, titleID, sizeof(historyList[i].titleID))) {
			historyList[i].timestamp = getHistoryTimestamp();
			if ((historyList[i].bitmask & 0x3F) != 0x3F) {
				int newLaunchCount = historyList[i].launchCount + 1;
				if (newLaunchCount >= 0x80)
					newLaunchCount = 0x7F;

				if (newLaunchCount >= 14 && ((newLaunchCount - 14) % 10) == 0) {
					int value;
					while ((historyList[i].bitmask >> (value = rand() % 6)) & 1) {
					}
					historyList[i].shiftAmount = value;
					historyList[i].bitmask |= 1 << value;
				}
				historyList[i].launchCount = newLaunchCount;
			} else {
				if (historyList[i].launchCount < 0x3F) {
					historyList[i].launchCount++;
				} else {
					historyList[i].launchCount = historyList[i].bitmask & 0x3F;
					historyList[i].shiftAmount = 7;
				}
			}
			return;
		}
	}

	{
		OSDHistoryEntry *newEntry;
		int slot;

		if (blankSlotCount > 0) {
			slot = blankSlots[rand() % blankSlotCount];
			newEntry = &historyList[slot];
		} else {
			OSDHistoryEntry evictedEntry;
			slot = leastUsedRecordIdx;
			newEntry = &historyList[slot];
			memcpy(&evictedEntry, newEntry, sizeof(evictedEntry));
			appendEvictedHistoryEntry(&evictedEntry);
		}

		memset(newEntry, 0, sizeof(*newEntry));
		strncpy(newEntry->titleID, titleID, sizeof(newEntry->titleID) - 1);
		newEntry->launchCount = 1;
		newEntry->bitmask = 1;
		newEntry->shiftAmount = 0;
		newEntry->timestamp = getHistoryTimestamp();
	}
}

int updateOSDHistoryFile(const char *titleID)
{
	OSDHistoryEntry historyList[OSD_HISTORY_MAX_ENTRIES];
	int fd;
	int count;
	int ret = 0;
	int port;

	if (titleID == NULL || strlen(titleID) < 11)
		return 0;

	osd_history_path[6] = getHistoryRegionLetter();

	for (port = 0; port < 2; port++) {
		int mcType;
		int format;
		int infoRet;

		mcGetInfo(port, 0, &mcType, NULL, &format);
		mcSync(0, NULL, &infoRet);
		if (infoRet < 0 || mcType != sceMcTypePS2 || format != MC_FORMATTED)
			continue;

		osd_history_path[2] = port + '0';
		fd = genOpen(osd_history_path, FIO_O_RDONLY);
		if (fd < 0) {
			if (ensureHistoryDirectory(port) < 0)
				continue;
			memset(historyList, 0, OSD_HISTORY_FILE_SIZE);
		} else {
			count = genRead(fd, historyList, OSD_HISTORY_FILE_SIZE);
			genClose(fd);
			if (count != OSD_HISTORY_FILE_SIZE)
				memset(historyList, 0, OSD_HISTORY_FILE_SIZE);
		}

		processHistoryList(titleID, historyList);

		fd = genOpen(osd_history_path, FIO_O_WRONLY | FIO_O_CREAT | FIO_O_TRUNC);
		if (fd < 0) {
			ret = fd;
			continue;
		}
		count = genWrite(fd, historyList, OSD_HISTORY_FILE_SIZE);
		genClose(fd);
		if (count != OSD_HISTORY_FILE_SIZE)
			ret = -EIO;
	}

	return ret;
}
