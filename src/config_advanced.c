//---------------------------------------------------------------------------
// File name:   config_advanced.c
//---------------------------------------------------------------------------
#include "config_private.h"

enum CONFIG_ADVANCED {
	CONFIG_ADVANCED_FIRST = 1,
	CONFIG_ADVANCED_APP_GAMEID = CONFIG_ADVANCED_FIRST,
	CONFIG_ADVANCED_CDROM_DISABLE_GAMEID,
	CONFIG_ADVANCED_PSU_HUGENAMES,
	CONFIG_ADVANCED_PSU_DATENAMES,
	CONFIG_ADVANCED_PSU_NOOVERWRITE,
	CONFIG_ADVANCED_PATHPAD_LOCK,
	CONFIG_ADVANCED_FB_NOICONS,
	CONFIG_ADVANCED_HOSTWRITE,

	CONFIG_ADVANCED_AFT_OPTIONS,
	CONFIG_ADVANCED_RETURN = CONFIG_ADVANCED_AFT_OPTIONS,

	CONFIG_ADVANCED_COUNT
};

static int getConfigAdvancedItemY(int s)
{
	int y = Menu_start_y + FONT_HEIGHT + FONT_HEIGHT / 2;
	int i;

	if (s <= CONFIG_ADVANCED_APP_GAMEID)
		return y;

	for (i = CONFIG_ADVANCED_APP_GAMEID; i < s; i++) {
		y += FONT_HEIGHT;
		if (i != CONFIG_ADVANCED_APP_GAMEID && i != CONFIG_ADVANCED_PSU_HUGENAMES && i != CONFIG_ADVANCED_PSU_DATENAMES)
			y += FONT_HEIGHT / 2;
	}

	return y;
}

