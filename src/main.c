//---------------------------------------------------------------------------
//File name:   main.c
//---------------------------------------------------------------------------
#include "launchelf.h"
#include "init.h"
#include "main_startup.h"

//#define DEBUG
#ifdef DEBUG
#define dbgprintf(args...) scr_printf(args)
#define dbginit_scr() init_scr()
#else
#define dbgprintf(args...) \
	do {                   \
	} while (0)
#define dbginit_scr() \
	do {              \
	} while (0)
#endif

enum {
	BUTTON,
	DPAD
};

int TV_mode;
int selected = 0;
int timeout = 0, prev_timeout = 1;
int init_delay = 0, prev_init_delay = 1;
int mode = BUTTON;
int user_acted = 0; /* Set when commands given, to break timeout */
char LaunchElfDir[MAX_PATH], mainMsg[MAX_PATH];
char CNF[MAX_NAME];
int swapKeys;

u64 WaitTime;
u64 CurrTime;
u64 init_delay_start;
u64 timeout_start;

char if_conf[IPCONF_MAX_LEN];
int if_conf_len;

char ip[16] = "192.168.0.10";
char netmask[16] = "255.255.255.0";
char gw[16] = "192.168.0.1";

char netConfig[IPCONF_MAX_LEN + 64];  //Adjust size as needed

#ifdef MX4SIO
u8 mx4sio_driver_running = 0;
#endif

u8 console_is_PSX = 0;

static int menu_LK[SETTING_LK_BTN_COUNT];  //holds RunElf index for each valid main menu entry

static int boot_argc;
static char *boot_argv[8];
static char boot_path[MAX_PATH];

//Variables for SYSTEM.CNF processing
int BootDiscType = 0;
char SystemCnf_BOOT[MAX_PATH];
char SystemCnf_BOOT2[MAX_PATH];
char SystemCnf_VER[10];    //Arbitrary. Real value should always be shorter
char SystemCnf_VMODE[10];  //Arbitrary, same deal. As yet unused

char default_ESR_path[] = "mc:/BOOT/ESR.ELF";
char default_OSDSYS_path[30];
char default_OSDSYS_path2[30];

char ROMVER_data[16];  //16 byte file read from rom0:ROMVER at init
char rough_region;     //E==Europe, A==US, I==Japan, H==Asia, C==China

int cdmode;      //Last detected disc type
int old_cdmode;  //used for disc change detection
int uLE_cdmode;  //used for smart disc detection

typedef struct
{
	int type;
	char name[16];
} DiscType;

DiscType DiscTypes[] = {
    {SCECdNODISC, "!"},
    {SCECdDETCT, "??"},
    {SCECdDETCTCD, "CD ?"},
    {SCECdDETCTDVDS, "DVD-SL ?"},
    {SCECdDETCTDVDD, "DVD-DL ?"},
    {SCECdUNKNOWN, "Unknown"},
    {SCECdPSCD, "PS1 CD"},
    {SCECdPSCDDA, "PS1 CDDA"},
    {SCECdPS2CD, "PS2 CD"},
    {SCECdPS2CDDA, "PS2 CDDA"},
    {SCECdPS2DVD, "PS2 DVD"},
    {SCECdESRDVD_0, "ESR DVD (off)"},
    {SCECdESRDVD_1, "ESR DVD (on)"},
    {SCECdCDDA, "Audio CD"},
    {SCECdDVDV, "Video DVD"},
    {SCECdIllegalMedia, "Unsupported"},
    {0x00, ""}  //end of list
};              //ends DiscTypes array

//Static function declarations
static int PrintRow(int row_f, char *text_p);
static int PrintPos(int row_f, int column, char *text_p, int COLORID);
static void Show_About_uLE(void);
static void setLaunchKeys(void);
static int drawMainScreen(void);
static void ShowDebugInfo(void);
static void ShowFont(void);
static void Validate_CNF_Path(void);
static void displayRetroGemGameID(const char *gameID, int frames);
static int buildLaunchGameID(const char *exec_path, char *gameID, size_t gameID_len);
static int isLikelyDiscLaunch(const char *selected_path);
static void CleanUp(void);
static void Execute(char *pathin);
//---------------------------------------------------------------------------
//executable code
//---------------------------------------------------------------------------
//Function to print a text row to the 'gs' screen
//------------------------------
static int PrintRow(int row_f, char *text_p)
{
	static int row;
	int x = (Menu_start_x + 4);
	int y;

	if (row_f >= 0)
		row = row_f;
	y = (Menu_start_y + FONT_HEIGHT * row++);
	printXY(text_p, x, y, setting->color[COLOR_TEXT], TRUE, 0);
	return row;
}
//------------------------------
//endfunc PrintRow
//---------------------------------------------------------------------------
//Function to print a text row with text positioning
//------------------------------
static int PrintPos(int row_f, int column, char *text_p, int COLORID)
{
	static int row;
	int x = (Menu_start_x + 4 + column * FONT_WIDTH);
	int y;

	if (row_f >= 0)
		row = row_f;
	y = (Menu_start_y + FONT_HEIGHT * row++);
	printXY(text_p, x, y, setting->color[COLORID], TRUE, 0);
	return row;
}
//------------------------------
//endfunc PrintPos
//---------------------------------------------------------------------------
static u8 calculateRetroGemCRC(const u8 *data, int len)
{
	int i;
	u8 crc = 0x00;

	for (i = 0; i < len; i++)
		crc = (u8)(crc + data[i]);

	return (u8)(0x100 - crc);
}

static void displayRetroGemGameID(const char *gameID, int frames)
{
	GSGLOBAL *gid_gs;
	u8 data[64];
	int gidlen, dpos, data_len, xstart, ystart, height;
	int i, j, frame;

	if (gameID == NULL || gameID[0] == '\0')
		return;

	gidlen = strnlen(gameID, 11);
	if (gidlen <= 0)
		return;

	if (frames < 1)
		frames = 1;
	else if (frames > 3)
		frames = 3;

	gid_gs = gsKit_init_global();
	if (gid_gs == NULL)
		return;

	gid_gs->DoubleBuffering = GS_SETTING_ON;
	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	if (dmaKit_chan_init(DMA_CHANNEL_GIF) != 0) {
		gsKit_deinit_global(gid_gs);
		return;
	}

	gsKit_init_screen(gid_gs);
	gsKit_display_buffer(gid_gs);
	gsKit_mode_switch(gid_gs, GS_ONESHOT);

	memset(data, 0, sizeof(data));
	dpos = 0;
	data[dpos++] = 0xA5;
	data[dpos++] = 0x00;
	dpos++; // CRC placeholder
	data[dpos++] = (u8)gidlen;
	memcpy(&data[dpos], gameID, gidlen);
	dpos += gidlen;
	data[dpos++] = 0x00;
	data[dpos++] = 0xD5;
	data[dpos++] = 0x00;

	data_len = dpos;
	data[2] = calculateRetroGemCRC(&data[3], data_len - 3);

	xstart = (gid_gs->Width / 2) - (data_len * 8);
	ystart = gid_gs->Height - (((gid_gs->Height / 8) * 2) + 20);
	height = 2;

	for (frame = 0; frame < frames; frame++) {
		gsKit_clear(gid_gs, GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x00));

		for (i = 0; i < data_len; i++) {
			for (j = 7; j >= 0; j--) {
				int x = xstart + (i * 16 + ((7 - j) * 2));
				int x1 = x + 1;
				u64 bit_color;

				gsKit_prim_sprite(gid_gs, x, ystart, x1, ystart + height, 0, GS_SETREG_RGBA(0xFF, 0x00, 0xFF, 0x00));
				bit_color = ((data[i] >> j) & 1) ? GS_SETREG_RGBA(0x00, 0xFF, 0xFF, 0x00) : GS_SETREG_RGBA(0xFF, 0xFF, 0x00, 0x00);
				gsKit_prim_sprite(gid_gs, x1, ystart, x1 + 1, ystart + height, 0, bit_color);
			}
		}

		gsKit_queue_exec(gid_gs);
		gsKit_finish();
		gsKit_sync_flip(gid_gs);
	}

	gsKit_deinit_global(gid_gs);
}

