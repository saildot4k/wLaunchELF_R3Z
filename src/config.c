//---------------------------------------------------------------------------
// File name:   config.c
//---------------------------------------------------------------------------
#include "launchelf.h"
#include "gui_colors.h"
#include <stdbool.h>
#include <stdlib.h>

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
	DEF_STARTUP_RESET_IOP_ELFOAD = 0,
	DEF_SHOW_TITLES = 1,
	DEF_PATHPAD_LOCK = 0,
	DEF_PSU_HUGENAMES = 0,
	DEF_PSU_DATENAMES = 0,
	DEF_PSU_NOOVERWRITE = 0,
	DEF_FB_NOICONS = 0,
};

static const char LK_ID[SETTING_LK_COUNT][10] = {
	"auto",
	"Circle",
	"Cross",
	"Square",
	"Triangle",
	"L1",
	"R1",
	"L2",
	"R2",
	"L3",
	"R3",
	"Start",
	"Select",  //Predefined for "CONFIG"
	"Left",
	"Right",
	"ESR",
	"OSDSYS"};

char PathPad[MAX_PATH_PAD][MAX_PATH];
SETTING *setting = NULL;
static SETTING *tmpsetting;

static void formatLabelValue(char *dst, size_t dst_size, const char *label, const char *value)
{
	int prefix_len;
	int value_len;

	if (dst_size == 0)
		return;
	prefix_len = snprintf(dst, dst_size, "  %s: ", label ? label : "");
	if (prefix_len < 0 || prefix_len >= (int)dst_size) {
		dst[dst_size - 1] = '\0';
		return;
	}

	value_len = (int)dst_size - prefix_len - 1;
	if (value_len < 0)
		value_len = 0;
	snprintf(dst + prefix_len, dst_size - prefix_len, "%.*s", value_len, value ? value : "");
}

static void formatLabelValueAligned(char *dst, size_t dst_size, const char *label, const char *value, int label_width)
{
	int prefix_len;
	int value_len;
	int pad_spaces;
	int label_len;

	if (dst_size == 0)
		return;
	if (label_width < 0)
		label_width = 0;

	label_len = (label != NULL) ? (int)strlen(label) : 0;
	pad_spaces = label_width - label_len;
	if (pad_spaces < 0)
		pad_spaces = 0;

	prefix_len = snprintf(dst, dst_size, "  %s: %*s", label ? label : "", pad_spaces, "");
	if (prefix_len < 0 || prefix_len >= (int)dst_size) {
		dst[dst_size - 1] = '\0';
		return;
	}

	value_len = (int)dst_size - prefix_len - 1;
	if (value_len < 0)
		value_len = 0;
	snprintf(dst + prefix_len, dst_size - prefix_len, "%.*s", value_len, value ? value : "");
}
//---------------------------------------------------------------------------
// End of declarations
// Start of functions
//---------------------------------------------------------------------------
// get_CNF_string is the main CNF parser called for each CNF variable in a
// CNF file. Input and output data is handled via its pointer parameters.
// The return value flags 'false' when no variable is found. (normal at EOF)
//---------------------------------------------------------------------------
int get_CNF_string(char **CNF_p_p,
                   char **name_p_p,
                   char **value_p_p)
{
	char *np, *vp, *tp = *CNF_p_p;

start_line:
	while ((*tp <= ' ') && (*tp > '\0'))
		tp += 1;  //Skip leading whitespace, if any
	if (*tp == '\0')
		return false;  //but exit at EOF
	np = tp;           //Current pos is potential name
	if (*tp < 'A')     //but may be a comment line
	{                  //We must skip a comment line
		while ((*tp != '\r') && (*tp != '\n') && (*tp > '\0'))
			tp += 1;      //Seek line end
		goto start_line;  //Go back to try next line
	}

	while ((*tp >= 'A') || ((*tp >= '0') && (*tp <= '9')))
		tp += 1;  //Seek name end
	if (*tp == '\0')
		return false;  //but exit at EOF

	while ((*tp <= ' ') && (*tp > '\0'))
		*tp++ = '\0';  //zero&skip post-name whitespace
	if (*tp != '=')
		return false;  //exit (syntax error) if '=' missing
	*tp++ = '\0';      //zero '=' (possibly terminating name)

	while ((*tp <= ' ') && (*tp > '\0')  //Skip pre-value whitespace, if any
	       && (*tp != '\r') && (*tp != '\n'))
		tp += 1;  //but do not pass the end of the line
	if (*tp == '\0')
		return false;  //but exit at EOF
	vp = tp;           //Current pos is potential value

	while ((*tp != '\r') && (*tp != '\n') && (*tp != '\0'))
		tp += 1;  //Seek line end
	if (*tp != '\0')
		*tp++ = '\0';  //terminate value (passing if not EOF)
	while ((*tp <= ' ') && (*tp > '\0'))
		tp += 1;  //Skip following whitespace, if any

	*CNF_p_p = tp;    //return new CNF file position
	*name_p_p = np;   //return found variable name
	*value_p_p = vp;  //return found variable value
	return true;      //return control to caller
}  //Ends get_CNF_string

//---------------------------------------------------------------------------
int CheckMC(void)
{
	int dummy, ret;

	mcGetInfo(0, 0, &dummy, &dummy, &dummy);
	mcSync(0, NULL, &ret);

	if (-1 == ret || 0 == ret)
		return 0;

	mcGetInfo(1, 0, &dummy, &dummy, &dummy);
	mcSync(0, NULL, &ret);

	if (-1 == ret || 0 == ret)
		return 1;

	return -11;
}