void Config_Advanced(void)
{
	int s, max_s = CONFIG_ADVANCED_COUNT - 1;
	int x, y;
	int len;
	int event, post_event = 0;
	char c[MAX_PATH];
	int bool_label_width;
	const char *hostwrite_label;
	const char *app_gameid_label = LNG(RetroGem_Game_ID);
	const char *cdrom_gameid_label = LNG(Disable_RetroGem_Game_ID_for_Discs);
	const char *psu_huge_label = LNG(Create_PSU_with_ID_and_Game_Title);
	const char *psu_date_label = LNG(Create_PSU_with_Date);
	const char *psu_nooverwrite_label = LNG(Create_New_PSU_if_filename_exists);
	const char *pathpad_lock_label = LNG(Lock_Main_LaunchKey_PathPad_Paths);
	const char *fb_noicons_label = LNG(Disable_Icons_in_File_Browser);

	event = 1;
	s = CONFIG_ADVANCED_FIRST;
	while (1) {
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP) {
				event |= 2;
				if (s != CONFIG_ADVANCED_FIRST)
					s--;
				else
					s = max_s;
			} else if (new_pad & PAD_DOWN) {
				event |= 2;
				if (s != max_s)
					s++;
				else
					s = CONFIG_ADVANCED_FIRST;
			} else if (new_pad & PAD_LEFT) {
				event |= 2;
				if (s != max_s)
					s = max_s;
				else
					s = CONFIG_ADVANCED_FIRST;
				} else if (new_pad & PAD_RIGHT) {
					event |= 2;
					if (s != max_s)
						s = max_s;
					else
						s = CONFIG_ADVANCED_FIRST;
				} else if ((new_pad & PAD_CROSS) || (new_pad & PAD_CIRCLE)) {
					event |= 2;
					if (s == CONFIG_ADVANCED_APP_GAMEID)
						setting->app_gameid = !setting->app_gameid;
					else if (s == CONFIG_ADVANCED_CDROM_DISABLE_GAMEID)
						setting->cdrom_disable_gameid = !setting->cdrom_disable_gameid;
					else if (s == CONFIG_ADVANCED_PSU_HUGENAMES)
						setting->PSU_HugeNames = !setting->PSU_HugeNames;
					else if (s == CONFIG_ADVANCED_PSU_DATENAMES) {
						setting->PSU_DateNames = !setting->PSU_DateNames;
						if (!setting->PSU_DateNames)
							setting->PSU_NoOverwrite = 0;
					} else if (s == CONFIG_ADVANCED_PSU_NOOVERWRITE) {
						setting->PSU_NoOverwrite = !setting->PSU_NoOverwrite;
						if (setting->PSU_NoOverwrite)
							setting->PSU_DateNames = 1;
					} else if (s == CONFIG_ADVANCED_PATHPAD_LOCK)
						setting->PathPad_Lock = !setting->PathPad_Lock;
					else if (s == CONFIG_ADVANCED_FB_NOICONS)
						setting->FB_NoIcons = !setting->FB_NoIcons;
					else if (s == CONFIG_ADVANCED_HOSTWRITE)
						setting->HOSTwrite = !setting->HOSTwrite;
					else if (s == CONFIG_ADVANCED_RETURN)
						return;
				} else if (new_pad & PAD_TRIANGLE)
					return;
			}

		if (event || post_event) {
			clrScr(setting->color[COLOR_BACKGR]);
			x = Menu_start_x;
			y = Menu_start_y;

#ifdef UDPFS
			hostwrite_label = LNG(Enable_Network_write);
#else
			hostwrite_label = LNG(Enable_Host_write);
#endif
			bool_label_width = (int)strlen(app_gameid_label);
			if ((int)strlen(cdrom_gameid_label) > bool_label_width)
				bool_label_width = (int)strlen(cdrom_gameid_label);
			if ((int)strlen(psu_huge_label) > bool_label_width)
				bool_label_width = (int)strlen(psu_huge_label);
			if ((int)strlen(psu_date_label) > bool_label_width)
				bool_label_width = (int)strlen(psu_date_label);
			if ((int)strlen(psu_nooverwrite_label) > bool_label_width)
				bool_label_width = (int)strlen(psu_nooverwrite_label);
			if ((int)strlen(pathpad_lock_label) > bool_label_width)
				bool_label_width = (int)strlen(pathpad_lock_label);
			if ((int)strlen(fb_noicons_label) > bool_label_width)
				bool_label_width = (int)strlen(fb_noicons_label);
			if ((int)strlen(hostwrite_label) > bool_label_width)
				bool_label_width = (int)strlen(hostwrite_label);

			printXY(LNG(Advanced_Settings), x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			configFormatLabelValueAligned(c, sizeof(c), app_gameid_label, setting->app_gameid ? LNG(ON) : LNG(OFF), bool_label_width);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

				configFormatLabelValueAligned(c, sizeof(c), cdrom_gameid_label, setting->cdrom_disable_gameid ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				y += FONT_HEIGHT / 2;

				configFormatLabelValueAligned(c, sizeof(c), psu_huge_label, setting->PSU_HugeNames ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;

				configFormatLabelValueAligned(c, sizeof(c), psu_date_label, setting->PSU_DateNames ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;

				configFormatLabelValueAligned(c, sizeof(c), psu_nooverwrite_label, setting->PSU_NoOverwrite ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				y += FONT_HEIGHT / 2;

				configFormatLabelValueAligned(c, sizeof(c), pathpad_lock_label, setting->PathPad_Lock ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				y += FONT_HEIGHT / 2;

				configFormatLabelValueAligned(c, sizeof(c), fb_noicons_label, setting->FB_NoIcons ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				y += FONT_HEIGHT / 2;

				configFormatLabelValueAligned(c, sizeof(c), hostwrite_label, setting->HOSTwrite ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				y += FONT_HEIGHT / 2;

				sprintf(c, "  %s", LNG(RETURN));
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);

				y = getConfigAdvancedItemY(s);
				drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);

				if (s < CONFIG_ADVANCED_AFT_OPTIONS) {
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s",
					              LNG(Change));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s",
					              LNG(Change));
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
		}

		drawScr();
		post_event = event;
		event = 0;
	}
}
//---------------------------------------------------------------------------
// End of file: config_advanced.c
//---------------------------------------------------------------------------