static int buildLaunchGameID(const char *exec_path, char *gameID, size_t gameID_len)
{
	const char *start;
	const char *sep;
	size_t i;

	if (gameID == NULL || gameID_len < 12 || exec_path == NULL || exec_path[0] == '\0')
		return 0;

	gameID[0] = '\0';

	/* Disc executables (e.g. cdrom0:\\SLUS_123.45;1) already carry a title ID. */
	if (!strncmp(exec_path, "cdrom", 5)) {
		start = strchr(exec_path, '\\');
		if (start == NULL)
			start = strchr(exec_path, ':');
		if (start != NULL)
			start++;
	} else {
		start = exec_path;
		sep = strrchr(exec_path, '/');
		if (sep != NULL)
			start = sep + 1;
		sep = strrchr(exec_path, '\\');
		if (sep != NULL && (sep + 1) > start)
			start = sep + 1;
		sep = strrchr(exec_path, ':');
		if (sep != NULL && (sep + 1) > start)
			start = sep + 1;
	}

	if (start == NULL || *start == '\0')
		return 0;

	for (i = 0; i < 11 && start[i] != '\0'; i++) {
		char c = start[i];
		if (c == ';' || c == '/' || c == '\\')
			break;
		if (c == '.' && strncmp(exec_path, "cdrom", 5))
			break;
		gameID[i] = c;
	}
	gameID[i] = '\0';

	while (i > 0) {
		char c = gameID[i - 1];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
			gameID[--i] = '\0';
		else
			break;
	}

	if (!strcmp(gameID, "???"))
		return 0;

	return gameID[0] != '\0';
}

static int isLikelyDiscLaunch(const char *selected_path)
{
	if (selected_path == NULL || selected_path[0] == '\0')
		return 0;
	if (!stricmp(selected_path, setting->Misc_PS2Disc))
		return 1;
	return 0;
}

//---------------------------------------------------------------------------
//Function to show a screen with program credits ("About uLE")
//------------------------------
static void Show_About_uLE(void)
{
	char TextRow[256];
	int event, post_event = 0;
	int hpos = 16;

	event = 1;  //event = initial entry
	//----- Start of event loop -----
	while (1) {
		//Pad response section
		waitAnyPadReady();
		if (readpad() && new_pad) {
			event |= 2;
			break;
		}

		//Display section
		if (event || post_event) {  //NB: We need to update two frame buffers per event
			clrScr(setting->color[COLOR_BACKGR]);
			sprintf(TextRow, "About wLaunchELF %s  %s", ULE_VERSION, ULE_VERDATE);
			PrintPos(03, hpos, TextRow, COLOR_SELECT);
			sprintf(TextRow, " commit: %s (based on commit 41e4ebe)", GIT_HASH);
			PrintPos(04, hpos, TextRow, COLOR_TEXT);
			PrintPos(05, hpos, "Mod created by: Matias Israelson", COLOR_TEXT);
			PrintPos(-1, hpos, "DS3/DS4 support by Alex Parrado", COLOR_TEXT);
			PrintPos(-1, hpos, "Project maintainers:  sp193 & AKuHAK", COLOR_TEXT);
			PrintPos(-1, hpos, "  ", COLOR_TEXT);
			PrintPos(-1, hpos, "uLaunchELF Project maintainers:", COLOR_TEXT);
			PrintPos(-1, hpos, "  Eric Price       (aka: 'E P')", COLOR_TEXT);
			PrintPos(-1, hpos, "  Ronald Andersson (aka: 'dlanor')", COLOR_TEXT);
			PrintPos(-1, hpos, " ", COLOR_TEXT);
			PrintPos(-1, hpos, "Other contributors:", COLOR_TEXT);
			PrintPos(-1, hpos, "  Polo35, radad, Drakonite, sincro", COLOR_TEXT);
			PrintPos(-1, hpos, "  kthu, Slam-Tilt, chip, pixel, Hermes", COLOR_TEXT);
			PrintPos(-1, hpos, "  and others in the PS2Dev community", COLOR_TEXT);
			PrintPos(-1, hpos, " ", COLOR_TEXT);
			PrintPos(-1, hpos, "Main release site:", COLOR_TEXT);
			PrintPos(-1, hpos, "   github.com/ps2homebrew/wLaunchELF/releases", COLOR_TEXT);
			PrintPos(-1, hpos, "Mod Release site:", COLOR_SELECT);
			PrintPos(-1, hpos, "   github.com/israpps/wLaunchELF_ISR/releases", COLOR_TEXT);
			PrintPos(-1, hpos, "Ancestral project: LaunchELF v3.41 by Mirakichi", COLOR_TEXT);
			//PrintPos(-1, hpos, "Created by:        Mirakichi");
		}  //ends if(event||post_event)
		drawScr();// https://github.com/israpps/wLaunchELF_ISR/tree/41e43b3-mod
		post_event = event;
		event = 0;
	}  //ends while
	   //----- End of event loop -----
}