static int getLaunchMcPort(void)
{
	int mcport;

	if (!strncmp(LaunchElfDir, "mc0", 3))
		return 0;
	if (!strncmp(LaunchElfDir, "mc1", 3))
		return 1;

	if (!strncmp(LaunchElfDir, "mc:", 3)) {
		mcport = CheckMC();
		if (mcport == 0 || mcport == 1)
			return mcport;
		return 0;
	}

	mcport = CheckMC();
	if (mcport == 0 || mcport == 1)
		return mcport;

	return 0;
}
//---------------------------------------------------------------------------
static char *preloadCNFFd(int fd, const char *cnf_path)
{
	int rd, total, cnf_seek_size;
	size_t CNF_size;
	char probe;
	char *RAM_p;
	const int CNF_MAX_SIZE = 128 * 1024;

	if (fd < 0)
		return NULL;

	cnf_seek_size = genLseek(fd, 0, SEEK_END);
	DPRINTF("%s: CNF_size=%d\n", __func__, cnf_seek_size);
	if (cnf_seek_size > 0 && genLseek(fd, 0, SEEK_SET) >= 0) {
		CNF_size = (size_t)cnf_seek_size;
		RAM_p = (char *)memalign(64, CNF_size + 1);
		if (RAM_p == NULL) {
			goto fallback_stream;
		}

		rd = genRead(fd, RAM_p, CNF_size);  //Read CNF as one long string
		genClose(fd);
		if (rd <= 0) {
			free(RAM_p);
			return NULL;
		}
		RAM_p[rd] = '\0';  //Terminate the CNF string
		return RAM_p;
	}

	/*
	 * Some stacks may fail SEEK_END/SEEK_SET even though reads are valid.
	 * Retry with a streamed read and conservative size cap.
	 */
fallback_stream:
	genClose(fd);
	fd = genOpen(cnf_path, FIO_O_RDONLY);
	if (fd < 0)
		return NULL;

	RAM_p = (char *)memalign(64, CNF_MAX_SIZE + 1);
	if (RAM_p == NULL) {
		genClose(fd);
		return NULL;
	}
	memset(RAM_p, 0, CNF_MAX_SIZE + 1);

	total = 0;
	while (total < CNF_MAX_SIZE) {
		int chunk = CNF_MAX_SIZE - total;
		if (chunk > 4096)
			chunk = 4096;

		rd = genRead(fd, RAM_p + total, chunk);
		if (rd < 0) {
			free(RAM_p);
			genClose(fd);
			return NULL;
		}
		if (rd == 0)
			break;
		total += rd;
	}
	if (total == CNF_MAX_SIZE && genRead(fd, &probe, 1) > 0) {
		// Reject suspiciously large CNF payloads.
		free(RAM_p);
		genClose(fd);
		return NULL;
	}

	genClose(fd);
	if (total <= 0) {
		free(RAM_p);
		return NULL;
	}
	RAM_p[total] = '\0';
	return RAM_p;
}
//preloadCNF loads an entire CNF file into RAM it allocates
//------------------------------
char *preloadCNF(char *path)
{
	int fd, tst;
	char cnf_path[MAX_PATH];

	fd = -1;
	if ((tst = genFixPath(path, cnf_path)) >= 0)
		fd = genOpen(cnf_path, FIO_O_RDONLY);
	if (fd < 0)
		return NULL;

	return preloadCNFFd(fd, cnf_path);
}
//------------------------------
//endfunc preloadCNF
// Save LAUNCHELF.CNF
//---------------------------------------------------------------------------
void saveConfig(char *mainMsg, char *CNF)
{
	int i, ret, fd;
	char c[MAX_PATH], tmp[26 * MAX_PATH + 30 * MAX_PATH];
	char cnf_path[MAX_PATH];
	size_t CNF_size, CNF_step;

	sprintf(tmp, "CNF_version = 3\r\n%n", &CNF_size);  //Start CNF with version header

	for (i = 0; i < SETTING_LK_COUNT; i++) {  //Loop to save the ELF paths for launch keys
		if ((i <= SETTING_LK_SELECT) || (setting->LK_Flag[i] != 0)) {
			sprintf(tmp + CNF_size,
			        "LK_%s_E1 = %s\r\n"
			        "%n",  // %n causes NO output, but only a measurement
			        LK_ID[i],
			        setting->LK_Path[i],
			        &CNF_step  // This variable measures the size of sprintf data
			        );
			CNF_size += CNF_step;
		}
	}  //ends for

	i = strlen(setting->Misc);
	sprintf(tmp + CNF_size,
	        "Misc = %s\r\n"
	        "Misc_PS2Disc = %s\r\n"
	        "Misc_FileBrowser = %s\r\n"
	        "Misc_PS2Browser = %s\r\n"
	        "Misc_PS2Net = %s\r\n"
	        "Misc_PS2PowerOff = %s\r\n"
	        "Misc_HddManager = %s\r\n"
	        "Misc_TextEditor = %s\r\n"
	        "Misc_Configure = %s\r\n"
	        "Misc_ShowFont = %s\r\n"
	        "Misc_Debug_Info = %s\r\n"
	        "Misc_About_uLE = %s\r\n"
	        "Misc_Show_Build_Info = %s\r\n"
	        "Misc_OSDSYS = %s\r\n"
	        "%n",  // %n causes NO output, but only a measurement
	        setting->Misc,
	        setting->Misc_PS2Disc + i,
	        setting->Misc_FileBrowser + i,
	        setting->Misc_PS2Browser + i,
	        setting->Misc_PS2Net + i,
	        setting->Misc_PS2PowerOff + i,
	        setting->Misc_HddManager + i,
	        setting->Misc_TextEditor + i,
	        setting->Misc_Configure + i,
	        setting->Misc_ShowFont + i,
	        setting->Misc_Debug_Info + i,
	        setting->Misc_About_uLE + i,
	        setting->Misc_Show_Build_Info + i,
	        setting->Misc_OSDSYS + i,
	        &CNF_step  // This variable measures the size of sprintf data
	        );
	CNF_size += CNF_step;

	CNF_size += storeGuiColorsCNF(tmp + CNF_size);

	sprintf(tmp + CNF_size,
	        "LK_auto_Timer = %d\r\n"
	        "Menu_Hide_Paths = %d\r\n"
	        "GUI_Swap_Keys = %d\r\n"
	        "NET_HOSTwrite = %d\r\n"
	        "APP_GameID = %d\r\n"
	        "CDROM_Disable_GameID = %d\r\n"
	        "Menu_Title = %s\r\n"
	        "Init_Delay = %d\r\n"
	        "USBKBD_USED = %d\r\n"
	        "REBOOT_IOP_ELFLOAD = %d\r\n"
	        "USBKBD_FILE = %s\r\n"
	        "KBDMAP_FILE = %s\r\n"
	        "Menu_Show_Titles = %d\r\n"
	        "PathPad_Lock = %d\r\n"
	        "CNF_Path = %s\r\n"
	        "LANG_FILE = %s\r\n"
	        "FONT_FILE = %s\r\n"
	        "PSU_HugeNames = %d\r\n"
	        "PSU_DateNames = %d\r\n"
	        "PSU_NoOverwrite = %d\r\n"
	        "FB_NoIcons = %d\r\n"
	        "%n",                      // %n causes NO output, but only a measurement
	        setting->timeout,          //auto_Timer
	        setting->Hide_Paths,       //Menu_Hide_Paths
	        setting->swapKeys,         //GUI_Swap_Keys
	        setting->HOSTwrite,        //NET_HOST_write
	        setting->app_gameid,       //app_gameid
	        setting->cdrom_disable_gameid, //cdrom_disable_gameid
	        setting->Menu_Title,       //Menu_Title
	        setting->Init_Delay,       //Init_Delay
	        setting->usbkbd_used,      //USBKBD_USED
	        setting->reboot_iop_elf_load,
	        setting->usbkbd_file,      //USBKBD_FILE
	        setting->kbdmap_file,      //KBDMAP_FILE
	        setting->Show_Titles,      //Menu_Show_Titles
	        setting->PathPad_Lock,     //PathPad_Lock
	        setting->CNF_Path,         //CNF_Path
	        setting->lang_file,        //LANG_FILE
	        setting->font_file,        //FONT_FILE
	        setting->PSU_HugeNames,    //PSU_HugeNames
	        setting->PSU_DateNames,    //PSU_DateNames
	        setting->PSU_NoOverwrite,  //PSU_NoOverwrite
	        setting->FB_NoIcons,       //FB_NoIcons
	        &CNF_step                  // This variable measures the size of sprintf data
	        );
	CNF_size += CNF_step;

	for (i = 0; i < SETTING_LK_BTN_COUNT; i++) {  //Loop to save user defined launch key titles
		if (setting->LK_Title[i][0]) {            //Only save non-empty strings
			sprintf(tmp + CNF_size,
			        "LK_%s_Title = %s\r\n"
			        "%n",  // %n causes NO output, but only a measurement
			        LK_ID[i],
			        setting->LK_Title[i],
			        &CNF_step  // This variable measures the size of sprintf data
			        );
			CNF_size += CNF_step;
		}  //ends if
	}      //ends for

	sprintf(tmp + CNF_size,
	        "PathPad_Lock = %d\r\n"
	        "%n",                   // %n causes NO output, but only a measurement
	        setting->PathPad_Lock,  //PathPad_Lock
	        &CNF_step               // This variable measures the size of sprintf data
	        );
	CNF_size += CNF_step;

	for (i = 0; i < MAX_PATH_PAD; i++) {  //Loop to save non-empty PathPad entries
		if (PathPad[i][0]) {              //Only save non-empty strings
			sprintf(tmp + CNF_size,
			        "PathPad[%02d] = %s\r\n"
			        "%n",  // %n causes NO output, but only a measurement
			        i,
			        PathPad[i],
			        &CNF_step  // This variable measures the size of sprintf data
			        );
			CNF_size += CNF_step;
		}  //ends if
	}      //ends for

	strcpy(c, LaunchElfDir);
	strcat(c, CNF);
	ret = genFixPath(c, cnf_path);
	if ((ret >= 0) && ((fd = genOpen(cnf_path, FIO_O_RDONLY)) >= 0))
		genClose(fd);
	else {                                //Start of clause for failure to use LaunchElfDir
		if (setting->CNF_Path[0] == 0) {  //if NO CNF Path override defined
			sprintf(c, "mc%d:/SYS-CONF", getLaunchMcPort());

			if ((fd = fileXioDopen(c)) >= 0) {
				fileXioDclose(fd);
				char strtmp[MAX_PATH] = "/";
				strcat(c, strcat(strtmp, CNF));
			} else {
				strcpy(c, LaunchElfDir);
				strcat(c, CNF);
			}
		}
	}  //End of clause for failure to use LaunchElfDir

	ret = genFixPath(c, cnf_path);
	if ((ret < 0) || ((fd = genOpen(cnf_path, FIO_O_CREAT | FIO_O_WRONLY | FIO_O_TRUNC)) < 0)) {
		sprintf(c, "mc%d:/SYS-CONF", getLaunchMcPort());
		if ((fd = fileXioDopen(c)) >= 0) {
			fileXioDclose(fd);
			char strtmp[MAX_PATH] = "/";
			strcat(c, strcat(strtmp, CNF));
		} else {
			strcpy(c, LaunchElfDir);
			strcat(c, CNF);
		}
		ret = genFixPath(c, cnf_path);
		if ((fd = genOpen(cnf_path, FIO_O_CREAT | FIO_O_WRONLY | FIO_O_TRUNC)) < 0) {
			sprintf(mainMsg, "%s %s", LNG(Failed_To_Save), CNF);
			return;
		}
	}
	ret = genWrite(fd, &tmp, CNF_size);
	if (ret == CNF_size)
		sprintf(mainMsg, "%s (%s)", LNG(Saved_Config), c);
	else
		sprintf(mainMsg, "%s (%s)", LNG(Failed_writing), CNF);
	genClose(fd);
}
//---------------------------------------------------------------------------
void initConfig(void)
{
	int i;

	if (setting != NULL)
		free(setting);
	setting = (SETTING *)malloc(sizeof(SETTING));

	sprintf(setting->Misc, "%s/", LNG_DEF(MISC));
	sprintf(setting->Misc_PS2Disc, "%s/%s", LNG_DEF(MISC), LNG_DEF(PS2Disc));
	sprintf(setting->Misc_FileBrowser, "%s/%s", LNG_DEF(MISC), LNG_DEF(FileBrowser));
	sprintf(setting->Misc_PS2Browser, "%s/%s", LNG_DEF(MISC), LNG_DEF(PS2Browser));
	sprintf(setting->Misc_PS2Net, "%s/%s", LNG_DEF(MISC), LNG_DEF(PS2Net));
	sprintf(setting->Misc_PS2PowerOff, "%s/%s", LNG_DEF(MISC), LNG_DEF(PS2PowerOff));
	sprintf(setting->Misc_HddManager, "%s/%s", LNG_DEF(MISC), LNG_DEF(HddManager));
	sprintf(setting->Misc_TextEditor, "%s/%s", LNG_DEF(MISC), LNG_DEF(TextEditor));
	sprintf(setting->Misc_Configure, "%s/%s", LNG_DEF(MISC), LNG_DEF(Configure));
	sprintf(setting->Misc_ShowFont, "%s/%s", LNG_DEF(MISC), LNG_DEF(ShowFont));
	sprintf(setting->Misc_Debug_Info, "%s/%s", LNG_DEF(MISC), LNG_DEF(Debug_Info));
	sprintf(setting->Misc_About_uLE, "%s/%s", LNG_DEF(MISC), LNG_DEF(About_uLE));
	sprintf(setting->Misc_Show_Build_Info, "%s/%s", LNG(MISC), LNG(Build_Info));
	sprintf(setting->Misc_OSDSYS, "%s/%s", LNG_DEF(MISC), LNG_DEF(OSDSYS));

	for (i = 0; i < SETTING_LK_COUNT; i++) {
		setting->LK_Path[i][0] = 0;
		setting->LK_Title[i][0] = 0;
		setting->LK_Flag[i] = 0;
	}
	for (i = 0; i < MAX_PATH_PAD; i++)
		PathPad[i][0] = 0;

	strcpy(setting->LK_Path[SETTING_LK_CIRCLE], setting->Misc_FileBrowser);
	setting->LK_Flag[SETTING_LK_CIRCLE] = 1;
	strcpy(setting->LK_Path[SETTING_LK_TRIANGLE], setting->Misc_About_uLE);
	setting->LK_Flag[SETTING_LK_TRIANGLE] = 1;
	setting->usbkbd_file[0] = '\0';
	setting->kbdmap_file[0] = '\0';
	setting->Menu_Title[0] = '\0';
	setting->CNF_Path[0] = '\0';
	setting->lang_file[0] = '\0';
	setting->font_file[0] = '\0';
	setting->timeout = DEF_TIMEOUT;
	setting->Hide_Paths = DEF_HIDE_PATHS;
	setting->color[COLOR_BACKGR] = DEF_COLOR1;
	setting->color[COLOR_FRAME] = DEF_COLOR2;
	setting->color[COLOR_SELECT] = DEF_COLOR3;
	setting->color[COLOR_TEXT] = DEF_COLOR4;
	setting->color[COLOR_GRAPH1] = DEF_COLOR5;
	setting->color[COLOR_GRAPH2] = DEF_COLOR6;
	setting->color[COLOR_GRAPH3] = DEF_COLOR7;
	setting->color[COLOR_GRAPH4] = DEF_COLOR8;
	setting->screen_x = 0;
	setting->screen_y = 0;
	setting->Menu_Frame = DEF_MENU_FRAME;
	setting->swapKeys = DEF_SWAPKEYS;
	setting->HOSTwrite = DEF_HOSTWRITE;
	setting->app_gameid = DEF_APP_GAMEID;
	setting->cdrom_disable_gameid = DEF_CDROM_DISABLE_GAMEID;
	setting->TV_mode = TV_mode_AUTO;
	setting->Popup_Opaque = DEF_POPUP_OPAQUE;
	setting->Init_Delay = DEF_INIT_DELAY;
	setting->usbkbd_used = DEF_USBKBD_USED;
	setting->reboot_iop_elf_load = DEF_STARTUP_RESET_IOP_ELFOAD;
	setting->Show_Titles = DEF_SHOW_TITLES;
	setting->PathPad_Lock = DEF_PATHPAD_LOCK;
	setting->PSU_HugeNames = DEF_PSU_HUGENAMES;
	setting->PSU_DateNames = DEF_PSU_DATENAMES;
	setting->PSU_NoOverwrite = DEF_PSU_NOOVERWRITE;
	setting->FB_NoIcons = DEF_FB_NOICONS;
}
//------------------------------
//endfunc initConfig
//---------------------------------------------------------------------------
// Load LAUNCHELF.CNF
// Return value: 0==OK, -1==failure
//---------------------------------------------------------------------------
int loadConfig(char *mainMsg, char *CNF)
{
	int i, fd, tst, len, mcport, var_cnt, CNF_version;
	char tsts[MAX_PATH];
	char path[MAX_PATH];
	char cnf_path[MAX_PATH];
	char *RAM_p, *CNF_p, *name, *value;

	initConfig();

	strcpy(path, LaunchElfDir);
	strcat(path, CNF);
	if (!strncmp(path, "cdrom", 5))
		strcat(path, ";1");

	fd = -1;
	if ((tst = genFixPath(path, cnf_path)) >= 0)
		fd = genOpen(cnf_path, FIO_O_RDONLY);
	if (fd < 0) {
		char strtmp[MAX_PATH];
		int ports_to_try[2];
		int port_count = 0;
		int port_ix;

		mcport = getLaunchMcPort();
		if (mcport == 0 || mcport == 1)
			ports_to_try[port_count++] = mcport;
		if (mcport != 0)
			ports_to_try[port_count++] = 0;
		if (mcport != 1)
			ports_to_try[port_count++] = 1;

		for (port_ix = 0; port_ix < port_count && fd < 0; port_ix++) {
			sprintf(strtmp, "mc%d:/SYS-CONF/", ports_to_try[port_ix]);
			strcpy(cnf_path, strtmp);
			strcat(cnf_path, CNF);
			fd = genOpen(cnf_path, FIO_O_RDONLY);
			if (fd >= 0)
				strcpy(LaunchElfDir, strtmp);
		}
	}
	if (fd < 0) {
	failed_load:
		sprintf(mainMsg, "%s %s", LNG(Failed_To_Load), CNF);
		return -1;
	}
	// This point is only reached after succesfully opening CNF.
	// Reuse this open descriptor to avoid probing/opening the same CNF twice.
	if ((RAM_p = preloadCNFFd(fd, cnf_path)) == NULL)
		goto failed_load;
	CNF_p = RAM_p;

	//RA NB: in the code below, the 'LK_' variables have been implemented such that
	//       any _Ex suffix will be accepted, with identical results. This will need
	//       to be modified when more execution methods are implemented.

	CNF_version = 0;                                                       // The CNF version is still unidentified
	for (var_cnt = 0; get_CNF_string(&CNF_p, &name, &value); var_cnt++) {  // A variable was found, now we dispose of its value.
		if (!strcmp(name, "CNF_version")) {
			CNF_version = atoi(value);
			continue;
		}

		if (scanGuiColorsCNF(name, value))
			continue;

		for (i = 0; i < SETTING_LK_COUNT; i++) {
			len = snprintf(tsts, sizeof(tsts), "LK_%s_E", LK_ID[i]);
			if ((len > 0) && (len < (int)sizeof(tsts)) && !strncmp(name, tsts, len)) {
				strcpy(setting->LK_Path[i], value);
				setting->LK_Flag[i] = 1;
				break;
			}
		}
		if (i < SETTING_LK_COUNT)
			continue;
		//----------
		//In the next group, the Misc device must be defined before its subprograms
		//----------
		else if (!strcmp(name, "Misc"))
			sprintf(setting->Misc, "%s/", value);
			else if (!strcmp(name, "Misc_PS2Disc"))
				sprintf(setting->Misc_PS2Disc, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_FileBrowser"))
				sprintf(setting->Misc_FileBrowser, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_PS2Browser"))
				sprintf(setting->Misc_PS2Browser, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_PS2Net"))
				sprintf(setting->Misc_PS2Net, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_PS2PowerOff"))
				sprintf(setting->Misc_PS2PowerOff, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_HddManager"))
				sprintf(setting->Misc_HddManager, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_TextEditor"))
				sprintf(setting->Misc_TextEditor, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_Configure"))
				sprintf(setting->Misc_Configure, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_ShowFont"))
				sprintf(setting->Misc_ShowFont, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_Debug_Info"))
				sprintf(setting->Misc_Debug_Info, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_About_uLE"))
				sprintf(setting->Misc_About_uLE, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_Show_Build_Info"))
				sprintf(setting->Misc_Show_Build_Info, "%s%s", setting->Misc, value);
			else if (!strcmp(name, "Misc_OSDSYS"))
				sprintf(setting->Misc_OSDSYS, "%s%s", setting->Misc, value);
			//----------
			else if (!strcmp(name, "LK_auto_Timer"))
				setting->timeout = atoi(value);
			else if (!strcmp(name, "Menu_Hide_Paths"))
				setting->Hide_Paths = atoi(value);
			//---------- NB: color settings moved to scanGuiColorsCNF
			else if (!strcmp(name, "Menu_Pages")) {
				// Legacy multi-CNF key, ignored.
			} else if (!strcmp(name, "GUI_Swap_Keys"))
				setting->swapKeys = atoi(value);
		else if (!strcmp(name, "NET_HOSTwrite"))
			setting->HOSTwrite = atoi(value);
		else if (!strcmp(name, "APP_GameID") || !strcmp(name, "app_gameid") || !strcmp(name, "Enable_GameID"))
			setting->app_gameid = atoi(value);
		else if (!strcmp(name, "CDROM_Disable_GameID") || !strcmp(name, "cdrom_disable_gameid") || !strcmp(name, "Disable_Disc_GameID"))
			setting->cdrom_disable_gameid = atoi(value);
		else if (!strcmp(name, "Menu_Title")) {
			strncpy(setting->Menu_Title, value, MAX_MENU_TITLE);
			setting->Menu_Title[MAX_MENU_TITLE] = '\0';
		} else if (!strcmp(name, "Init_Delay"))
			setting->Init_Delay = atoi(value);
		else if (!strcmp(name, "USBKBD_USED"))
			setting->usbkbd_used = atoi(value);
		else if (!strcmp(name, "REBOOT_IOP_ELFLOAD"))
			setting->reboot_iop_elf_load = atoi(value);
		else if (!strcmp(name, "USBKBD_FILE"))
			strcpy(setting->usbkbd_file, value);
		else if (!strcmp(name, "KBDMAP_FILE"))
			strcpy(setting->kbdmap_file, value);
		else if (!strcmp(name, "Menu_Show_Titles"))
			setting->Show_Titles = atoi(value);
		else if (!strcmp(name, "PathPad_Lock"))
			setting->PathPad_Lock = atoi(value);
		else if (!strcmp(name, "CNF_Path"))
			strcpy(setting->CNF_Path, value);
		else if (!strcmp(name, "LANG_FILE"))
			strcpy(setting->lang_file, value);
		else if (!strcmp(name, "FONT_FILE"))
			strcpy(setting->font_file, value);
		//----------
		else if (!strcmp(name, "PSU_HugeNames"))
			setting->PSU_HugeNames = atoi(value);
		else if (!strcmp(name, "PSU_DateNames"))
			setting->PSU_DateNames = atoi(value);
		else if (!strcmp(name, "PSU_NoOverwrite"))
			setting->PSU_NoOverwrite = atoi(value);
		else if (!strcmp(name, "FB_NoIcons"))
			setting->FB_NoIcons = atoi(value);
			//----------
			else {
				for (i = 0; i < SETTING_LK_BTN_COUNT; i++) {
					snprintf(tsts, sizeof(tsts), "LK_%s_Title", LK_ID[i]);
					if (!strcmp(name, tsts)) {
						strncpy(setting->LK_Title[i], value, MAX_ELF_TITLE - 1);
						setting->LK_Title[i][MAX_ELF_TITLE - 1] = '\0';
						break;
					}
			}
			if (i < SETTING_LK_BTN_COUNT)
				continue;
			else if (!strncmp(name, "PathPad[", 8)) {
				i = atoi(name + 8);
				if (i < MAX_PATH_PAD) {
					strncpy(PathPad[i], value, MAX_PATH - 1);
					PathPad[i][MAX_PATH - 1] = '\0';
				}
			}
		}
	}  //ends for
	if (CNF_version == 0)
		DPRINTF("loadConfig: CNF_version missing, treating \"%s\" as legacy config.\n", cnf_path);
	for (i = 0; i < SETTING_LK_BTN_COUNT; i++)
		setting->LK_Title[i][MAX_ELF_TITLE - 1] = 0;
	free(RAM_p);
	sprintf(mainMsg, "%s (%s)", LNG(Loaded_Config), cnf_path);
	return 0;
}
//------------------------------
//endfunc loadConfig
//---------------------------------------------------------------------------
// Polo: added GUI color menu with preview
// suloku: added main cosmetic/color selection
//---------------------------------------------------------------------------


enum CONFIG_SCREEN {
	CONFIG_SCREEN_FIRST = 0,
	CONFIG_SCREEN_COL_FIRST = CONFIG_SCREEN_FIRST,
	CONFIG_SCREEN_COL_BACKGR_R = CONFIG_SCREEN_COL_FIRST,
	CONFIG_SCREEN_COL_BACKGR_G,
	CONFIG_SCREEN_COL_BACKGR_B,
	CONFIG_SCREEN_COL_FRAMES_R,
	CONFIG_SCREEN_COL_FRAMES_G,
	CONFIG_SCREEN_COL_FRAMES_B,
	CONFIG_SCREEN_COL_SELECT_R,
	CONFIG_SCREEN_COL_SELECT_G,
	CONFIG_SCREEN_COL_SELECT_B,
	CONFIG_SCREEN_COL_TEXT_R,
	CONFIG_SCREEN_COL_TEXT_G,
	CONFIG_SCREEN_COL_TEXT_B,
	CONFIG_SCREEN_COL_GRAPH1_R,
	CONFIG_SCREEN_COL_GRAPH1_G,
	CONFIG_SCREEN_COL_GRAPH1_B,
	CONFIG_SCREEN_COL_GRAPH2_R,
	CONFIG_SCREEN_COL_GRAPH2_G,
	CONFIG_SCREEN_COL_GRAPH2_B,
	CONFIG_SCREEN_COL_GRAPH3_R,
	CONFIG_SCREEN_COL_GRAPH3_G,
	CONFIG_SCREEN_COL_GRAPH3_B,
	CONFIG_SCREEN_COL_LAST,
	CONFIG_SCREEN_COL_GRAPH4_R = CONFIG_SCREEN_COL_LAST,
	CONFIG_SCREEN_COL_GRAPH4_G,
	CONFIG_SCREEN_COL_GRAPH4_B,

	//First option after colour selectors
	CONFIG_SCREEN_AFT_COLORS,
	CONFIG_SCREEN_TV_MODE = CONFIG_SCREEN_AFT_COLORS,
	CONFIG_SCREEN_TV_STARTX,
	CONFIG_SCREEN_TV_STARTY,

	CONFIG_SCREEN_MENU_TITLE,
	CONFIG_SCREEN_MENU_FRAME,
	CONFIG_SCREEN_POPUP_OPAQUE,

	CONFIG_SCREEN_RETURN,
	CONFIG_SCREEN_DEFAULT,

	CONFIG_SCREEN_COUNT,
};

static void Config_Screen(void)
{
	int i;
	int s, max_s = CONFIG_SCREEN_COUNT - 1;  //define cursor index and its max value
	int x, y;
	int len;
	int event, post_event = 0;
	u8 rgb[COLOR_COUNT][3];
	char c[MAX_PATH];
	char value_text[32];
	int bool_label_width;
	const char *tv_mode_value;
	int space = ((SCREEN_WIDTH - SCREEN_MARGIN - 4 * FONT_WIDTH) - (Menu_start_x + 2 * FONT_WIDTH)) / 8;

	event = 1;  //event = initial entry

	for (i = 0; i < COLOR_COUNT; i++) {
		rgb[i][0] = setting->color[i] & 0xFF;
		rgb[i][1] = setting->color[i] >> 8 & 0xFF;
		rgb[i][2] = setting->color[i] >> 16 & 0xFF;
	}

	s = CONFIG_SCREEN_FIRST;
		while (1) {
			//Pad response section
			waitPadReady(0, 0);
			if (readpad()) {
				if (new_pad & PAD_UP) {
				event |= 2;  //event |= valid pad command
				if (s == CONFIG_SCREEN_FIRST)
					s = max_s;
				else if (s == CONFIG_SCREEN_AFT_COLORS)
					s = CONFIG_SCREEN_COL_BACKGR_B;
				else
					s--;
			} else if (new_pad & PAD_DOWN) {
				event |= 2;  //event |= valid pad command
				if ((s < CONFIG_SCREEN_AFT_COLORS) && (s % 3 == 2))
					s = CONFIG_SCREEN_AFT_COLORS;
				else if (s == max_s)
					s = CONFIG_SCREEN_FIRST;
				else
					s++;
			} else if (new_pad & PAD_LEFT) {
				event |= 2;  //event |= valid pad command

				if (s >= CONFIG_SCREEN_RETURN)
					s = CONFIG_SCREEN_MENU_FRAME;
				else if (s >= CONFIG_SCREEN_MENU_FRAME)
					s = CONFIG_SCREEN_MENU_TITLE;
				else if (s >= CONFIG_SCREEN_MENU_TITLE)
					s = CONFIG_SCREEN_TV_MODE;
				else if (s >= CONFIG_SCREEN_TV_STARTX)
					s = CONFIG_SCREEN_TV_MODE;  //at or
				else if (s >= CONFIG_SCREEN_AFT_COLORS)
					s = CONFIG_SCREEN_COL_LAST;  //if s beyond color settings
				else if (s >= CONFIG_SCREEN_COL_FIRST + 3)
					s -= 3;  //if s in a color beyond the first colour, step to preceding color
			} else if (new_pad & PAD_RIGHT) {
				event |= 2;  //event |= valid pad command
				if (s >= CONFIG_SCREEN_MENU_FRAME)
					s = CONFIG_SCREEN_RETURN;
				else if (s >= CONFIG_SCREEN_MENU_TITLE)
					s = CONFIG_SCREEN_MENU_FRAME;
				else if (s >= CONFIG_SCREEN_TV_STARTX)
					s = CONFIG_SCREEN_MENU_TITLE;
				else if (s >= CONFIG_SCREEN_TV_MODE)
					s = CONFIG_SCREEN_TV_STARTX;
				else if (s >= CONFIG_SCREEN_COL_LAST)
					s = CONFIG_SCREEN_AFT_COLORS;  //if s in the last colour, move it to the first control after colour selection.
				else
					s += 3;
			} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {  //User pressed CANCEL=>Subtract/Clear
				event |= 2;                                                                         //event |= valid pad command
				if (s < CONFIG_SCREEN_AFT_COLORS) {
					if (rgb[s / 3][s % 3] > 0) {
						rgb[s / 3][s % 3]--;
						setting->color[s / 3] =
						    GS_SETREG_RGBA(rgb[s / 3][0], rgb[s / 3][1], rgb[s / 3][2], 0);
					}
				} else if (s == CONFIG_SCREEN_TV_STARTX) {
					if (setting->screen_x > -gsGlobal->StartX) {
						setting->screen_x--;
						updateScreenMode();
					}
				} else if (s == CONFIG_SCREEN_TV_STARTY) {
					if (setting->screen_y > -gsGlobal->StartY) {
						setting->screen_y--;
						updateScreenMode();
					}
				} else if (s == CONFIG_SCREEN_MENU_TITLE) {  //cursor is at Menu_Title
					setting->Menu_Title[0] = '\0';
				}
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {  //User pressed OK=>Add/Ok/Edit
				event |= 2;                                                                         //event |= valid pad command
				if (s < CONFIG_SCREEN_AFT_COLORS) {
					if (rgb[s / 3][s % 3] < 255) {
						rgb[s / 3][s % 3]++;
						setting->color[s / 3] =
						    GS_SETREG_RGBA(rgb[s / 3][0], rgb[s / 3][1], rgb[s / 3][2], 0);
					}
				} else if (s == CONFIG_SCREEN_TV_MODE) {
					setting->TV_mode = (setting->TV_mode + 1) % TV_mode_COUNT;  //Change between the various modes
					updateScreenMode();
				} else if (s == CONFIG_SCREEN_TV_STARTX) {
					if (setting->screen_x < gsGlobal->StartX) {
						setting->screen_x++;
						updateScreenMode();
					}
				} else if (s == CONFIG_SCREEN_TV_STARTY) {
					if (setting->screen_y < gsGlobal->StartY) {
						setting->screen_y++;
						updateScreenMode();
					}
				} else if (s == CONFIG_SCREEN_MENU_TITLE) {  //cursor is at Menu_Title
					char tmp[MAX_MENU_TITLE + 1];
					strcpy(tmp, setting->Menu_Title);
					if (keyboard(tmp, MAX_MENU_TITLE) >= 0)
						strcpy(setting->Menu_Title, tmp);
				} else if (s == CONFIG_SCREEN_MENU_FRAME) {
					setting->Menu_Frame = !setting->Menu_Frame;
				} else if (s == CONFIG_SCREEN_POPUP_OPAQUE) {
					setting->Popup_Opaque = !setting->Popup_Opaque;
				} else if (s == CONFIG_SCREEN_RETURN) {  //Always put 'RETURN' next to last
					return;
				} else if (s == CONFIG_SCREEN_DEFAULT) {  //Always put 'DEFAULT SCREEN SETTINGS' last
					setting->color[COLOR_BACKGR] = DEF_COLOR1;
					setting->color[COLOR_FRAME] = DEF_COLOR2;
					setting->color[COLOR_SELECT] = DEF_COLOR3;
					setting->color[COLOR_TEXT] = DEF_COLOR4;
					setting->color[COLOR_GRAPH1] = DEF_COLOR5;
					setting->color[COLOR_GRAPH2] = DEF_COLOR6;
					setting->color[COLOR_GRAPH3] = DEF_COLOR7;
					setting->color[COLOR_GRAPH4] = DEF_COLOR8;
					setting->TV_mode = TV_mode_AUTO;
					setting->screen_x = 0;
					setting->screen_y = 0;
					setting->Menu_Frame = DEF_MENU_FRAME;
					setting->Popup_Opaque = DEF_POPUP_OPAQUE;
					updateScreenMode();

					for (i = 0; i < COLOR_COUNT; i++) {
						rgb[i][0] = setting->color[i] & 0xFF;
						rgb[i][1] = setting->color[i] >> 8 & 0xFF;
						rgb[i][2] = setting->color[i] >> 16 & 0xFF;
					}
				}
			} else if (new_pad & PAD_TRIANGLE)
				return;
		}

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[COLOR_BACKGR]);

			x = Menu_start_x;

			for (i = 0; i < COLOR_COUNT; i++) {
				y = Menu_start_y;
				sprintf(c, "%s%d", LNG(Color), i + 1);
				printXY(c, x + (space * (i + 1)) - (printXY(c, 0, 0, 0, FALSE, space - FONT_WIDTH / 2) / 2), y,
				        setting->color[COLOR_TEXT], TRUE, space - FONT_WIDTH / 2);
				if (i == COLOR_BACKGR)
					sprintf(c, "%s", LNG(Backgr));
				else if (i == COLOR_FRAME)
					sprintf(c, "%s", LNG(Frames));
				else if (i == COLOR_SELECT)
					sprintf(c, "%s", LNG(Select));
				else if (i == COLOR_TEXT)
					sprintf(c, "%s", LNG(Normal));
				else if (i >= COLOR_GRAPH1)
					sprintf(c, "%s%d", LNG(Graph), i - COLOR_GRAPH1 + 1);
				printXY(c, x + (space * (i + 1)) - (printXY(c, 0, 0, 0, FALSE, space - FONT_WIDTH / 2) / 2), y + FONT_HEIGHT,
				        setting->color[COLOR_TEXT], TRUE, space - FONT_WIDTH / 2);
				y += FONT_HEIGHT * 2;
				printXY("R:", x, y, setting->color[COLOR_TEXT], TRUE, 0);
				sprintf(c, "%02X", rgb[i][0]);
				printXY(c, x + (space * (i + 1)) - FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				printXY("G:", x, y, setting->color[COLOR_TEXT], TRUE, 0);
				sprintf(c, "%02X", rgb[i][1]);
				printXY(c, x + (space * (i + 1)) - FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				printXY("B:", x, y, setting->color[COLOR_TEXT], TRUE, 0);
				sprintf(c, "%02X", rgb[i][2]);
				printXY(c, x + (space * (i + 1)) - FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				sprintf(c, "\xFF"
				           "4");
				printXY(c, x + (space * (i + 1)) - FONT_WIDTH, y, setting->color[i], TRUE, 0);
				}  //ends loop for colour RGB values
				y += FONT_HEIGHT * 2;
				bool_label_width = (int)strlen(LNG(TV_mode));
				if ((int)strlen(LNG(Screen_X_offset)) > bool_label_width)
					bool_label_width = (int)strlen(LNG(Screen_X_offset));
				if ((int)strlen(LNG(Screen_Y_offset)) > bool_label_width)
					bool_label_width = (int)strlen(LNG(Screen_Y_offset));
				if ((int)strlen(LNG(Menu_Frame)) > bool_label_width)
					bool_label_width = (int)strlen(LNG(Menu_Frame));
				if ((int)strlen(LNG(Popups_Opaque)) > bool_label_width)
					bool_label_width = (int)strlen(LNG(Popups_Opaque));
				if (setting->TV_mode == TV_mode_NTSC)
					tv_mode_value = "NTSC";
				else if (setting->TV_mode == TV_mode_PAL)
					tv_mode_value = "PAL";
				else if (setting->TV_mode == TV_mode_VGA)
					tv_mode_value = "VGA";
				else if (setting->TV_mode == TV_mode_480P)
					tv_mode_value = "Progressive";
				else
					tv_mode_value = "AUTO";
				formatLabelValueAligned(c, sizeof(c), LNG(TV_mode), tv_mode_value, bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				y += FONT_HEIGHT / 2;

			snprintf(value_text, sizeof(value_text), "%d", setting->screen_x);
			formatLabelValueAligned(c, sizeof(c), LNG(Screen_X_offset), value_text, bool_label_width);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			snprintf(value_text, sizeof(value_text), "%d", setting->screen_y);
			formatLabelValueAligned(c, sizeof(c), LNG(Screen_Y_offset), value_text, bool_label_width);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			if (setting->Menu_Title[0] == '\0')
				sprintf(c, "  %s: %s", LNG(Menu_Title), LNG(NULL));
			else
				sprintf(c, "  %s: %s", LNG(Menu_Title), setting->Menu_Title);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

				formatLabelValueAligned(c, sizeof(c), LNG(Menu_Frame), setting->Menu_Frame ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;

				formatLabelValueAligned(c, sizeof(c), LNG(Popups_Opaque), setting->Popup_Opaque ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				y += FONT_HEIGHT / 2;

			sprintf(c, "  %s", LNG(RETURN));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			sprintf(c, "  %s", LNG(Use_Default_Screen_Settings));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			//Cursor positioning section
			x = Menu_start_x;
			y = Menu_start_y;

			if (s < CONFIG_SCREEN_AFT_COLORS) {  //if cursor indicates a colour component
				int colnum = s / 3;
				int comnum = s - colnum * 3;
				x += (space * (colnum + 1)) - (FONT_WIDTH * 4);
				y += (2 + comnum) * FONT_HEIGHT;
			} else {                                                                      //if cursor indicates anything after colour components
				y += (s - CONFIG_SCREEN_AFT_COLORS + 6) * FONT_HEIGHT + FONT_HEIGHT / 2;  //adjust y for cursor beyond colours
				//Here y is almost correct, except for additional group spacing
				if (s >= CONFIG_SCREEN_AFT_COLORS)  //if cursor at or beyond TV mode choice
					y += FONT_HEIGHT / 2;           //adjust for half-row space below colours
				if (s >= CONFIG_SCREEN_TV_STARTX)   //if cursor at or beyond screen offsets
					y += FONT_HEIGHT / 2;           //adjust for half-row space below TV mode choice
				if (s >= CONFIG_SCREEN_MENU_TITLE)  //if cursor at or beyond 'Menu Title'
					y += FONT_HEIGHT / 2;           //adjust for half-row space below screen offsets
				if (s >= CONFIG_SCREEN_MENU_FRAME)  //if cursor at or beyond 'Menu Frame'
					y += FONT_HEIGHT / 2;           //adjust for half-row space below 'Menu Title'
				if (s >= CONFIG_SCREEN_RETURN)      //if cursor at or beyond 'RETURN'
					y += FONT_HEIGHT / 2;           //adjust for half-row space below 'Popups Opaque'
			}
			drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);  //draw cursor

			//Tooltip section
			if (s < CONFIG_SCREEN_AFT_COLORS || s == CONFIG_SCREEN_TV_STARTX || s == CONFIG_SCREEN_TV_STARTY) {  //if cursor at a colour component or a screen offset
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
			} else if (s == CONFIG_SCREEN_TV_MODE || s == CONFIG_SCREEN_MENU_FRAME || s == CONFIG_SCREEN_POPUP_OPAQUE) {
				//if cursor at 'TV mode', 'Menu Frame' or 'Popups Opaque'
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s",
					              LNG(Change));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s",
					              LNG(Change));
			} else if (s == CONFIG_SCREEN_MENU_TITLE) {  //if cursor at Menu_Title
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s \xFF"
					                 "0:%s",
					              LNG(Edit), LNG(Clear));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s \xFF"
					                 "1:%s",
					              LNG(Edit), LNG(Clear));
			} else {  //if cursor at 'RETURN' or 'DEFAULT' options
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
}  //ends Config_Screen
//---------------------------------------------------------------------------
// Other settings by EP
// sincro: ADD USBD SELECTOR MENU
// dlanor: Add Menu_Title config
//---------------------------------------------------------------------------
enum CONFIG_STARTUP {
	CONFIG_STARTUP_FIRST = 1,
	CONFIG_STARTUP_SELECT_BTN = CONFIG_STARTUP_FIRST,
	CONFIG_STARTUP_INIT_DELAY,
	CONFIG_STARTUP_TIMEOUT,
	CONFIG_STARTUP_RESET_IOP_ELFOAD,
	CONFIG_STARTUP_KEYBOARD,
	CONFIG_STARTUP_USBKBD,
	CONFIG_STARTUP_KBDMAP,
	CONFIG_STARTUP_CNF,
	CONFIG_STARTUP_LANG,
	CONFIG_STARTUP_FONT,
	CONFIG_STARTUP_ESR,
	CONFIG_STARTUP_OSDSYS,

	CONFIG_STARTUP_RETURN,

	CONFIG_STARTUP_COUNT
};

static int getConfigStartupItemY(int s)
{
	int y = Menu_start_y + FONT_HEIGHT + FONT_HEIGHT / 2;
	int i;

	if (s <= CONFIG_STARTUP_FIRST)
		return y;

	for (i = CONFIG_STARTUP_FIRST; i < s; i++) {
		y += FONT_HEIGHT;
		if (i == CONFIG_STARTUP_RESET_IOP_ELFOAD || i == CONFIG_STARTUP_KBDMAP || i == CONFIG_STARTUP_OSDSYS)
			y += FONT_HEIGHT / 2;
	}

	return y;
}

static void Config_Startup(void)
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
					if (s == CONFIG_STARTUP_INIT_DELAY && setting->Init_Delay > 0)
						setting->Init_Delay--;
					else if (s == CONFIG_STARTUP_TIMEOUT && setting->timeout > 0)
						setting->timeout--;
					else if (s == CONFIG_STARTUP_USBKBD)
						setting->usbkbd_file[0] = '\0';
					else if (s == CONFIG_STARTUP_KBDMAP)
						setting->kbdmap_file[0] = '\0';
					else if (s == CONFIG_STARTUP_CNF)
						setting->CNF_Path[0] = '\0';
					else if (s == CONFIG_STARTUP_LANG) {
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
					}
				} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
					event |= 2;  //event |= valid pad command
					if (s == CONFIG_STARTUP_SELECT_BTN)
						setting->swapKeys = !setting->swapKeys;
					else if (s == CONFIG_STARTUP_INIT_DELAY)
						setting->Init_Delay++;
					else if (s == CONFIG_STARTUP_TIMEOUT)
						setting->timeout++;
					else if (s == CONFIG_STARTUP_KEYBOARD)
						setting->usbkbd_used = !setting->usbkbd_used;
					else if (s == CONFIG_STARTUP_RESET_IOP_ELFOAD)
						setting->reboot_iop_elf_load = !setting->reboot_iop_elf_load;
					else if (s == CONFIG_STARTUP_USBKBD)
						getFilePath(setting->usbkbd_file, USBKBD_IRX_CNF);
					else if (s == CONFIG_STARTUP_KBDMAP)
						getFilePath(setting->kbdmap_file, KBDMAP_FILE_CNF);
					else if (s == CONFIG_STARTUP_CNF) {
						char *tmp;
						getFilePath(setting->CNF_Path, CNF_PATH_CNF);
						if ((tmp = strrchr(setting->CNF_Path, '/')))
							tmp[1] = '\0';
					} else if (s == CONFIG_STARTUP_LANG) {
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
					} else if (s == CONFIG_STARTUP_RETURN)
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
			
			sprintf(c, "  %s: %s", LNG(USB_Keyboard_Used), (setting->usbkbd_used) ? LNG(ON): LNG(OFF));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			formatLabelValue(c, sizeof(c), LNG(USB_Keyboard_IRX), (strlen(setting->usbkbd_file) == 0) ? LNG(DEFAULT) : setting->usbkbd_file);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			formatLabelValue(c, sizeof(c), LNG(USB_Keyboard_Map), (strlen(setting->kbdmap_file) == 0) ? LNG(DEFAULT) : setting->kbdmap_file);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			formatLabelValue(c, sizeof(c), LNG(CNF_Path_override), (strlen(setting->CNF_Path) == 0) ? LNG(NONE) : setting->CNF_Path);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			formatLabelValue(c, sizeof(c), LNG(Language_File), (strlen(setting->lang_file) == 0) ? LNG(DEFAULT) : setting->lang_file);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			formatLabelValue(c, sizeof(c), LNG(Font_File), (strlen(setting->font_file) == 0) ? LNG(DEFAULT) : setting->font_file);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			formatLabelValue(c, sizeof(c), "ESR elf", (strlen(setting->LK_Path[SETTING_LK_ESR]) == 0) ? LNG(DEFAULT) : setting->LK_Path[SETTING_LK_ESR]);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			formatLabelValue(c, sizeof(c), "OSDSYS kelf", (strlen(setting->LK_Path[SETTING_LK_OSDSYS]) == 0) ? LNG(DEFAULT) : setting->LK_Path[SETTING_LK_OSDSYS]);
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
			if ((s == CONFIG_STARTUP_SELECT_BTN) || (s == CONFIG_STARTUP_KEYBOARD) || (s == CONFIG_STARTUP_RESET_IOP_ELFOAD)) {  //usbkbd_used
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s",
					              LNG(Change));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s",
					              LNG(Change));
			} else if ((s == CONFIG_STARTUP_INIT_DELAY) || (s == CONFIG_STARTUP_TIMEOUT)) {  //Init_Delay || timeout
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
			           //Language||Fontfile||ESR_elf||OSDSYS_kelf
			           || (s == CONFIG_STARTUP_LANG) || (s == CONFIG_STARTUP_FONT) || (s == CONFIG_STARTUP_ESR) || (s == CONFIG_STARTUP_OSDSYS)) {
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
// Network settings GUI by Slam-Tilt
//---------------------------------------------------------------------------
static void saveNetworkSettings(char *Message)
{
	char firstline[50];
	int out_fd, in_fd;
	int ret = 0, i = 0, port;
	int size, sizeleft = 0;
	char *ipconfigfile = 0;
	char path[MAX_PATH];

	// Default message, will get updated if save is sucessfull
	sprintf(Message, "%s", LNG(Saved_Failed));

	sprintf(firstline, "%s %s %s\n\r", ip, netmask, gw);



	// This block looks at the existing ipconfig.dat and works out if there is
	// already any data beyond the first line. If there is it will get appended to the output
	// to new file later.

	if (genFixPath("uLE:/IPCONFIG.DAT", path) >= 0)
		in_fd = genOpen(path, FIO_O_RDONLY);
	else
		in_fd = -1;

	if (strncmp(path, "mc", 2)) {
		mcSync(0, NULL, NULL);
		mcMkDir(0, 0, "SYS-CONF");
		mcSync(0, NULL, &ret);
	}
	if (in_fd >= 0) {

		size = genLseek(in_fd, 0, SEEK_END);
		DPRINTF("%s: size of existing file is %ibytes\n\r", __func__, size);

		ipconfigfile = (char *)memalign(64, size);

		genLseek(in_fd, 0, SEEK_SET);
		genRead(in_fd, ipconfigfile, size);


		/*for (i = 0; (ipconfigfile[i] != 0 && i <= size); i++)
		{
			// DPRINTF("%i-%c\n\r",i,ipconfigfile[i]);
		}*/

		sizeleft = size - i;

		genClose(in_fd);
	} else {
		port = CheckMC();
		if (port < 0)
			port = 0;  //Default to mc0, if it fails.
		sprintf(path, "mc%d:/SYS-CONF/IPCONFIG.DAT", port);
	}

	// Writing the data out

	out_fd = genOpen(path, FIO_O_WRONLY | FIO_O_TRUNC | FIO_O_CREAT);
	if (out_fd >= 0) {
		mcSync(0, NULL, &ret);
		genWrite(out_fd, firstline, strlen(firstline));
		mcSync(0, NULL, &ret);

		// If we have any extra data, spit that out too.
		if (sizeleft > 0) {
			mcSync(0, NULL, &ret);
			genWrite(out_fd, &ipconfigfile[i], sizeleft);
			mcSync(0, NULL, &ret);
		}

			snprintf(Message, MAX_PATH, "%s %.*s", LNG(Saved), MAX_PATH - 4, path);

		genClose(out_fd);
	}
}
//---------------------------------------------------------------------------
// Convert IP string to numbers
//---------------------------------------------------------------------------
static void ipStringToOctet(char *ip, int ip_octet[4])
{

	// This takes a string (ip) representing an IP address and converts it
	// into an array of ints (ip_octet)
	// Rewritten 22/10/05

	char oct_str[5];
	int oct_cnt, i, oct_len;

	oct_cnt = 0;
	oct_len = 0;
	oct_str[0] = '\0';

	for (i = 0; ((i <= strlen(ip)) && (oct_cnt < 4)); i++) {
		if ((ip[i] == '.') || (i == strlen(ip))) {
			ip_octet[oct_cnt] = atoi(oct_str);
			oct_cnt++;
			oct_len = 0;
			oct_str[0] = '\0';
		} else if (oct_len < (int)sizeof(oct_str) - 1) {
			oct_str[oct_len++] = ip[i];
			oct_str[oct_len] = '\0';
		}
	}
}
//---------------------------------------------------------------------------
static data_ip_struct BuildOctets(char *ip, char *nm, char *gw)
{

	// Populate 3 arrays with the ip address (as ints)

	data_ip_struct iplist;

	ipStringToOctet(ip, iplist.ip);
	ipStringToOctet(nm, iplist.nm);
	ipStringToOctet(gw, iplist.gw);

	return (iplist);
}
//---------------------------------------------------------------------------
enum CONFIG_NET {
	CONFIG_NET_FIRST = 1,
	CONFIG_NET_IP = CONFIG_NET_FIRST,
	CONFIG_NET_NM,
	CONFIG_NET_GW,

	//Settings after IP addresses
	CONFIG_NET_AFT_IP,
	CONFIG_NET_SAVE = CONFIG_NET_AFT_IP,
	CONFIG_NET_RETURN,

	CONFIG_NET_COUNT
};

static void Config_Network(void)
{
	// Menu System for Network Settings Page.

	int s, l;
	int x, y;
	int event, post_event = 0;
	int len;
	char c[MAX_PATH];
	data_ip_struct ipdata;
	char NetMsg[MAX_PATH] = "";
	char path[MAX_PATH];

	event = 1;  //event = initial entry
	s = CONFIG_NET_FIRST;
	l = 1;
	ipdata = BuildOctets(ip, netmask, gw);

	while (1) {
		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP) {
				event |= 2;  //event |= valid pad command
				if (s != CONFIG_NET_FIRST)
					s--;
				else {
					s = CONFIG_NET_RETURN;
					l = 1;
				}
			} else if (new_pad & PAD_DOWN) {
				event |= 2;  //event |= valid pad command
				if (s != CONFIG_NET_COUNT - 1)
					s++;
				else
					s = CONFIG_NET_FIRST;
				if (s >= CONFIG_NET_AFT_IP)
					l = 1;
			} else if (new_pad & PAD_LEFT) {
				event |= 2;  //event |= valid pad command
				if (s < CONFIG_NET_AFT_IP)
					if (l > 1)
						l--;
			} else if (new_pad & PAD_RIGHT) {
				event |= 2;  //event |= valid pad command
				if (s < CONFIG_NET_AFT_IP)
					if (l < 5)
						l++;
			} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				event |= 2;  //event |= valid pad command
				if ((s < CONFIG_NET_AFT_IP) && (l > 1)) {
					if (s == CONFIG_NET_IP) {
						if (ipdata.ip[l - 2] > 0) {
							ipdata.ip[l - 2]--;
						}
					} else if (s == CONFIG_NET_NM) {
						if (ipdata.nm[l - 2] > 0) {
							ipdata.nm[l - 2]--;
						}
					} else if (s == CONFIG_NET_GW) {
						if (ipdata.gw[l - 2] > 0) {
							ipdata.gw[l - 2]--;
						}
					}
				}
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				event |= 2;  //event |= valid pad command
				if ((s < CONFIG_NET_AFT_IP) && (l > 1)) {
					if (s == CONFIG_NET_IP) {
						if (ipdata.ip[l - 2] < 255) {
							ipdata.ip[l - 2]++;
						}
					} else if (s == CONFIG_NET_NM) {
						if (ipdata.nm[l - 2] < 255) {
							ipdata.nm[l - 2]++;
						}
					} else if (s == CONFIG_NET_GW) {
						if (ipdata.gw[l - 2] < 255) {
							ipdata.gw[l - 2]++;
						}
					}

				}

				else if (s == CONFIG_NET_SAVE) {
					sprintf(ip, "%i.%i.%i.%i", ipdata.ip[0], ipdata.ip[1], ipdata.ip[2], ipdata.ip[3]);
					sprintf(netmask, "%i.%i.%i.%i", ipdata.nm[0], ipdata.nm[1], ipdata.nm[2], ipdata.nm[3]);
					sprintf(gw, "%i.%i.%i.%i", ipdata.gw[0], ipdata.gw[1], ipdata.gw[2], ipdata.gw[3]);

					saveNetworkSettings(NetMsg);
				} else  //s == CONFIG_NET_RETURN
					return;
			} else if (new_pad & PAD_TRIANGLE)
				return;
		}

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[COLOR_BACKGR]);

			x = Menu_start_x;
			y = Menu_start_y;

			printXY(LNG(NETWORK_SETTINGS), x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;

			len = (strlen(LNG(IP_Address)) + 5 > strlen(LNG(Netmask)) + 5) ?
			          strlen(LNG(IP_Address)) + 5 :
			          strlen(LNG(Netmask)) + 5;
			len = (len > strlen(LNG(Gateway)) + 5) ? len : strlen(LNG(Gateway)) + 5;
			sprintf(c, "%s:", LNG(IP_Address));
			printXY(c, x + 2 * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
			sprintf(c, "%.3i . %.3i . %.3i . %.3i", ipdata.ip[0], ipdata.ip[1], ipdata.ip[2], ipdata.ip[3]);
			printXY(c, x + len * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			sprintf(c, "%s:", LNG(Netmask));
			printXY(c, x + 2 * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
			sprintf(c, "%.3i . %.3i . %.3i . %.3i", ipdata.nm[0], ipdata.nm[1], ipdata.nm[2], ipdata.nm[3]);
			printXY(c, x + len * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			sprintf(c, "%s:", LNG(Gateway));
			printXY(c, x + 2 * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
			sprintf(c, "%.3i . %.3i . %.3i . %.3i", ipdata.gw[0], ipdata.gw[1], ipdata.gw[2], ipdata.gw[3]);
			printXY(c, x + len * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;

			uLE_related(path, "uLE:/IPCONFIG.DAT");  //Get save target.
			{
				int path_len;
				int prefix_len = snprintf(c, sizeof(c), "  %s \"", LNG(Save_to));
				if (prefix_len < 0 || prefix_len >= (int)sizeof(c))
					c[sizeof(c) - 1] = '\0';
				else {
					path_len = (int)sizeof(c) - prefix_len - 2;  // room for '"' and '\0'
					if (path_len < 0)
						path_len = 0;
					snprintf(c + prefix_len, sizeof(c) - prefix_len, "%.*s\"", path_len, path);
				}
			}
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;
			sprintf(c, "  %s", LNG(RETURN));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			//Cursor positioning section
			y = Menu_start_y + s * FONT_HEIGHT + FONT_HEIGHT / 2;

			if (s >= CONFIG_NET_AFT_IP)
				y += FONT_HEIGHT / 2;
			if (s >= CONFIG_NET_RETURN)
				y += FONT_HEIGHT / 2;
			if (l > 1)
				x += (len - 1) * FONT_WIDTH - 1 + (l - 2) * 6 * FONT_WIDTH;
			drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);

			//Tooltip section
			if ((s < CONFIG_NET_AFT_IP) && (l == 1)) {
				len = sprintf(c, "%s", LNG(Right_DPad_to_Edit));
			} else if (s < CONFIG_NET_AFT_IP) {
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
			} else if (s == CONFIG_NET_SAVE) {
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s",
					              LNG(Save));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s",
					              LNG(Save));
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
			setScrTmp(NetMsg, c);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;

	}  //ends while
}  //ends Config_Network
//---------------------------------------------------------------------------
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

static void Config_Advanced(void)
{
	int s, max_s = CONFIG_ADVANCED_COUNT - 1;
	int x, y;
	int len;
	int event, post_event = 0;
	char c[MAX_PATH];
	int bool_label_width;
	const char *hostwrite_label;
	const char *app_gameid_label = "RetroGem Game ID";
	const char *cdrom_gameid_label = "Disable RetroGem Game ID for Discs";
	const char *psu_huge_label = "Create PSU with ID and Game Title";
	const char *psu_date_label = "Create PSU with Date";
	const char *psu_nooverwrite_label = "Create New PSU if filename exists";
	const char *pathpad_lock_label = "Lock Main LaunchKey/PathPad Paths";
	const char *fb_noicons_label = "Disable Icons in File Browser";

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
					else if (s == CONFIG_ADVANCED_PSU_DATENAMES)
						setting->PSU_DateNames = !setting->PSU_DateNames;
					else if (s == CONFIG_ADVANCED_PSU_NOOVERWRITE)
						setting->PSU_NoOverwrite = !setting->PSU_NoOverwrite;
					else if (s == CONFIG_ADVANCED_PATHPAD_LOCK)
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

			printXY("ADVANCED SETTINGS", x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			formatLabelValueAligned(c, sizeof(c), app_gameid_label, setting->app_gameid ? LNG(ON) : LNG(OFF), bool_label_width);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

				formatLabelValueAligned(c, sizeof(c), cdrom_gameid_label, setting->cdrom_disable_gameid ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				y += FONT_HEIGHT / 2;

				formatLabelValueAligned(c, sizeof(c), psu_huge_label, setting->PSU_HugeNames ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;

				formatLabelValueAligned(c, sizeof(c), psu_date_label, setting->PSU_DateNames ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;

				formatLabelValueAligned(c, sizeof(c), psu_nooverwrite_label, setting->PSU_NoOverwrite ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				y += FONT_HEIGHT / 2;

				formatLabelValueAligned(c, sizeof(c), pathpad_lock_label, setting->PathPad_Lock ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				y += FONT_HEIGHT / 2;

				formatLabelValueAligned(c, sizeof(c), fb_noicons_label, setting->FB_NoIcons ? LNG(ON) : LNG(OFF), bool_label_width);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
				y += FONT_HEIGHT / 2;

				formatLabelValueAligned(c, sizeof(c), hostwrite_label, setting->HOSTwrite ? LNG(ON) : LNG(OFF), bool_label_width);
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
// Configuration menu
//---------------------------------------------------------------------------
enum CONFIG_MAIN {
	CONFIG_MAIN_FIRST = 0,
	CONFIG_MAIN_DEFAULT = CONFIG_MAIN_FIRST,

	CONFIG_MAIN_BTN_CIRCLE,
	CONFIG_MAIN_BTN_CROSS,
	CONFIG_MAIN_BTN_SQUARE,
	CONFIG_MAIN_BTN_TRIANGLE,
	CONFIG_MAIN_BTN_L1,
	CONFIG_MAIN_BTN_R1,
	CONFIG_MAIN_BTN_L2,
	CONFIG_MAIN_BTN_R2,
	CONFIG_MAIN_BTN_L3,
	CONFIG_MAIN_BTN_R3,
	CONFIG_MAIN_BTN_START,

	//After button settings
	CONFIG_MAIN_AFT_BTNS,
	CONFIG_MAIN_SHOW_TITLES = CONFIG_MAIN_AFT_BTNS,
	CONFIG_MAIN_FILENAME,
	CONFIG_MAIN_SCREEN,
	CONFIG_MAIN_SETTINGS,
	CONFIG_MAIN_NETWORK,
	CONFIG_MAIN_ADVANCED,
	CONFIG_MAIN_LAST = CONFIG_MAIN_ADVANCED,

	CONFIG_MAIN_OK,
	CONFIG_MAIN_CANCEL,

	CONFIG_MAIN_COUNT
};

void config(char *mainMsg, char *CNF)
{
	char c[MAX_PATH];
	char title_tmp[MAX_ELF_TITLE];
	char *localMsg;
	int i;
	int s;
	int x, y;
	int len;
	int bool_label_width;
	int event, post_event = 0;

	tmpsetting = setting;
	setting = (SETTING *)malloc(sizeof(SETTING));
	*setting = *tmpsetting;

	event = 1;  //event = initial entry
	s = CONFIG_MAIN_FIRST;
	while (1) {
		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP) {
				event |= 2;  //event |= valid pad command
				if (s != CONFIG_MAIN_FIRST)
					s--;
				else
					s = CONFIG_MAIN_COUNT - 1;
			} else if (new_pad & PAD_DOWN) {
				event |= 2;  //event |= valid pad command
				if (s != CONFIG_MAIN_COUNT - 1)
					s++;
				else
					s = 0;
			} else if (new_pad & PAD_LEFT) {
				event |= 2;  //event |= valid pad command
				if (s > CONFIG_MAIN_LAST)
					s = CONFIG_MAIN_AFT_BTNS;
				else
					s = CONFIG_MAIN_FIRST;
				} else if (new_pad & PAD_RIGHT) {
					event |= 2;  //event |= valid pad command
					if (s < CONFIG_MAIN_AFT_BTNS)
						s = CONFIG_MAIN_AFT_BTNS;
					else if (s <= CONFIG_MAIN_LAST)
						s = CONFIG_MAIN_OK;
				} else if ((new_pad & PAD_SQUARE) && (s < CONFIG_MAIN_AFT_BTNS)) {
					event |= 2;  //event |= valid pad command
					strcpy(title_tmp, setting->LK_Title[s]);
					if (keyboard(title_tmp, MAX_ELF_TITLE) >= 0)
						strcpy(setting->LK_Title[s], title_tmp);
				} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
					event |= 2;  //event |= valid pad command
					if (s < CONFIG_MAIN_AFT_BTNS) {
						if (!setting->PathPad_Lock) {
							setting->LK_Path[s][0] = 0;
							setting->LK_Title[s][0] = 0;
						}
					}
				} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
					event |= 2;  //event |= valid pad command
					if (s < CONFIG_MAIN_AFT_BTNS) {
						if (!setting->PathPad_Lock) {
							getFilePath(setting->LK_Path[s], TRUE);
							if (!strncmp(setting->LK_Path[s], "mc0", 3) ||
							    !strncmp(setting->LK_Path[s], "mc1", 3)) {
								snprintf(c, sizeof(c), "mc%.*s", (int)sizeof(c) - 3, &setting->LK_Path[s][3]);
								strcpy(setting->LK_Path[s], c);
							}
						}
					} else if (s == CONFIG_MAIN_SHOW_TITLES)
						setting->Show_Titles = !setting->Show_Titles;
					else if (s == CONFIG_MAIN_FILENAME)
						setting->Hide_Paths = !setting->Hide_Paths;
					else if (s == CONFIG_MAIN_SCREEN)
						Config_Screen();
					else if (s == CONFIG_MAIN_SETTINGS)
						Config_Startup();
				else if (s == CONFIG_MAIN_NETWORK)
					Config_Network();
				else if (s == CONFIG_MAIN_ADVANCED)
					Config_Advanced();
				else if (s == CONFIG_MAIN_OK) {
					free(tmpsetting);
					saveConfig(mainMsg, CNF);
					break;
				} else if (s == CONFIG_MAIN_CANCEL)
					goto cancel_exit;
			} else if (new_pad & PAD_TRIANGLE) {
			cancel_exit:
				free(setting);
				setting = tmpsetting;
				updateScreenMode();
				Load_External_Language();
				loadFont(setting->font_file);
				mainMsg[0] = 0;
				break;
			}
		}  //end if(readpad())

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[COLOR_BACKGR]);

			if (s < CONFIG_MAIN_AFT_BTNS)
				localMsg = setting->LK_Title[s];
			else
				localMsg = "";

			x = Menu_start_x;
			y = Menu_start_y;
			printXY(LNG(Button_Settings), x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			for (i = CONFIG_MAIN_FIRST; i < CONFIG_MAIN_AFT_BTNS; i++) {
				switch (i) {
					case CONFIG_MAIN_DEFAULT:
						strcpy(c, "  Default: ");
						break;
					case CONFIG_MAIN_BTN_CIRCLE:
						strcpy(c, "  \xFF"
						          "0     : ");
						break;
					case CONFIG_MAIN_BTN_CROSS:
						strcpy(c, "  \xFF"
						          "1     : ");
						break;
					case CONFIG_MAIN_BTN_SQUARE:
						strcpy(c, "  \xFF"
						          "2     : ");
						break;
					case CONFIG_MAIN_BTN_TRIANGLE:
						strcpy(c, "  \xFF"
						          "3     : ");
						break;
					case CONFIG_MAIN_BTN_L1:
						strcpy(c, "  L1     : ");
						break;
					case CONFIG_MAIN_BTN_R1:
						strcpy(c, "  R1     : ");
						break;
					case CONFIG_MAIN_BTN_L2:
						strcpy(c, "  L2     : ");
						break;
					case CONFIG_MAIN_BTN_R2:
						strcpy(c, "  R2     : ");
						break;
					case CONFIG_MAIN_BTN_L3:
						strcpy(c, "  L3     : ");
						break;
					case CONFIG_MAIN_BTN_R3:
						strcpy(c, "  R3     : ");
						break;
					case CONFIG_MAIN_BTN_START:
						strcpy(c, "  START  : ");
						break;
				}
				strcat(c, setting->LK_Path[i]);
				printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
				y += FONT_HEIGHT;
			}

			y += FONT_HEIGHT / 2;

			bool_label_width = (int)strlen(LNG(Show_launch_titles));
			if ((int)strlen(LNG(Hide_full_ELF_paths)) > bool_label_width)
				bool_label_width = (int)strlen(LNG(Hide_full_ELF_paths));

			formatLabelValueAligned(c, sizeof(c), LNG(Show_launch_titles), (setting->Show_Titles)? LNG(ON) : LNG(OFF), bool_label_width);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			formatLabelValueAligned(c, sizeof(c), LNG(Hide_full_ELF_paths), (setting->Hide_Paths)?LNG(ON): LNG(OFF), bool_label_width);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			sprintf(c, "  %s...", LNG(Screen_Settings));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			sprintf(c, "  %s...", LNG(Startup_Settings));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			sprintf(c, "  %s...", LNG(Network_Settings));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			sprintf(c, "  Advanced Settings...");
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;

			sprintf(c, "  %s", LNG(OK));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			sprintf(c, "  %s", LNG(Cancel));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);

			//Cursor positioning section
			y = Menu_start_y + (s + 1) * FONT_HEIGHT;
			if (s >= CONFIG_MAIN_AFT_BTNS)
				y += FONT_HEIGHT / 2;
			if (s >= CONFIG_MAIN_OK)
				y += FONT_HEIGHT;
			drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);

				//Tooltip section
				if (s < CONFIG_MAIN_AFT_BTNS) {
					if (setting->PathPad_Lock) {
						len = sprintf(c, "\xFF"
						                 "2:%s",
						              LNG(Edit_Title));
					} else if (swapKeys) {
						len = sprintf(c, "\xFF"
						                 "1:%s \xFF"
						                 "0:%s \xFF"
						                 "2:%s",
						              LNG(Browse), LNG(Clear), LNG(Edit_Title));
					} else {
						len = sprintf(c, "\xFF"
						                 "0:%s \xFF"
						                 "1:%s \xFF"
						                 "2:%s",
						              LNG(Browse), LNG(Clear), LNG(Edit_Title));
					}
				} else if ((s == CONFIG_MAIN_SHOW_TITLES) || (s == CONFIG_MAIN_FILENAME)) {
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
			setScrTmp(localMsg, c);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;

	}  //ends while
}  //ends config
//---------------------------------------------------------------------------
// End of file: config.c
//---------------------------------------------------------------------------
