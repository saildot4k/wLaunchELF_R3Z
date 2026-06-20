#include "launchelf.h"
#include "mmce_cmds.h"

#ifndef MMCE_CMD_PING
#define MMCE_CMD_PING 0x01
#endif
#ifndef MMCE_CMD_GET_STATUS
#define MMCE_CMD_GET_STATUS 0x02
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
#ifndef MMCE_CMD_SET_CARD_CHANNEL
#define MMCE_CMD_SET_CARD_CHANNEL 0x0A
#endif

#define MMCE_SET_MODE_NUM 0x00
#define MMCE_STATUS_BUSY 0x0001
#define MMCE_READY_POLL_DELAY_US (200 * 1000)
#define MMCE_STATUS_ERROR_BUDGET 8
#define MMCE_READY_POLL_MAX 40

static int mmceCmdGetStatusInternal(const char *devname)
{
	return fileXioDevctl(devname, MMCE_CMD_GET_STATUS, NULL, 0, NULL, 0);
}

int mmceCmdPing(const char *devname)
{
	int ret;

	ret = fileXioDevctl(devname, MMCE_CMD_PING, NULL, 0, NULL, 0);
	DPRINTF("MMCE[%s] PING ret=%d\n", devname, ret);
	return ret;
}

int mmceCmdWaitReady(const char *devname)
{
	int i, status;
	int ready_count = 0;
	int status_error_count = 0;

	for (i = 0; i < MMCE_READY_POLL_MAX; i++) {
		status = mmceCmdGetStatusInternal(devname);
		if (status < 0) {
			status_error_count++;
			DPRINTF("MMCE[%s] GET_STATUS poll=%d ret=%d transient_err=%d/%d\n",
			        devname, i + 1, status, status_error_count, MMCE_STATUS_ERROR_BUDGET);
			if (status_error_count >= MMCE_STATUS_ERROR_BUDGET)
				return status;
			ready_count = 0;
			DelayThread(MMCE_READY_POLL_DELAY_US);
			continue;
		}
		status_error_count = 0;
		DPRINTF("MMCE[%s] GET_STATUS poll=%d status=0x%04x busy=%d\n",
		        devname, i + 1, status & 0xFFFF, (status & MMCE_STATUS_BUSY) ? 1 : 0);
		if ((status & MMCE_STATUS_BUSY) == 0) {
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

int mmceCmdSetCardAndChannel(const char *devname, u8 card_type, u16 card_num, u16 channel_num)
{
	int ret;
	u8 arg[5];

	arg[0] = card_type;
	arg[1] = (u8)(card_num >> 8);
	arg[2] = (u8)(card_num & 0xFF);
	arg[3] = (u8)(channel_num >> 8);
	arg[4] = (u8)(channel_num & 0xFF);
	ret = fileXioDevctl(devname, MMCE_CMD_SET_CARD_CHANNEL, arg, sizeof(arg), NULL, 0);
	DPRINTF("MMCE[%s] SET_CARD_CHANNEL type=%u card=%u channel=%u ret=%d\n",
	        devname, (unsigned int)card_type, (unsigned int)card_num,
	        (unsigned int)channel_num, ret);
	return ret;
}

int mmceCmdSetGameId(const char *devname, const char *card_id)
{
	int ret;
	unsigned int len;

	len = (unsigned int)strlen(card_id) + 1;
	ret = fileXioDevctl(devname, MMCE_CMD_SET_GAMEID, (void *)card_id, len, NULL, 0);
	DPRINTF("MMCE[%s] SET_GAMEID req='%s' len=%u ret=%d\n", devname, card_id, len, ret);
	return ret;
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
	DPRINTF("MMCE[%s] GET_GAMEID id='%s'\n", devname, game_id);
	return 0;
}

int mmceCmdWaitGameIdStable(const char *devname, char *game_id, size_t game_id_size)
{
	int i, status, ret;
	char prev_game_id[256];
	char cur_game_id[256];
	int have_prev = 0;
	int status_error_count = 0;

	if ((game_id == NULL) || (game_id_size == 0))
		return -1;

	prev_game_id[0] = '\0';
	cur_game_id[0] = '\0';

	for (i = 0; i < MMCE_READY_POLL_MAX; i++) {
		status = mmceCmdGetStatusInternal(devname);
		if (status < 0) {
			status_error_count++;
			DPRINTF("MMCE[%s] wait-gameid poll=%d status_ret=%d transient_err=%d/%d\n",
			        devname, i + 1, status, status_error_count, MMCE_STATUS_ERROR_BUDGET);
			if (status_error_count >= MMCE_STATUS_ERROR_BUDGET)
				return status;
			have_prev = 0;
			DelayThread(MMCE_READY_POLL_DELAY_US);
			continue;
		}
		status_error_count = 0;
		if (status & MMCE_STATUS_BUSY) {
			have_prev = 0;
			DPRINTF("MMCE[%s] wait-gameid poll=%d busy=1\n", devname, i + 1);
			DelayThread(MMCE_READY_POLL_DELAY_US);
			continue;
		}
		DPRINTF("MMCE[%s] wait-gameid poll=%d busy=0\n", devname, i + 1);

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
	int ret;
	u32 arg = 0;

	arg = (MMCE_SET_MODE_NUM << 16) | channel_num;
	ret = fileXioDevctl(devname, MMCE_CMD_SET_CHANNEL, &arg, sizeof(arg), NULL, 0);
	DPRINTF("MMCE[%s] SET_CHANNEL mode=%u channel=%u ret=%d\n",
	        devname, (unsigned int)MMCE_SET_MODE_NUM, (unsigned int)channel_num, ret);
	return ret;
}