static void Show_build_info(void)
{
	char TextRow[256];
	int event, post_event = 0;
	int hpos = 16;

	event = 1;  //event = initial entry
	//----- Start of event loop -----
	while (1) {
		//Pad response section
		waitAnyPadReady();
		if (readpad() && new_pad) {
			event |= 2;
			break;
		}

		//Display section
		if (event || post_event) {  //NB: We need to update two frame buffers per event
			clrScr(setting->color[COLOR_BACKGR]);
			sprintf(TextRow, " wLaunchELF %s (%s)", ULE_VERSION, GIT_HASH);
			PrintPos(03, hpos, TextRow, COLOR_TEXT);
			PrintPos(-1, hpos, "Build features:", COLOR_SELECT);
			
			PrintPos(-1, hpos, 
#ifdef ETH
" ETH:1"
#else
" ETH:0"
#endif	
, COLOR_TEXT);
			PrintPos(-1, hpos, 
#ifdef XFROM
" XFROM=1"
#else
" XFROM=0"
#endif
#ifdef DVRP
" DVRP_HDD=1"
#else
" DVRP_HDD=0"
#endif
, COLOR_TEXT);

			PrintPos(-1, hpos, 
#ifdef EXFAT
" EXFAT=1"
#else
" EXFAT=0"
#endif
#ifdef DS34
" DS34=1"
#else
" DS34=0"
#endif
, COLOR_TEXT);
			PrintPos(-1, hpos, 
#ifdef MX4SIO
" MX4SIO=1"
#else
" MX4SIO=0"
#endif
#ifdef MMCE
" MMCE=1"
#else
" MMCE=0"
#endif
, COLOR_TEXT);
#if defined(UDPTTY) || defined(SIO_DEBUG) || defined(POWERPC_UART) || defined(NO_IOP_RESET)


			PrintPos(-1, hpos, "Debug Features:", COLOR_SELECT);
			PrintPos(-1, hpos, 
#ifdef NO_IOP_RESET
" IOP_RESET=0"
#else
" IOP_RESET=1"
#endif
#ifdef UDPTTY
" UDPTTY=1"
#else
" UDPTTY=0"
#endif
, COLOR_TEXT);
			PrintPos(-1, hpos, 
#ifdef SIO_DEBUG
" SIO_DEBUG=1"
#else
" SIO_DEBUG=0"
#endif
#ifdef POWERPC_UART
" PPC_UART=1"
#else
" PPC_UART=0"
#endif
, COLOR_TEXT);
#endif
			PrintPos(-1, hpos, "Mod Release site:", COLOR_TEXT);
			PrintPos(-1, hpos, "   github.com/israpps/wLaunchELF_ISR/releases", COLOR_TEXT);


		}  //ends if(event||post_event)
		drawScr();// https://github.com/israpps/wLaunchELF_ISR/tree/41e43b3-mod
		post_event = event;
		event = 0;
	}  //ends while
	   //----- End of event loop -----
}
//------------------------------
//endfunc Show_About_uLE
//---------------------------------------------------------------------------
static void setLaunchKeys(void)
{
	if (!setting->LK_Flag[SETTING_LK_SELECT])
		strcpy(setting->LK_Path[SETTING_LK_SELECT], setting->Misc_Configure);
}
//------------------------------
//endfunc setLaunchKeys()
//---------------------------------------------------------------------------
static int drawMainScreen(void)
{
	int nElfs = 0;
	int i;
	int x, y;
	u64 color;
	char *p;
	char c[MAX_PATH + 8], f[MAX_PATH];

	setLaunchKeys();

	clrScr(setting->color[COLOR_BACKGR]);

	x = Menu_start_x;
	y = Menu_start_y;
	c[0] = 0;
	if (init_delay)
		sprintf(c, "%s: %d", LNG(Init_Delay), init_delay / 1000);
	else if (setting->LK_Path[SETTING_LK_AUTO][0]) {
		if (!user_acted)
			sprintf(c, "%s: %d", LNG(TIMEOUT), timeout / 1000);
		else
			sprintf(c, "%s: %s", LNG(TIMEOUT), LNG(Halted));
	}
	if (c[0]) {
		printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
		y += FONT_HEIGHT * 2;
	}
	for (i = 0; i < SETTING_LK_BTN_COUNT; i++) {
		if ((setting->LK_Path[i][0]) && ((i <= SETTING_LK_SELECT) || setting->LK_Flag[i])) {
			menu_LK[nElfs] = i;  //memorize RunElf index for this menu entry
			switch (i) {
				case SETTING_LK_AUTO:
					strcpy(c, "Default: ");
					break;
				case SETTING_LK_CIRCLE:
					strcpy(c, "     \xFF"
					          "0: ");
					break;
				case SETTING_LK_CROSS:
					strcpy(c, "     \xFF"
					          "1: ");
					break;
				case SETTING_LK_SQUARE:
					strcpy(c, "     \xFF"
					          "2: ");
					break;
				case SETTING_LK_TRIANGLE:
					strcpy(c, "     \xFF"
					          "3: ");
					break;
				case SETTING_LK_L1:
					strcpy(c, "     L1: ");
					break;
				case SETTING_LK_R1:
					strcpy(c, "     R1: ");
					break;
				case SETTING_LK_L2:
					strcpy(c, "     L2: ");
					break;
				case SETTING_LK_R2:
					strcpy(c, "     R2: ");
					break;
				case SETTING_LK_L3:
					strcpy(c, "     L3: ");
					break;
				case SETTING_LK_R3:
					strcpy(c, "     R3: ");
					break;
				case SETTING_LK_START:
					strcpy(c, "  START: ");
					break;
				case SETTING_LK_SELECT:
					strcpy(c, " SELECT: ");
					break;
				case SETTING_LK_LEFT:
					sprintf(c, "%s: ", LNG(LEFT));
					break;
				case SETTING_LK_RIGHT:
					sprintf(c, "%s: ", LNG(RIGHT));
					break;
			}                          //ends switch
			if (setting->Show_Titles)  //Show Launch Titles ?
				strcpy(f, setting->LK_Title[i]);
			else
				f[0] = '\0';
			if (!f[0]) {                                          //No title present, or allowed ?
				if (setting->Hide_Paths) {                        //Hide full path ?
					if ((p = strrchr(setting->LK_Path[i], '/')))  // found delimiter ?
						strcpy(f, p + 1);
					else  // No delimiter !
						strcpy(f, setting->LK_Path[i]);
					if ((p = strrchr(f, '.')))
						*p = 0;
				} else {  //Show full path !
					strcpy(f, setting->LK_Path[i]);
				}
			}  //ends clause for No title
			if (nElfs++ == selected && mode == DPAD)
				color = setting->color[COLOR_SELECT];
			else
				color = setting->color[COLOR_TEXT];
			int len = (strlen(LNG(LEFT)) + 2 > strlen(LNG(RIGHT)) + 2) ?
			              strlen(LNG(LEFT)) + 2 :
			              strlen(LNG(RIGHT)) + 2;
			if (i == SETTING_LK_LEFT) {  // LEFT
				if (strlen(LNG(RIGHT)) + 2 > strlen(LNG(LEFT)) + 2)
					printXY(c, x + (strlen(LNG(RIGHT)) + 2 > 9 ? ((strlen(LNG(RIGHT)) + 2) - (strlen(LNG(LEFT)) + 2)) * FONT_WIDTH : (9 - (strlen(LNG(LEFT)) + 2)) * FONT_WIDTH),
					        y, color, TRUE, 0);
				else
					printXY(c, x + (strlen(LNG(LEFT)) + 2 > 9 ? 0 : (9 - (strlen(LNG(LEFT)) + 2)) * FONT_WIDTH),
					        y, color, TRUE, 0);
			} else if (i == SETTING_LK_RIGHT) {  // RIGHT
				if (strlen(LNG(LEFT)) + 2 > strlen(LNG(RIGHT)) + 2)
					printXY(c, x + (strlen(LNG(LEFT)) + 2 > 9 ? ((strlen(LNG(LEFT)) + 2) - (strlen(LNG(RIGHT)) + 2)) * FONT_WIDTH : (9 - (strlen(LNG(RIGHT)) + 2)) * FONT_WIDTH),
					        y, color, TRUE, 0);
				else
					printXY(c, x + (strlen(LNG(RIGHT)) + 2 > 9 ? 0 : (9 - (strlen(LNG(RIGHT)) + 2)) * FONT_WIDTH),
					        y, color, TRUE, 0);
			} else
				printXY(c, x + (len > 9 ? (len - 9) * FONT_WIDTH : 0), y, color, TRUE, 0);
			printXY(f, x + (len > 9 ? len * FONT_WIDTH : 9 * FONT_WIDTH), y, color, TRUE, 0);
			y += FONT_HEIGHT;
		}  //ends clause for defined LK_Path[i] valid for menu
	}      //ends for

	if (mode == BUTTON)
		sprintf(c, "%s!", LNG(PUSH_ANY_BUTTON_or_DPAD));
	else {
		if (swapKeys)
			sprintf(c, "\xFF"
			           "1:%s \xFF"
			           "0:%s",
			        LNG(OK), LNG(Cancel));
		else
			sprintf(c, "\xFF"
			           "0:%s \xFF"
			           "1:%s",
			        LNG(OK), LNG(Cancel));
	}

	setScrTmp(mainMsg, c);

	return nElfs;
}
//------------------------------
//endfunc drawMainScreen
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//Function to show a screen with debugging info
//------------------------------
static void ShowDebugInfo(void)
{
	char TextRow[256];
	unsigned short romVersion;
	int i, event, post_event = 0;
	event = 1;  //event = initial entry
	//----- Start of event loop -----
	while (1) {
		//Pad response section
		waitAnyPadReady();
		if (readpad() && new_pad) {
			event |= 2;
			break;
		}

		//Display section
		if (event || post_event) {  //NB: We need to update two frame buffers per event
			clrScr(setting->color[COLOR_BACKGR]);
			PrintRow(0, "Debug Info Screen:");
			if (ROMVER_data[0] == '\0')
				uLE_InitializeRegion();
			if (ROMVER_data[0] == '\0')
				snprintf(TextRow, sizeof(TextRow), "rom0:ROMVER == \"<unavailable>\"");
			else
				snprintf(TextRow, sizeof(TextRow), "rom0:ROMVER == \"%s\"", ROMVER_data);
			PrintRow(2, TextRow);
			sprintf(TextRow, "argc == %d", boot_argc);
			PrintRow(4, TextRow);
			for (i = 0; (i < boot_argc) && (i < 8); i++) {
				sprintf(TextRow, "argv[%d] == \"%s\"", i, boot_argv[i]);
				PrintRow(-1, TextRow);
			}
			sprintf(TextRow,     "Main System Update KELF == \"%s\"", (console_is_PSX) ? "BIEXEC-SYSTEM/xosdmain.elf" : (strchr(default_OSDSYS_path2,'/')+ 1));
			PrintRow(-1, TextRow);
			romVersion = getROMVersion();
			if ((romVersion < 0x230) && (romVersion > 0x130))
			{
				sprintf(TextRow, "Specific System Update KELF == \"B%cEXEC-SYSTEM/osd%03x.elf\"", rough_region, (romVersion+10)&~0x0F);
				PrintRow(-1, TextRow);
			}
			{
				int max_path = (int)sizeof(TextRow) - (int)strlen("boot_path == \"\"") - 1;
				if (max_path < 0)
					max_path = 0;
				snprintf(TextRow, sizeof(TextRow), "boot_path == \"%.*s\"", max_path, boot_path);
			}
			PrintRow(-1, TextRow);
			{
				int max_path = (int)sizeof(TextRow) - (int)strlen("LaunchElfDir == \"\"") - 1;
				if (max_path < 0)
					max_path = 0;
				snprintf(TextRow, sizeof(TextRow), "LaunchElfDir == \"%.*s\"", max_path, LaunchElfDir);
			}
			PrintRow(-1, TextRow);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}  //ends while
	   //----- End of event loop -----
}
//------------------------------
//endfunc ShowDebugInfo
//---------------------------------------------------------------------------
static void ShowFont(void)
{
	int test_type = 0;
	int test_types = 2;  //Patch test_types for number of test loops
	int i, j, event, post_event = 0;
	char Hex[18] = "0123456789ABCDEF";
	int ch_x_stp = 1 + FONT_WIDTH + 1 + LINE_THICKNESS;
	int ch_y_stp = 2 + FONT_HEIGHT + 1 + LINE_THICKNESS;
	int mat_w = LINE_THICKNESS + 17 * ch_x_stp;
	int mat_h = LINE_THICKNESS + 17 * ch_y_stp;
	int mat_x = (((SCREEN_WIDTH - mat_w) / 2) & -2);
	int mat_y = (((SCREEN_HEIGHT - mat_h) / 2) & -2);
	int ch_x = mat_x + LINE_THICKNESS + 1;
	//	int	ch_y  = mat_y+LINE_THICKNESS+2;
	int px, ly, cy;
	u64 col_0 = setting->color[COLOR_BACKGR], col_1 = setting->color[COLOR_FRAME], col_3 = setting->color[COLOR_TEXT];

	//The next line is a patch to save font, if/when needed (needs patch in draw.c too)
	//	WriteFont_C("mc0:/SYS-CONF/font_uLE.c");

	event = 1;  //event = initial entry
	//----- Start of event loop -----
	while (1) {
		//Display section
		if (event || post_event) {  //NB: We need to update two frame buffers per event
			drawOpSprite(col_0, mat_x, mat_y, mat_x + mat_w - 1, mat_y + mat_h - 1);
			//Here the background rectangle has been prepared
			/* //Start of commented out section //Move this line as needed for tests
			//Start of gsKit test section
			if(test_type > 1) goto done_test;
			gsKit_prim_point(gsGlobal, mat_x+16, mat_y+16, 1, col_3);
			gsKit_prim_point(gsGlobal, mat_x+33, mat_y+16, 1, col_3);
			gsKit_prim_point(gsGlobal, mat_x+33, mat_y+33, 1, col_3);
			gsKit_prim_point(gsGlobal, mat_x+16, mat_y+33, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+48, mat_y+48, mat_x+65, mat_y+48, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+65, mat_y+48, mat_x+65, mat_y+65, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+65, mat_y+65, mat_x+48, mat_y+65, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+48, mat_y+65, mat_x+48, mat_y+48, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+80, mat_y+80, mat_x+97, mat_y+81, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+97, mat_y+80, mat_x+96, mat_y+97, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+97, mat_y+97, mat_x+80, mat_y+96, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+80, mat_y+97, mat_x+81, mat_y+80, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+80, mat_y+16, mat_x+81, mat_y+16, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+97, mat_y+16, mat_x+97, mat_y+17, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+97, mat_y+33, mat_x+96, mat_y+33, 1, col_3);
			gsKit_prim_line(gsGlobal, mat_x+80, mat_y+33, mat_x+80, mat_y+32, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+16, mat_y+80, mat_x+17, mat_y+81, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+33, mat_y+80, mat_x+32, mat_y+81, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+33, mat_y+97, mat_x+32, mat_y+96, 1, col_3);
			gsKit_prim_sprite(gsGlobal, mat_x+16, mat_y+97, mat_x+17, mat_y+96, 1, col_3);
			goto end_display;
done_test:
			//End of gsKit test section
*/  //End of commented out section  //Move this line as needed for tests
			//Start of font display section
			//Now we start to draw all vertical frame lines
			px = mat_x;
			drawOpSprite(col_1, px, mat_y, px + LINE_THICKNESS - 1, mat_y + mat_h - 1);
			for (j = 0; j < 17; j++) {  //for each font column, plus the row_index column
				px += ch_x_stp;
				drawOpSprite(col_1, px, mat_y, px + LINE_THICKNESS - 1, mat_y + mat_h - 1);
			}  //ends for each font column, plus the row_index column
			//Here all the vertical frame lines have been drawn
			//Next we draw the top horizontal line
			drawOpSprite(col_1, mat_x, mat_y, mat_x + mat_w - 1, mat_y + LINE_THICKNESS - 1);
			cy = mat_y + LINE_THICKNESS + 2;
			ly = mat_y;
			for (i = 0; i < 17; i++) {  //for each font row
				px = ch_x;
				if (!i) {                                 //if top row (which holds the column indexes)
					drawChar('\\', px, cy, col_3);        //Display '\' at index crosspoint
				} else {                                  //else a real font row
					drawChar(Hex[i - 1], px, cy, col_3);  //Display row index
				}
				for (j = 0; j < 16; j++) {  //for each font column
					px += ch_x_stp;
					if (!i) {                             //if top row (which holds the column indexes)
						drawChar(Hex[j], px, cy, col_3);  //Display Column index
					} else {
						drawChar((i - 1) * 16 + j, px, cy, col_3);  //Display font character
					}
				}  //ends for each font column
				ly += ch_y_stp;
				drawOpSprite(col_1, mat_x, ly, mat_x + mat_w - 1, ly + LINE_THICKNESS - 1);
				cy += ch_y_stp;
			}  //ends for each font row
			   //End of font display section
		}      //ends if(event||post_event)
		       //end_display:
		drawScr();
		post_event = event;
		event = 0;

		//Pad response section
		waitAnyPadReady();
		if (readpad() && new_pad) {
			event |= 2;
			if ((++test_type) < test_types) {
				mat_y++;
				continue;
			}
			break;
		}
	}  //ends while
	   //----- End of event loop -----
}
//------------------------------
//endfunc ShowFont
//---------------------------------------------------------------------------
static void Validate_CNF_Path(void)
{
	char cnf_path[MAX_PATH];

	if (setting->CNF_Path[0] != '\0') {
		if (genFixPath(setting->CNF_Path, cnf_path) >= 0)
			strcpy(LaunchElfDir, setting->CNF_Path);
	}
}
//------------------------------
//endfunc Validate_CNF_Path
//---------------------------------------------------------------------------
//CleanUp releases uLE stuff preparatory to launching some other application
//------------------------------
static void CleanUp(void)
{
	clrScr(GS_SETREG_RGBA(0x00, 0x00, 0x00, 0));
	drawScr();
	clrScr(GS_SETREG_RGBA(0x00, 0x00, 0x00, 0));
	drawScr();
	free(setting);
	free(elisaFnt);
	free(External_Lang_Buffer);
	padPortClose(1, 0);
	padPortClose(0, 0);
	//    padEnd();  //Required when a newer libpad library is used.
	closeKeyboardIfOpened();
#ifdef DS34
	WaitSema(semRunning);
	isRunning=0;
	SignalSema(semRunning);	
	WaitSema(semFinish);
	ds34usb_reset();
	ds34bt_reset();
#endif
}
//------------------------------
//endfunc CleanUp
//---------------------------------------------------------------------------
// Execute. Execute an action. May be called recursively.
// For any path specified, its device must be accessible.
//------------------------------
static void Execute(char *pathin)
{
	char tmp[MAX_PATH];
	static char path[MAX_PATH];
	static char fullpath[MAX_PATH];
	static char party[MAX_PATH];
	char *p;
	int x, t = 0;
	char dvdpl_path[] = "mc0:/BREXEC-DVDPLAYER/dvdplayer.elf";
	int dvdpl_update;

	if (pathin[0] == 0)
		return;

	if (!uLE_related(path, pathin))  //1==uLE_rel 0==missing, -1==other dev
		return;

Recurse_for_ESR:  //Recurse here for PS2Disc command with ESR disc

	if (!strncmp(path, "mc", 2)) {
		party[0] = 0;
		if (path[2] != ':')
			goto CheckELF_path;
		strcpy(fullpath, "mc0:");
		strcat(fullpath, path + 3);
		if ((t = checkELFheader(fullpath)) > 0)
			goto ELFchecked;
		fullpath[2] = '1';
		goto CheckELF_fullpath;

	} else if (!strncmp(path, "vmc", 3)) {
		x = path[3] - '0';
		if ((x < 0) || (x > 1) || !vmcMounted[x])
			goto ELFnotFound;
		goto CheckELF_path;
	} else if (!strncmp(path, "hdd0:/", 6)) {
		loadHddModules();
		if ((t = checkELFheader(path)) <= 0)
			goto ELFnotFound;
		//coming here means the ELF is fine
		snprintf(party, sizeof(party), "hdd0:%s", path + 6);
		p = strchr(party, '/');
		snprintf(fullpath, sizeof(fullpath), "pfs0:%s", p);
		*p = 0;
		goto ELFchecked;
		} else if (!strncmp(path, "dvr_hdd0:/", 10)) {
#ifdef DVRP
			if (!console_is_PSX)
				goto ELFnotFound;
			loadDVRPHddModules();
			if ((t = checkELFheader(path)) <= 0)
				goto ELFnotFound;
			//coming here means the ELF is fine
			snprintf(party, sizeof(party), "dvr_hdd0:%s", path + 10);
			p = strchr(party, '/');
			snprintf(fullpath, sizeof(fullpath), "dvr_pfs0:%s", p);
			*p = 0;
			goto ELFchecked;
#else
			goto ELFnotFound;
#endif
		} else if (!strncmp(path, "xfrom", 5)) {
#ifdef XFROM
			if (!console_is_PSX)
				goto ELFnotFound;
			loadFlashModules();
			if ((t = checkELFheader(path)) <= 0)
				goto ELFnotFound;
			strcpy(fullpath, path);
			goto ELFchecked;
#else
			goto ELFnotFound;
#endif
		} else if (!strncmp(path, "mx4sio", 6)) {
#ifdef MX4SIO
			if (!mx4sio_driver_running && !loadMx4sioModules())
				goto ELFnotFound;
			if ((t = checkELFheader(path)) <= 0)
				goto ELFnotFound;
			party[0] = 0;
			strcpy(fullpath, path);
			goto ELFchecked;
#else
			goto ELFnotFound;
#endif
		} else if (!strncmp(path, "mmce", 4)) {
#ifdef MMCE
			loadMmceModules();
			if ((t = checkELFheader(path)) <= 0)
				goto ELFnotFound;
			strcpy(fullpath, path);
			goto ELFchecked;
#else
			goto ELFnotFound;
#endif
		} else if (!strncmp(path, "usb", 3)) {
			char *pathSep;

			if ((t = checkELFheader(path)) <= 0)
				goto ELFnotFound;
			party[0] = 0;
			if (genFixPath(path, fullpath) < 0)
				goto ELFnotFound;
			pathSep = strchr(fullpath, '/');
			if (pathSep && (pathSep - fullpath < 7) && pathSep[-1] == ':')
				strcpy(fullpath + (pathSep - fullpath), pathSep + 1);
			goto ELFchecked;
		} else if (!strncmp(path, "ata", 3)) {
			char *pathSep;

#ifdef EXFAT
			loadAtaModules();
#endif
			if ((t = checkELFheader(path)) <= 0)
				goto ELFnotFound;
			party[0] = 0;
			strcpy(fullpath, path);
			pathSep = strchr(path, '/');
			if (pathSep && (pathSep - path < 7) && pathSep[-1] == ':')
				strcpy(fullpath + (pathSep - path), pathSep + 1);
			goto ELFchecked;
		} else if (!strncmp(path, "mass", 4)) {
			char *pathSep;

			if ((t = checkELFheader(path)) <= 0)
				goto ELFnotFound;
			party[0] = 0;

			strcpy(fullpath, path);
			pathSep = strchr(path, '/');
			if (pathSep && (pathSep - path < 7) && pathSep[-1] == ':')
				strcpy(fullpath + (pathSep - path), pathSep + 1);
			goto ELFchecked;
		} else if (!strncmp(path, "host:", 5)) {
#ifdef ETH
			initHOST();
			party[0] = 0;
			strcpy(fullpath, "host:");
			if (path[5] == '/')
				strcat(fullpath, path + 6);
			else
				strcat(fullpath, path + 5);
			makeHostPath(fullpath, fullpath);
			goto CheckELF_fullpath;
#else
			goto ELFnotFound;
#endif
		} else if (!strncmp(path, "udpfs:", 6)) {
#ifdef UDPFS
			load_udpfs();
			party[0] = 0;
			snprintf(fullpath, sizeof(fullpath), "%s", path);
			goto CheckELF_fullpath;
#else
			goto ELFnotFound;
#endif
		} else if (!stricmp(path, setting->Misc_OSDSYS)) {
		char arg0[20], arg1[20], arg2[20], arg3[40];
		char *args[4] = {arg0, arg1, arg2, arg3};
		char kelf_loader[40];
		int fd, argc;

		if (setting->LK_Flag[SETTING_LK_OSDSYS] && setting->LK_Path[SETTING_LK_OSDSYS][0])
			strcpy(path, setting->LK_Path[SETTING_LK_OSDSYS]);
		else
			strcpy(path, default_OSDSYS_path);

		fd = genOpen(path, FIO_O_RDONLY);
		if (fd >= 0)
			goto close_fd_and_launch_OSDSYS;
		if (strncmp(path, "mc:", 3) != 0)
			goto ELFnotFound;
		strcpy(fullpath, path);
		path[2] = '0';
		strcpy(path + 3, fullpath + 2);
		fd = genOpen(path, FIO_O_RDONLY);
		if (fd >= 0)
			goto close_fd_and_launch_OSDSYS;
		path[2] = '1';
		fd = genOpen(path, FIO_O_RDONLY);
		if (fd >= 0)
			goto close_fd_and_launch_OSDSYS;
		if (fd < 0)
			goto ELFnotFound;
	close_fd_and_launch_OSDSYS:
		genClose(fd);
		strcpy(arg0, "-m rom0:SIO2MAN");
		strcpy(arg1, "-m rom0:MCMAN");
		strcpy(arg2, "-m rom0:MCSERV");
		sprintf(arg3, "-x %s", path);
		argc = 4;
		strcpy(kelf_loader, "moduleload");
		CleanUp();
		LoadExecPS2(kelf_loader, argc, args);

	} else if (!stricmp(path, setting->Misc_PS2Disc)) {
		drawMsg(LNG(Reading_SYSTEMCNF));
		party[0] = 0;
		readSystemCnf();
		if (BootDiscType == 2) {  //Boot a PS2 disc
			strcpy(fullpath, SystemCnf_BOOT2);
			goto CheckELF_fullpath;
		}
		if (BootDiscType == 1) {  //Boot a PS1 disc
			char disc_gameid[12];
			int show_disc_gameid;
			char *args[2] = {SystemCnf_BOOT, SystemCnf_VER};

			show_disc_gameid = 0;
			if (!setting->cdrom_disable_gameid)
				show_disc_gameid = buildLaunchGameID(SystemCnf_BOOT, disc_gameid, sizeof(disc_gameid));

			CleanUp();
			if (show_disc_gameid)
				displayRetroGemGameID(disc_gameid, 2);
			LoadExecPS2("rom0:PS1DRV", 2, args);
			sprintf(mainMsg, "PS1DRV %s", LNG(Failed));
			goto Done_PS2Disc;
		}
		if (uLE_cdDiscValid()) {
			if (cdmode == SCECdDVDV) {
				x = Check_ESR_Disc();
				DPRINTF("Check_ESR_Disc => %d\n", x);
				if (x > 0) {  //ESR Disc, so launch ESR
					if (setting->LK_Flag[SETTING_LK_ESR] && setting->LK_Path[SETTING_LK_ESR][0])
						strcpy(path, setting->LK_Path[SETTING_LK_ESR]);
					else
						strcpy(path, default_ESR_path);

					goto Recurse_for_ESR;
				}

				//DVD Video Disc, so launch DVD player
				char arg0[20], arg1[20], arg2[20], arg3[40];
				char *args[4] = {arg0, arg1, arg2, arg3};
				char kelf_loader[40];
				char MG_region[10];
				int i, pos, tst, argc;

				if ((tst = SifLoadModule("rom0:ADDDRV", 0, NULL)) < 0)
					goto Fail_DVD_Video;

				strcpy(arg0, "-k rom1:EROMDRVA");
				strcpy(arg1, "-m erom0:UDFIO");
				strcpy(arg2, "-x erom0:DVDPLA");
				argc = 3;
				strcpy(kelf_loader, "moduleload2 rom1:UDNL rom1:DVDCNF");

				strcpy(MG_region, "ACEJMORU");
				pos = strlen(arg0) - 1;
				for (i = 0; i < 9; i++) {  //NB: MG_region[8] is a string terminator
					arg0[pos] = MG_region[i];
					tst = SifLoadModuleEncrypted(arg0 + 3, 0, NULL);
					if (tst >= 0)
						break;
				}

				pos = strlen(arg2);
				if (i == 8)
					strcpy(&arg2[pos - 3], "ELF");
				else
					arg2[pos - 1] = MG_region[i];
				//At this point all args are ready to use internal DVD player

				//We must check for an updated player on MC
				dvdpl_path[6] = rough_region;
				dvdpl_update = 0;
				for (i = 0; i < 2; i++) {
					dvdpl_path[2] = '0' + i;
					if (wleExists(dvdpl_path)) {
						dvdpl_update = 1;
						break;
					}
				}

				if ((tst < 0) && (dvdpl_update == 0))
					goto Fail_PS2Disc;  //We must abort if no working kelf found

				if (dvdpl_update) {  // Launch DVD player from memory card
					strcpy(arg0, "-m rom0:SIO2MAN");
					strcpy(arg1, "-m rom0:MCMAN");
					strcpy(arg2, "-m rom0:MCSERV");
					sprintf(arg3, "-x %s", dvdpl_path);  // -x :elf is encrypted for mc
					argc = 4;
					strcpy(kelf_loader, "moduleload");
				}

				CleanUp();
				LoadExecPS2(kelf_loader, argc, args);

			Fail_DVD_Video:
				sprintf(mainMsg, "DVD-Video %s", LNG(Failed));
				goto Done_PS2Disc;
			}
			if (cdmode == SCECdCDDA) {
				//Fail_CDDA:
				sprintf(mainMsg, "CDDA %s", LNG(Failed));
				goto Done_PS2Disc;
			}
		}
	Fail_PS2Disc:
		sprintf(mainMsg, "%s => %s CDVD 0x%02X", LNG(PS2Disc), LNG(Failed), cdmode);
	Done_PS2Disc:
		x = x;
	} else if (!stricmp(path, setting->Misc_FileBrowser)) {
		mainMsg[0] = 0;
		tmp[0] = 0;
		LastDir[0] = 0;
		getFilePath(tmp, FALSE);
		if (tmp[0]) {
			if (IsTextEditorFileType(tmp)) {

				TextEditor(tmp);
			} else
				Execute(tmp);
		}
		return;
	} else if (!stricmp(path, setting->Misc_PS2Browser)) {
		Exit(0);
		//There has been a major change in the code for calling PS2Browser
		//The method above is borrowed from PS2MP3. It's independent of ELF loader
		//The method below was used earlier, but causes reset with new ELF loader
		//party[0]=0;
		//strcpy(fullpath,"rom0:OSDSYS");
#ifdef ETH
	} else if (!stricmp(path, setting->Misc_PS2Net)) {
		mainMsg[0] = 0;
		loadNetModules();
		snprintf(mainMsg, sizeof(mainMsg), "%s", netConfig);
		return;
#endif
	} else if (!stricmp(path, setting->Misc_PS2PowerOff)) {
		mainMsg[0] = 0;
		drawMsg(LNG(Powering_Off_Console));
		setupPowerOff();
		closeAllAndPoweroff();
		return;
	} else if (!stricmp(path, setting->Misc_HddManager)) {
		hddManager();
		return;
	} else if (!stricmp(path, setting->Misc_TextEditor)) {
		TextEditor(NULL);
		return;
	} else if (!stricmp(path, setting->Misc_Configure)) {
		Load_External_Language();
		loadFont(setting->font_file);
		config(mainMsg, CNF);
		return;
			//Next clause is for an optional font test routine
	} else if (!stricmp(path, setting->Misc_ShowFont)) {
		ShowFont();
		return;
	} else if (!stricmp(path, setting->Misc_Debug_Info)) {
		ShowDebugInfo();
		return;
	} else if (!stricmp(path, setting->Misc_About_uLE)) {
		Show_About_uLE();
		return;
	} else if (!stricmp(path, setting->Misc_Show_Build_Info)) {
		Show_build_info();
		return;
	} else if (!strncmp(path, "cdfs", 4)) {
		loadCdModules();
		LCDVD_FLUSHCACHE();
		LCDVD_DISKREADY(0);
		party[0] = 0;
		goto CheckELF_path;
	} else if (!strncmp(path, "rom", 3)) {
		party[0] = 0;
	CheckELF_path:
		strcpy(fullpath, path);
	CheckELF_fullpath:
		if ((t = checkELFheader(fullpath)) <= 0)
			goto ELFnotFound;
	ELFchecked:
		{
			int show_launch_gameid = 0;
			int disc_launch = isLikelyDiscLaunch(path);
			char launch_gameid[12];

			if (disc_launch) {
				if (!setting->cdrom_disable_gameid)
					show_launch_gameid = buildLaunchGameID(fullpath, launch_gameid, sizeof(launch_gameid));
			} else if (setting->app_gameid) {
				show_launch_gameid = buildLaunchGameID(fullpath, launch_gameid, sizeof(launch_gameid));
			}

			x = setting->reboot_iop_elf_load;
			CleanUp();
			if (show_launch_gameid)
				displayRetroGemGameID(launch_gameid, 2);
			RunLoaderElf(fullpath, party, path, t, x);
		}
	} else {  //Invalid path
		t = 0;
	ELFnotFound:
		if (t == 0)
			sprintf(mainMsg, "%s %s.", fullpath, LNG(is_Not_Found));
		else
			sprintf(mainMsg, "%s: %s.", LNG(This_file_isnt_an_ELF), fullpath);
		return;
	}
}
//------------------------------
//endfunc Execute
//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	int event, post_event = 0;
	char RunPath[MAX_PATH];
	int RunELF_index, nElfs = 0;
	enum BOOT_DEVICE boot = BOOT_DEV_UNKNOWN;
	int CNF_error = -1;  //assume error until CNF correctly loaded
	int i;

	boot_argc = argc;
	for (i = 0; (i < argc) && (i < 8); i++)
		boot_argv[i] = argv[i];

	Reset();
	Init_Default_Language();
	if (wleExists("rom0:PSXVER")) {
		console_is_PSX = 1;
		DPRINTF("# Console is PSX-DESR\n");
	}
	DPRINTF("Loading USB modules\n");
	loadUsbModules();

	boot = prepareBootDeviceAndPath((argc > 0) ? argv[0] : NULL, boot_path, sizeof(boot_path));
	initializeBootDisplayDefaults();

	CNF_error = loadConfig(mainMsg, strcpy(CNF, "LAUNCHELF.CNF"));
	bringUpBootNetworkStack(boot);
	initializeBootGraphics();

	swapKeys = setting->swapKeys;

	//It's time to load and init drivers
	DPRINTF("Getting IPCONFIG\n");
	if (boot != BOOT_DEVICE_HOST)
		getIpConfig();

	WaitTime = Timer();
	DPRINTF("setup pad\n");
	setupPad();  //Comment out this line when using early setupPad above
	DPRINTF("Starting keyboard\n");
	startKbd();
	WaitTime = Timer();

	init_delay = setting->Init_Delay * 1000;
	init_delay_start = Timer();
	timeout = (setting->timeout + 1) * 1000;
	timeout_start = Timer();

	Load_External_Language();
	loadConfiguredBootFont();

	gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00));
	if (CNF_error < 0)
		sprintf(mainMsg, "%s", LNG(Failed_To_Load));
	else
		sprintf(mainMsg, "%s", LNG(Loaded_Config));

	//Here nearly everything is ready for the main menu event loop
	//But before we start that, we need to validate CNF_Path
	Validate_CNF_Path();

	RunPath[0] = 0;  //Nothing to run yet
	cdmode = -1;     //flag unchecked cdmode state
	event = 1;       //event = initial entry
	DPRINTF("starting main menu event loop\n");
	//----- Start of main menu event loop -----
	while (1) {
		int DiscType_ix;

		//Background event section
		uLE_cdStop();              //Test disc state and if needed stop disc (updates cdmode)
		if (cdmode == old_cdmode)  //if disc detection did not change state
			goto done_discControl;

		event |= 4;  //event |= disc change detection
		if (cdmode <= 0)
			sprintf(mainMsg, "%s ", LNG(No_Disc));
		else if (cdmode >= 1 && cdmode <= 4)
			sprintf(mainMsg, "%s == ", LNG(Detecting_Disc));
		else  //if(cdmode>=5)
			sprintf(mainMsg, "%s == ", LNG(Stop_Disc));

		DiscType_ix = 0;
		for (i = 0; DiscTypes[i].name[0]; i++)
			if (DiscTypes[i].type == uLE_cdmode)
				DiscType_ix = i;

		sprintf(mainMsg + strlen(mainMsg), DiscTypes[DiscType_ix].name);
	//Comment out the debug output below when not needed
	/*
		sprintf(mainMsg+strlen(mainMsg),
			"  cdmode==%d  uLE_cdmode==%d  type_ix==%d",
			cdmode, uLE_cdmode, DiscType_ix);
		//*/
	done_discControl:
		if (init_delay) {
			prev_init_delay = init_delay;
			CurrTime = Timer();
			if (CurrTime > (init_delay_start + init_delay)) {
				init_delay = 0;
				timeout_start = CurrTime;
			} else {
				init_delay = init_delay_start + init_delay - CurrTime;
				init_delay_start = CurrTime;
			}
			if ((init_delay / 1000) != (prev_init_delay / 1000))
				event |= 8;  //event |= visible delay change
		} else if (timeout && !user_acted) {
			prev_timeout = timeout;
			CurrTime = Timer();
			if (CurrTime > (timeout_start + timeout)) {
				timeout = 0;
			} else {
				timeout = timeout_start + timeout - CurrTime;
				timeout_start = CurrTime;
			}
			if ((timeout / 1000) != (prev_timeout / 1000))
				event |= 8;  //event |= visible timeout change
		}

		//Display section
		if (event || post_event) {  //NB: We need to update two frame buffers per event
			nElfs = drawMainScreen();
		}
		drawScr();
		post_event = event;
		event = 0;

		//Pad response section
		if (!init_delay && (waitAnyPadReady(), readpad())) {
			if (new_pad) {
				event |= 2;  //event |= pad command
			}
			RunELF_index = -1;
			switch (mode) {
				case BUTTON:
					if ((new_pad & PAD_SELECT) && (new_pad & PAD_START)) {
						initConfig();
						updateScreenMode();
					} else if (new_pad & PAD_CIRCLE)
						RunELF_index = SETTING_LK_CIRCLE;
					else if (new_pad & PAD_CROSS)
						RunELF_index = SETTING_LK_CROSS;
					else if (new_pad & PAD_SQUARE)
						RunELF_index = SETTING_LK_SQUARE;
					else if (new_pad & PAD_TRIANGLE)
						RunELF_index = SETTING_LK_TRIANGLE;
					else if (new_pad & PAD_L1)
						RunELF_index = SETTING_LK_L1;
					else if (new_pad & PAD_R1)
						RunELF_index = SETTING_LK_R1;
					else if (new_pad & PAD_L2)
						RunELF_index = SETTING_LK_L2;
					else if (new_pad & PAD_R2)
						RunELF_index = SETTING_LK_R2;
					else if (new_pad & PAD_L3)
						RunELF_index = SETTING_LK_L3;
					else if (new_pad & PAD_R3)
						RunELF_index = SETTING_LK_R3;
					else if (new_pad & PAD_START)
						RunELF_index = SETTING_LK_START;
					else if (new_pad & PAD_SELECT)
						RunELF_index = SETTING_LK_SELECT;
					else if ((new_pad & PAD_LEFT) && setting->LK_Flag[SETTING_LK_LEFT])
						RunELF_index = SETTING_LK_LEFT;
					else if ((new_pad & PAD_RIGHT) && setting->LK_Flag[SETTING_LK_RIGHT])
						RunELF_index = SETTING_LK_RIGHT;
					else if (new_pad & PAD_UP || new_pad & PAD_DOWN) {
						user_acted = 1;
						selected = 0;
						mode = DPAD;
					}
					if (RunELF_index >= 0 && setting->LK_Path[RunELF_index][0])
						strcpy(RunPath, setting->LK_Path[RunELF_index]);
					break;

				case DPAD:
					if (new_pad & PAD_UP) {
						selected--;
						if (selected < 0)
							selected = nElfs - 1;
					} else if (new_pad & PAD_DOWN) {
						selected++;
						if (selected >= nElfs)
							selected = 0;
					} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
						mode = BUTTON;
					} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
						if (setting->LK_Path[menu_LK[selected]][0])
							strcpy(RunPath, setting->LK_Path[menu_LK[selected]]);
					}
					break;
			}  //ends switch(mode)
		}      //ends Pad response section

		if (!user_acted && ((timeout / 1000) == 0) && setting->LK_Path[SETTING_LK_AUTO][0] && mode == BUTTON) {
			event |= 8;  //event |= visible timeout change
			strcpy(RunPath, setting->LK_Path[SETTING_LK_AUTO]);
		}

		if (RunPath[0]) {
			user_acted = 1;
			mode = BUTTON;
			Execute(RunPath);
			RunPath[0] = 0;
		}
	}  //ends while(1)
	   //----- End of main menu event loop -----
}
//------------------------------
//endfunc main
//---------------------------------------------------------------------------
//End of file: main.c
//---------------------------------------------------------------------------
