#include "launchelf.h"
#include "mmce_cmds.h"

#ifndef MMCE_CMD_PING
#define MMCE_CMD_PING 0x01
#endif
#ifndef MMCE_CMD_GET_STATUS
#define MMCE_CMD_GET_STATUS 0x02
#endif
#ifndef MMCE_CMD_GET_CARD
#define MMCE_CMD_GET_CARD 0x03
#endif
#ifndef MMCE_CMD_SET_CARD
#define MMCE_CMD_SET_CARD 0x04
#endif
#ifndef MMCE_CMD_GET_CHANNEL
#define MMCE_CMD_GET_CHANNEL 0x05
#endif
#ifndef MMCE_CMD_SET_CHANNEL
#define MMCE_CMD_SET_CHANNEL 0x06
#endif
#ifndef MMCE_CMD_GET_GAMEID
#define MMCE_CMD_GET_GAMEID 0x07
#endif
#ifndef MMCE_CMD_SET_GAMEID
#define MMCE_CMD_SET_GAMEID 0x08
#endif

#define MMCE_SET_MODE_NUM 0x00
#define MMCE_STATUS_BUSY 0x0001
#define MMCE_READY_POLL_DELAY_US (200 * 1000)
#define MMCE_READY_POLL_MAX 15

int mmceCmdPing(const char *devname)
{
	return fileXioDevctl(devname, MMCE_CMD_PING, NULL, 0, NULL, 0);
}

int mmceCmdWaitReady(const char *devname)
{
	int i, status;
	int ready_count = 0;

	for (i = 0; i < MMCE_READY_POLL_MAX; i++) {
		status = fileXioDevctl(devname, MMCE_CMD_GET_STATUS, NULL, 0, NULL, 0);
		if (status < 0)
			return status;
		if ((status & MMCE_STATUS_BUSY) == 0) {
			/*
			 * Require consecutive READY observations to avoid false-ready races
			 * where firmware reports ready briefly while still applying changes.
			 */
			ready_count++;
			if (ready_count >= 2)
				return 0;
		} else {
			ready_count = 0;
		}
		DelayThread(MMCE_READY_POLL_DELAY_US);
	}

	return -1;
}

int mmceCmdSetCardByNumber(const char *devname, u8 card_type, u16 card_num)
{
	u32 arg = 0;

	arg = ((u32)card_type << 24) | (MMCE_SET_MODE_NUM << 16) | card_num;
	return fileXioDevctl(devname, MMCE_CMD_SET_CARD, &arg, sizeof(arg), NULL, 0);
}

int mmceCmdSetGameId(const char *devname, const char *card_id)
{
	return fileXioDevctl(devname, MMCE_CMD_SET_GAMEID, (void *)card_id, strlen(card_id) + 1, NULL, 0);
}

int mmceCmdGetCard(const char *devname, u16 *card_num)
{
	int ret;

	ret = fileXioDevctl(devname, MMCE_CMD_GET_CARD, NULL, 0, NULL, 0);
	if (ret < 0)
		return ret;
	if (ret > 0xFFFF)
		return -1;

	*card_num = (u16)ret;
	return 0;
}

int mmceCmdGetGameId(const char *devname, char *game_id, size_t game_id_size)
{
	int ret;

	if ((game_id == NULL) || (game_id_size == 0))
		return -1;

	game_id[0] = '\0';
	ret = fileXioDevctl(devname, MMCE_CMD_GET_GAMEID, NULL, 0, game_id, game_id_size);
	if (ret < 0)
		return ret;
	game_id[game_id_size - 1] = '\0';
	return 0;
}

int mmceCmdWaitCardStable(const char *devname, u16 *card_num)
{
	int i, status, ret;
	u16 prev_card = 0xFFFF, cur_card = 0xFFFF;
	int have_prev = 0;

	for (i = 0; i < MMCE_READY_POLL_MAX; i++) {
		status = fileXioDevctl(devname, MMCE_CMD_GET_STATUS, NULL, 0, NULL, 0);
		if (status < 0)
			return status;
		if (status & MMCE_STATUS_BUSY) {
			have_prev = 0;
			DelayThread(MMCE_READY_POLL_DELAY_US);
			continue;
		}

		ret = mmceCmdGetCard(devname, &cur_card);
		if (ret < 0)
			return ret;

		if (have_prev && (cur_card == prev_card)) {
			if (card_num != NULL)
				*card_num = cur_card;
			return 0;
		}

		prev_card = cur_card;
		have_prev = 1;
		DelayThread(MMCE_READY_POLL_DELAY_US);
	}

	return -1;
}

int mmceCmdWaitGameIdStable(const char *devname, char *game_id, size_t game_id_size)
{
	int i, status, ret;
	char prev_game_id[256];
	char cur_game_id[256];
	int have_prev = 0;

	if ((game_id == NULL) || (game_id_size == 0))
		return -1;

	prev_game_id[0] = '\0';
	cur_game_id[0] = '\0';

	for (i = 0; i < MMCE_READY_POLL_MAX; i++) {
		status = fileXioDevctl(devname, MMCE_CMD_GET_STATUS, NULL, 0, NULL, 0);
		if (status < 0)
			return status;
		if (status & MMCE_STATUS_BUSY) {
			have_prev = 0;
			DelayThread(MMCE_READY_POLL_DELAY_US);
			continue;
		}

		ret = mmceCmdGetGameId(devname, cur_game_id, sizeof(cur_game_id));
		if (ret < 0)
			return ret;

		if (have_prev && (strcmp(cur_game_id, prev_game_id) == 0)) {
			snprintf(game_id, game_id_size, "%s", cur_game_id);
			return 0;
		}

		snprintf(prev_game_id, sizeof(prev_game_id), "%s", cur_game_id);
		have_prev = 1;
		DelayThread(MMCE_READY_POLL_DELAY_US);
	}

	return -1;
}

int mmceCmdSetChannel(const char *devname, u16 channel_num)
{
	u32 arg = 0;

	arg = (MMCE_SET_MODE_NUM << 16) | channel_num;
	return fileXioDevctl(devname, MMCE_CMD_SET_CHANNEL, &arg, sizeof(arg), NULL, 0);
}

int mmceCmdGetChannel(const char *devname, u16 *channel_num)
{
	int ret;

	ret = fileXioDevctl(devname, MMCE_CMD_GET_CHANNEL, NULL, 0, NULL, 0);
	if (ret < 0)
		return ret;
	if (ret > 0xFFFF)
		return -1;

	*channel_num = (u16)ret;
	return 0;
}

int mmceCmdWaitChannelStable(const char *devname, u16 *channel_num)
{
	int i, status, ret;
	u16 prev_channel = 0xFFFF, cur_channel = 0xFFFF;
	int have_prev = 0;

	for (i = 0; i < MMCE_READY_POLL_MAX; i++) {
		status = fileXioDevctl(devname, MMCE_CMD_GET_STATUS, NULL, 0, NULL, 0);
		if (status < 0)
			return status;
		if (status & MMCE_STATUS_BUSY) {
			have_prev = 0;
			DelayThread(MMCE_READY_POLL_DELAY_US);
			continue;
		}

		ret = mmceCmdGetChannel(devname, &cur_channel);
		if (ret < 0)
			return ret;

		if (have_prev && (cur_channel == prev_channel)) {
			if (channel_num != NULL)
				*channel_num = cur_channel;
			return 0;
		}

		prev_channel = cur_channel;
		have_prev = 1;
		DelayThread(MMCE_READY_POLL_DELAY_US);
	}

	return -1;
}
