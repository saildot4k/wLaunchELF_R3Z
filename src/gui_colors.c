#include "launchelf.h"
#include "gui_colors.h"

static unsigned long hextoul(const char *string)
{
	unsigned long value;
	char c;

	value = 0;
	while (!(((c = *string++) < '0') || (c > 'F') || ((c > '9') && (c < 'A'))))
		value = value * 16 + ((c > '9') ? (c - 'A' + 10) : (c - '0'));
	return value;
}

// storeGuiColorsCNF saves cosmetic color settings into a CNF buffer and returns written length.
size_t storeGuiColorsCNF(char *cnf_buf)
{
	size_t CNF_size;

	sprintf(cnf_buf,
	        "GUI_Col_1_ABGR = %08X\r\n"
	        "GUI_Col_2_ABGR = %08X\r\n"
	        "GUI_Col_3_ABGR = %08X\r\n"
	        "GUI_Col_4_ABGR = %08X\r\n"
	        "GUI_Col_5_ABGR = %08X\r\n"
	        "GUI_Col_6_ABGR = %08X\r\n"
	        "GUI_Col_7_ABGR = %08X\r\n"
	        "GUI_Col_8_ABGR = %08X\r\n"
	        "TV_mode = %d\r\n"
	        "Screen_Offset_X = %d\r\n"
	        "Screen_Offset_Y = %d\r\n"
	        "Popup_Opaque = %d\r\n"
	        "Menu_Frame = %d\r\n"
	        "%n",                               // %n causes NO output, but only a measurement
	        (u32)setting->color[COLOR_BACKGR],  //Col_1
	        (u32)setting->color[COLOR_FRAME],   //Col_2
	        (u32)setting->color[COLOR_SELECT],  //Col_3
	        (u32)setting->color[COLOR_TEXT],    //Col_4
	        (u32)setting->color[COLOR_GRAPH1],  //Col_5
	        (u32)setting->color[COLOR_GRAPH2],  //Col_6
	        (u32)setting->color[COLOR_GRAPH3],  //Col_7
	        (u32)setting->color[COLOR_GRAPH4],  //Col_8
	        setting->TV_mode,                   //TV_mode
	        setting->screen_x,                  //Screen_X
	        setting->screen_y,                  //Screen_Y
	        setting->Popup_Opaque,              //Popup_Opaque
	        setting->Menu_Frame,                //Menu_Frame
	        &CNF_size                           // This variable measures the size of sprintf data
	        );
	return CNF_size;
}

// scanGuiColorsCNF reads one cosmetic/color key-value pair, returning 1 when consumed.
int scanGuiColorsCNF(char *name, char *value)
{
	if (!strcmp(name, "GUI_Col_1_ABGR"))
		setting->color[COLOR_BACKGR] = hextoul(value);
	else if (!strcmp(name, "GUI_Col_2_ABGR"))
		setting->color[COLOR_FRAME] = hextoul(value);
	else if (!strcmp(name, "GUI_Col_3_ABGR"))
		setting->color[COLOR_SELECT] = hextoul(value);
	else if (!strcmp(name, "GUI_Col_4_ABGR"))
		setting->color[COLOR_TEXT] = hextoul(value);
	else if (!strcmp(name, "GUI_Col_5_ABGR"))
		setting->color[COLOR_GRAPH1] = hextoul(value);
	else if (!strcmp(name, "GUI_Col_6_ABGR"))
		setting->color[COLOR_GRAPH2] = hextoul(value);
	else if (!strcmp(name, "GUI_Col_7_ABGR"))
		setting->color[COLOR_GRAPH3] = hextoul(value);
	else if (!strcmp(name, "GUI_Col_8_ABGR"))
		setting->color[COLOR_GRAPH4] = hextoul(value);
	else if (!strcmp(name, "TV_mode"))
		setting->TV_mode = atoi(value);
	else if (!strcmp(name, "Screen_Offset_X"))
		setting->screen_x = atoi(value);
	else if (!strcmp(name, "Screen_Offset_Y"))
		setting->screen_y = atoi(value);
	else if (!strcmp(name, "Popup_Opaque"))
		setting->Popup_Opaque = atoi(value);
	else if (!strcmp(name, "Menu_Frame"))
		setting->Menu_Frame = atoi(value);
	else
		return 0;

	return 1;
}
