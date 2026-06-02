#ifndef CONFIG_PRIVATE_H
#define CONFIG_PRIVATE_H

#include "launchelf.h"
#include <stddef.h>

enum {
	DEF_TIMEOUT = 10,
	DEF_HIDE_PATHS = TRUE,
#ifdef CUSTOM_COLORS
	DEF_COLOR1 = GS_SETREG_RGBA(0, 0, 0, 0),  //Backgr
	DEF_COLOR2 = GS_SETREG_RGBA(160, 160, 160, 0),  //Frame
#ifdef DVRP
	DEF_COLOR3 = GS_SETREG_RGBA(0x7a, 0, 0xbe, 0),
#else
	DEF_COLOR3 = GS_SETREG_RGBA(0, 204, 255, 0),  //Select
#endif
	DEF_COLOR4 = GS_SETREG_RGBA(255, 255, 255, 0),  //Text
	DEF_COLOR5 = GS_SETREG_RGBA(255, 255, 0, 0),      //Graph1
	DEF_COLOR6 = GS_SETREG_RGBA(0, 255, 0, 0),        //Graph2
	DEF_COLOR7 = GS_SETREG_RGBA(64, 64, 64, 0),       //Graph3
	DEF_COLOR8 = GS_SETREG_RGBA(128, 128, 128, 0),    //Graph4
#else
	DEF_COLOR1 = GS_SETREG_RGBA(128, 128, 128, 0),  //Backgr
	DEF_COLOR2 = GS_SETREG_RGBA(64, 64, 64, 0),     //Frame
	DEF_COLOR3 = GS_SETREG_RGBA(96, 0, 0, 0),       //Select
	DEF_COLOR4 = GS_SETREG_RGBA(0, 0, 0, 0),        //Text
	DEF_COLOR5 = GS_SETREG_RGBA(96, 96, 0, 0),      //Graph1
	DEF_COLOR6 = GS_SETREG_RGBA(0, 96, 0, 0),       //Graph2
	DEF_COLOR7 = GS_SETREG_RGBA(224, 224, 224, 0),  //Graph3
	DEF_COLOR8 = GS_SETREG_RGBA(0, 0, 0, 0),        //Graph4
#endif //CUSTOM_COLORS
	DEF_MENU_FRAME = TRUE,
	DEF_SWAPKEYS = FALSE,
	DEF_HOSTWRITE = FALSE,
	DEF_APP_GAMEID = FALSE,
	DEF_CDROM_DISABLE_GAMEID = FALSE,
	DEF_POPUP_OPAQUE = FALSE,
	DEF_INIT_DELAY = 0,
	DEF_USBKBD_USED = 1,
	DEF_LANGUAGE = BUILTIN_LANGUAGE_ENGLISH,
	DEF_STARTUP_RESET_IOP_ELFOAD = 0,
	DEF_SHOW_TITLES = 1,
	DEF_PATHPAD_LOCK = 0,
	DEF_PSU_HUGENAMES = 0,
	DEF_PSU_DATENAMES = 0,
	DEF_PSU_NOOVERWRITE = 0,
	DEF_FB_NOICONS = 0,
};

void configFormatLabelValue(char *dst, size_t dst_size, const char *label, const char *value);
void configFormatLabelValueAligned(char *dst, size_t dst_size, const char *label, const char *value, int label_width);

void Config_Screen(void);
void Config_Startup(void);
void Config_Network(void);
void Config_Advanced(void);

#endif
