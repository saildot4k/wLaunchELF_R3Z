//---------------------------------------------------------------------------
// File name:   config_startup.c
//---------------------------------------------------------------------------
#include "config_private.h"

enum CONFIG_STARTUP {
	CONFIG_STARTUP_FIRST = 1,
	CONFIG_STARTUP_LANGUAGE = CONFIG_STARTUP_FIRST,
	CONFIG_STARTUP_SELECT_BTN,
	CONFIG_STARTUP_INIT_DELAY,
	CONFIG_STARTUP_TIMEOUT,
	CONFIG_STARTUP_RESET_IOP_ELFLOAD,
	CONFIG_STARTUP_VKEY_LAYOUT,
	CONFIG_STARTUP_KEYBOARD,
	CONFIG_STARTUP_USBKBD,
	CONFIG_STARTUP_KBDMAP,
	CONFIG_STARTUP_CNF,
	CONFIG_STARTUP_LANG_FILE,
	CONFIG_STARTUP_FONT,
	CONFIG_STARTUP_ESR,
	CONFIG_STARTUP_OSDSYS,
	CONFIG_STARTUP_POPSTARTER,
	CONFIG_STARTUP_HIDE_HDD,

	CONFIG_STARTUP_RETURN,

	CONFIG_STARTUP_COUNT
};

static int normalizeHideHddMode(int mode)
{
	while (mode < 0)
		mode += HIDE_HDD_COUNT;
	while (mode >= HIDE_HDD_COUNT)
		mode -= HIDE_HDD_COUNT;
	return mode;
}

static const char *hide_hdd_mode_names[HIDE_HDD_COUNT] = {
	"Show All",
	"hdd1",
	"hdd0/1",
	"ata1",
	"ata0/1",
	"hdd1 & ata1",
	"hdd0/1 & ata0/1",
};

static const char *getHideHddModeDisplayName(int mode)
{
	return hide_hdd_mode_names[normalizeHideHddMode(mode)];
}

static int getConfigStartupItemY(int s)
{
	int y = Menu_start_y + FONT_HEIGHT + FONT_HEIGHT / 2;
	int i;

	if (s <= CONFIG_STARTUP_FIRST)
		return y;

	for (i = CONFIG_STARTUP_FIRST; i < s; i++) {
		y += FONT_HEIGHT;
		if (i == CONFIG_STARTUP_RESET_IOP_ELFLOAD || i == CONFIG_STARTUP_KBDMAP || i == CONFIG_STARTUP_POPSTARTER || i == CONFIG_STARTUP_HIDE_HDD)
			y += FONT_HEIGHT / 2;
	}

	return y;
}

