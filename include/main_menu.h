#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include <stddef.h>
#include "launchelf.h"

enum MAIN_MENU_MODE {
	MAIN_MENU_MODE_BUTTON = 0,
	MAIN_MENU_MODE_DPAD
};

typedef struct
{
	int selected;
	int timeout;
	int prev_timeout;
	int init_delay;
	int prev_init_delay;
	int mode;
	int user_acted;
	u64 init_delay_start;
	u64 timeout_start;
	int menu_lk[SETTING_LK_BTN_COUNT];
} MainMenuState;

void MainMenuState_Init(MainMenuState *state);
void MainMenuState_BeginTimers(MainMenuState *state);
void MainMenuState_UpdateTimers(MainMenuState *state, int *event);
int drawMainMenuScreen(MainMenuState *state, const char *main_msg);
void MainMenuHandlePad(MainMenuState *state, int nElfs, char *runPath, size_t runPath_len, int *event);
void MainMenuHandleAutoLaunch(MainMenuState *state, char *runPath, size_t runPath_len, int *event);
void MainMenuMarkActionTaken(MainMenuState *state);

#endif
