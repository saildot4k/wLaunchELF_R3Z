#ifndef NEUTRINO_GAMEID_H
#define NEUTRINO_GAMEID_H

#include <stddef.h>

#define NEUTRINO_GAME_ID_SIZE 12

int prepareNeutrinoGameID(const char *iso_path, const char *elf_path, char *gameID, size_t gameID_len);
#ifdef MMCE
void setNeutrinoMmceGameID(const char *gameID, const char *iso_path, const char *elf_path);
#endif

#endif
