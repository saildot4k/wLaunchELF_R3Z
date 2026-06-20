#ifndef MMCE_CMDS_H
#define MMCE_CMDS_H

#include <tamtypes.h>
#include <stddef.h>

#define MMCE_CARD_TYPE_REGULAR 0x00
#define MMCE_CARD_TYPE_BOOT 0x01

int mmceCmdPing(const char *devname);
int mmceCmdWaitReady(const char *devname);
int mmceCmdSetCardAndChannel(const char *devname, u8 card_type, u16 card_num, u16 channel_num);
int mmceCmdSetGameId(const char *devname, const char *card_id);
int mmceCmdGetGameId(const char *devname, char *game_id, size_t game_id_size);
int mmceCmdWaitGameIdStable(const char *devname, char *game_id, size_t game_id_size);
int mmceCmdSetChannel(const char *devname, u16 channel_num);

#endif