void Config_Startup(void)
{
	int s, max_s = CONFIG_STARTUP_COUNT - 1;  //define cursor index and its max value
	int x, y;
	int len;
	int event, post_event = 0;
	char c[MAX_PATH];

	event = 1;  //event = initial entry
	s = CONFIG_STARTUP_FIRST;
	while (1) {
		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP) {
				event |= 2;  //event |= valid pad command
				if (s != CONFIG_STARTUP_FIRST)
					s--;
				else
					s = max_s;
			} else if (new_pad & PAD_DOWN) {
				event |= 2;  //event |= valid pad command
				if (s != max_s)
					s++;
				else
					s = CONFIG_STARTUP_FIRST;
			} else if (new_pad & PAD_LEFT) {
				event |= 2;  //event |= valid pad command
				if (s != max_s)
					s = max_s;
				else
					s = CONFIG_STARTUP_FIRST;
				} else if (new_pad & PAD_RIGHT) {
					event |= 2;  //event |= valid pad command
					if (s != max_s)
						s = max_s;
					else
						s = CONFIG_STARTUP_FIRST;
				} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
					event |= 2;  //event |= valid pad command
					if (s == CONFIG_STARTUP_LANGUAGE) {
						setting->lang_file[0] = '\0';
						Set_Language(setting->language - 1);
					} else if (s == CONFIG_STARTUP_INIT_DELAY && setting->Init_Delay > 0)
						setting->Init_Delay--;
					else if (s == CONFIG_STARTUP_TIMEOUT && setting->timeout > 0)
						setting->timeout--;
					else if (s == CONFIG_STARTUP_VKEY_LAYOUT)
						setting->virtual_keyboard_layout = normalizeVirtualKeyboardLayout(setting->virtual_keyboard_layout - 1);
					else if (s == CONFIG_STARTUP_USBKBD)
						setting->usbkbd_file[0] = '\0';
					else if (s == CONFIG_STARTUP_KBDMAP)
						setting->kbdmap_file[0] = '\0';
					else if (s == CONFIG_STARTUP_CNF)
						setting->CNF_Path[0] = '\0';
					else if (s == CONFIG_STARTUP_LANG_FILE) {
						setting->lang_file[0] = '\0';
						Load_External_Language();
					} else if (s == CONFIG_STARTUP_FONT) {
						setting->font_file[0] = '\0';
						loadFont("");
					} else if (s == CONFIG_STARTUP_ESR) {  //clear ESR file choice
						setting->LK_Path[SETTING_LK_ESR][0] = 0;
						setting->LK_Flag[SETTING_LK_ESR] = 0;
					} else if (s == CONFIG_STARTUP_OSDSYS) {  //clear OSDSYS file choice
						setting->LK_Path[SETTING_LK_OSDSYS][0] = 0;
						setting->LK_Flag[SETTING_LK_OSDSYS] = 0;
					} else if (s == CONFIG_STARTUP_POPSTARTER)
						setting->popstarter_file[0] = '\0';
					else if (s == CONFIG_STARTUP_HIDE_HDD)
						setting->Hide_Hdd = normalizeHideHddMode(setting->Hide_Hdd - 1);
				} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
					event |= 2;  //event |= valid pad command
					if (s == CONFIG_STARTUP_LANGUAGE) {
						setting->lang_file[0] = '\0';
						Set_Language(setting->language + 1);
					} else if (s == CONFIG_STARTUP_SELECT_BTN)
						setting->swapKeys = !setting->swapKeys;
					else if (s == CONFIG_STARTUP_INIT_DELAY)
						setting->Init_Delay++;
					else if (s == CONFIG_STARTUP_TIMEOUT)
						setting->timeout++;
					else if (s == CONFIG_STARTUP_KEYBOARD)
						setting->usbkbd_used = !setting->usbkbd_used;
					else if (s == CONFIG_STARTUP_RESET_IOP_ELFLOAD)
						setting->reboot_iop_elf_load = !setting->reboot_iop_elf_load;
					else if (s == CONFIG_STARTUP_VKEY_LAYOUT)
						setting->virtual_keyboard_layout = normalizeVirtualKeyboardLayout(setting->virtual_keyboard_layout + 1);
					else if (s == CONFIG_STARTUP_USBKBD)
						getFilePath(setting->usbkbd_file, USBKBD_IRX_CNF);
					else if (s == CONFIG_STARTUP_KBDMAP)
						getFilePath(setting->kbdmap_file, KBDMAP_FILE_CNF);
					else if (s == CONFIG_STARTUP_CNF) {
						char *tmp;
						getFilePath(setting->CNF_Path, CNF_PATH_CNF);
						if ((tmp = strrchr(setting->CNF_Path, '/')))
							tmp[1] = '\0';
					} else if (s == CONFIG_STARTUP_LANG_FILE) {
						getFilePath(setting->lang_file, LANG_CNF);
						Load_External_Language();
					} else if (s == CONFIG_STARTUP_FONT) {
						getFilePath(setting->font_file, FONT_CNF);
						if (loadFont(setting->font_file) == 0)
							setting->font_file[0] = '\0';
					} else if (s == CONFIG_STARTUP_ESR) {  //Make ESR file choice
						getFilePath(setting->LK_Path[SETTING_LK_ESR], LK_ELF_CNF);
						if (!strncmp(setting->LK_Path[SETTING_LK_ESR], "mc0", 3) ||
						    !strncmp(setting->LK_Path[SETTING_LK_ESR], "mc1", 3)) {
							snprintf(c, sizeof(c), "mc%.*s", (int)sizeof(c) - 3, &setting->LK_Path[SETTING_LK_ESR][3]);
							strcpy(setting->LK_Path[SETTING_LK_ESR], c);
						}
						if (setting->LK_Path[SETTING_LK_ESR][0])
							setting->LK_Flag[SETTING_LK_ESR] = 1;
					} else if (s == CONFIG_STARTUP_OSDSYS) {  //Make OSDSYS file choice
						getFilePath(setting->LK_Path[SETTING_LK_OSDSYS], TEXT_CNF);
						if (!strncmp(setting->LK_Path[SETTING_LK_OSDSYS], "mc0", 3) ||
						    !strncmp(setting->LK_Path[SETTING_LK_OSDSYS], "mc1", 3)) {
							snprintf(c, sizeof(c), "mc%.*s", (int)sizeof(c) - 3, &setting->LK_Path[SETTING_LK_OSDSYS][3]);
							strcpy(setting->LK_Path[SETTING_LK_OSDSYS], c);
						}
						if (setting->LK_Path[SETTING_LK_OSDSYS][0])
							setting->LK_Flag[SETTING_LK_OSDSYS] = 1;
					} else if (s == CONFIG_STARTUP_POPSTARTER) {
						getFilePath(setting->popstarter_file, ELF_FILE_CNF);
						if (!strncmp(setting->popstarter_file, "mc0", 3) ||
						    !strncmp(setting->popstarter_file, "mc1", 3)) {
							snprintf(c, sizeof(c), "mc%.*s", (int)sizeof(c) - 3, &setting->popstarter_file[3]);
							strcpy(setting->popstarter_file, c);
						}
					} else if (s == CONFIG_STARTUP_HIDE_HDD)
						setting->Hide_Hdd = normalizeHideHddMode(setting->Hide_Hdd + 1);
					else if (s == CONFIG_STARTUP_RETURN)
						return;
				} else if (new_pad & PAD_TRIANGLE)
					return;
			}

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[COLOR_BACKGR]);

			x = Menu_start_x;
			y = Menu_start_y;

			printXY(LNG(STARTUP_SETTINGS), x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			configFormatLabelValue(c, sizeof(c), LNG(Language), getBuiltinLanguageNativeName(setting->language));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			if (setting->swapKeys)
				sprintf(c, "  %s: \xFF"
				           "1:%s \xFF"
				           "0:%s",
				        LNG(Pad_mapping), LNG(OK), LNG(CANCEL));
			else
				sprintf(c, "  %s: \xFF"
				           "0:%s \xFF"
				           "1:%s",
				        LNG(Pad_mapping), LNG(OK), LNG(CANCEL));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			sprintf(c, "  %s: %d", LNG(Initial_Delay), setting->Init_Delay);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			sprintf(c, "  %s: %d", LNG(Default_Timeout), setting->timeout);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			sprintf(c, "  %s: %s", LNG(Reboot_IOP_loading_ELF), (setting->reboot_iop_elf_load) ? LNG(ON): LNG(OFF));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			configFormatLabelValue(c, sizeof(c), LNG(Virtual_Keyboard_Layout), getVirtualKeyboardLayoutDisplayName(setting->virtual_keyboard_layout));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			sprintf(c, "  %s: %s", LNG(USB_Keyboard_Used), (setting->usbkbd_used) ? LNG(ON): LNG(OFF));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			configFormatLabelValue(c, sizeof(c), LNG(USB_Keyboard_IRX), (strlen(setting->usbkbd_file) == 0) ? LNG(DEFAULT) : setting->usbkbd_file);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			configFormatLabelValue(c, sizeof(c), LNG(USB_Keyboard_Map), (strlen(setting->kbdmap_file) == 0) ? LNG(DEFAULT) : setting->kbdmap_file);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			configFormatLabelValue(c, sizeof(c), LNG(CNF_Path_override), (strlen(setting->CNF_Path) == 0) ? LNG(NONE) : setting->CNF_Path);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			configFormatLabelValue(c, sizeof(c), LNG(Language_File), (strlen(setting->lang_file) == 0) ? LNG(DEFAULT) : setting->lang_file);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			configFormatLabelValue(c, sizeof(c), LNG(Font_File), (strlen(setting->font_file) == 0) ? LNG(DEFAULT) : setting->font_file);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			configFormatLabelValue(c, sizeof(c), "ESR elf", (strlen(setting->LK_Path[SETTING_LK_ESR]) == 0) ? LNG(DEFAULT) : setting->LK_Path[SETTING_LK_ESR]);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			configFormatLabelValue(c, sizeof(c), "OSDSYS kelf", (strlen(setting->LK_Path[SETTING_LK_OSDSYS]) == 0) ? LNG(DEFAULT) : setting->LK_Path[SETTING_LK_OSDSYS]);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			configFormatLabelValue(c, sizeof(c), "POPSTARTER ELF", (strlen(setting->popstarter_file) == 0) ? LNG(DEFAULT) : setting->popstarter_file);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			configFormatLabelValue(c, sizeof(c), "Hide HDD", getHideHddModeDisplayName(setting->Hide_Hdd));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;
			sprintf(c, "  %s", LNG(RETURN));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			//Cursor positioning section
			y = getConfigStartupItemY(s);
			drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);


			//Tooltip section
			if ((s == CONFIG_STARTUP_SELECT_BTN) || (s == CONFIG_STARTUP_KEYBOARD) || (s == CONFIG_STARTUP_RESET_IOP_ELFLOAD)) {  //usbkbd_used
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s",
					              LNG(Change));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s",
					              LNG(Change));
			} else if ((s == CONFIG_STARTUP_LANGUAGE) || (s == CONFIG_STARTUP_INIT_DELAY) || (s == CONFIG_STARTUP_TIMEOUT) || (s == CONFIG_STARTUP_VKEY_LAYOUT) || (s == CONFIG_STARTUP_HIDE_HDD)) {  //language || Init_Delay || timeout || vkey layout || Hide HDD
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s \xFF"
					                 "0:%s",
					              LNG(Add), LNG(Subtract));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s \xFF"
					                 "1:%s",
					              LNG(Add), LNG(Subtract));
			} else if ((s == CONFIG_STARTUP_USBKBD) || (s == CONFIG_STARTUP_KBDMAP) || (s == CONFIG_STARTUP_CNF)
			           //usbkbd_file||kbdmap_file||CNF_Path
			           //Language||Fontfile||ESR_elf||OSDSYS_kelf||POPSTARTER_ELF
			           || (s == CONFIG_STARTUP_LANG_FILE) || (s == CONFIG_STARTUP_FONT) || (s == CONFIG_STARTUP_ESR) || (s == CONFIG_STARTUP_OSDSYS) || (s == CONFIG_STARTUP_POPSTARTER)) {
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s \xFF"
					                 "0:%s",
					              LNG(Browse), LNG(Clear));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s \xFF"
					                 "1:%s",
					              LNG(Browse), LNG(Clear));
			} else {
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s",
					              LNG(OK));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s",
					              LNG(OK));
			}
			sprintf(&c[len], " \xFF"
			                 "3:%s",
			        LNG(Return));
			setScrTmp("", c);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;

	}  //ends while
}  //ends Config_Startup
//---------------------------------------------------------------------------
// End of file: config_startup.c
//---------------------------------------------------------------------------
