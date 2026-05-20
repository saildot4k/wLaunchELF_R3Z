#include "gui_hdd0_format.h"

int getHddParty(const char *path, const FILEINFO *file, char *party, char *dir)
{
	char fullpath[MAX_PATH], *p, *party_start;

	if (strncmp(path, "hdd", 3))
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
		/* Keep party form as "hdd0:partition" (no slash after colon). */
		snprintf(party, MAX_NAME, "hdd0:%.*s", (int)(p - party_start), party_start);
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
	if (dir != NULL)
		snprintf(dir, MAX_PATH, "dvr_pfs0:%s", p);

	return 0;
}
