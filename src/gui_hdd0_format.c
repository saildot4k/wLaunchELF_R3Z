#include "gui_hdd0_format.h"

static int getHddDeviceFromPath(const char *path, char *device)
{
	if (strncmp(path, "hdd", 3) || path[3] < '0' || path[3] > '9' || path[4] != ':' || path[5] != '/')
		return -1;

	if (device != NULL) {
		memcpy(device, path, 5);
		device[5] = '\0';
	}
	return 0;
}

int getHddParty(const char *path, const FILEINFO *file, char *party, char *dir)
{
	char fullpath[MAX_PATH], device[6], *p, *party_start;

	if (getHddDeviceFromPath(path, device) < 0)
		return -1;

	strcpy(fullpath, path);
	if (file != NULL) {
		strcat(fullpath, file->name);
		if (file->stats.AttrFile & sceMcFileAttrSubdir)
			strcat(fullpath, "/");
	}
	if ((p = strchr(&fullpath[6], '/')) == NULL)
		p = &fullpath[strlen(fullpath)];
	party_start = &fullpath[6];

	if (party != NULL) {
		/* Keep party form as "hddN:partition" (no slash after colon). */
		snprintf(party, MAX_NAME, "%s%.*s", device, (int)(p - party_start), party_start);
	}
	if (dir != NULL)
		snprintf(dir, MAX_PATH, "pfs0:%s", p);

	return 0;
}

int getHddDVRPParty(const char *path, const FILEINFO *file, char *party, char *dir)
{
	char fullpath[MAX_PATH], *p, *party_start;

	if (strncmp(path, "dvr_hdd", 7))
		return -1;

	strcpy(fullpath, path);
	if (file != NULL) {
		strcat(fullpath, file->name);
		if (file->stats.AttrFile & sceMcFileAttrSubdir)
			strcat(fullpath, "/");
	}
	if ((p = strchr(&fullpath[10], '/')) == NULL)
		p = &fullpath[strlen(fullpath)];
	party_start = &fullpath[10];

	if (party != NULL) {
		/* Keep party form as "dvr_hdd0:partition" (no slash after colon). */
		snprintf(party, MAX_NAME, "dvr_hdd0:%.*s", (int)(p - party_start), party_start);
	}
	if (dir != NULL) {
		char party_buf[MAX_NAME];
		int pfs_ix;

		snprintf(party_buf, sizeof(party_buf), "dvr_hdd0:%.*s", (int)(p - party_start), party_start);
		pfs_ix = getDVRPPartyMountIndex(party_buf);
		if (pfs_ix < 0)
			pfs_ix = 0;
		snprintf(dir, MAX_PATH, "dvr_pfs%d:%s", pfs_ix, p);
	}

	return 0;
}
