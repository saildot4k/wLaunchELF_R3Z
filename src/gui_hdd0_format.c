#include "gui_hdd0_format.h"

int getHddParty(const char *path, const FILEINFO *file, char *party, char *dir)
{
	char fullpath[MAX_PATH], *p;

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

	if (party != NULL) {
		sprintf(party, "hdd0:%s", &fullpath[5]);
		party[p - fullpath + 1] = '\0';
	}
	if (dir != NULL)
		sprintf(dir, "pfs0:%s", p);

	return 0;
}

int getHddDVRPParty(const char *path, const FILEINFO *file, char *party, char *dir)
{
	char fullpath[MAX_PATH], *p;

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

	if (party != NULL) {
		sprintf(party, "dvr_hdd0:%s", &fullpath[9]);
		party[p - fullpath + 1] = '\0';
	}
	if (dir != NULL)
		sprintf(dir, "dvr_pfs0:%s", p);

	return 0;
}
