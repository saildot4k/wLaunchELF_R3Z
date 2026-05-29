#ifndef MAIN_GAMEID_H
#define MAIN_GAMEID_H

#include <stddef.h>

void displayRetroGemGameID(const char *gameID, int frames);
int buildLaunchGameID(const char *exec_path, char *gameID, size_t gameID_len);
int isLikelyDiscLaunch(const char *selected_path);

#endif
