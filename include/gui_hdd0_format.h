#ifndef GUI_HDD0_FORMAT_H
#define GUI_HDD0_FORMAT_H

#include "launchelf.h"

int getHddParty(const char *path, const FILEINFO *file, char *party, char *dir);
int getHddDVRPParty(const char *path, const FILEINFO *file, char *party, char *dir);

#endif
