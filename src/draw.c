//--------------------------------------------------------------
//File name:   draw.c
//--------------------------------------------------------------
#include "launchelf.h"

GSGLOBAL *gsGlobal;
GSTEXTURE TexIcon[2];
int SCREEN_WIDTH = 640;
int SCREEN_HEIGHT = 448;
//dlanor: values shown above are defaults for NTSC mode

int updateScr_1;      //dlanor: flags screen updates for drawScr()
int updateScr_2;      //dlanor: used for anti-flicker delay in drawScr()
u64 updateScr_t = 0;  //dlanor: exit time of last drawScr()

char LastMessage[MAX_TEXT_LINE + 2];

int Menu_start_x = SCREEN_MARGIN + LINE_THICKNESS + FONT_WIDTH;
int Menu_title_y = SCREEN_MARGIN;
int Menu_message_y = SCREEN_MARGIN + FONT_HEIGHT;
int Frame_start_y = SCREEN_MARGIN + 2 * FONT_HEIGHT + 2;  //First line of menu frame
int Menu_start_y = SCREEN_MARGIN + 2 * FONT_HEIGHT + LINE_THICKNESS + 5;
//dlanor: Menu_start_y is the 1st pixel line that may be used for main content of a menu
//dlanor: values below are only calculated when a rez is activated
int Menu_end_y;      //Normal menu display should not use pixels at this line or beyond
int Frame_end_y;     //first line of frame bottom
int Menu_tooltip_y;  //Menus may also use this row for tooltips


//The font file ELISA100.FNT is needed to display MC save titles in japanese
//and the arrays defined here are needed to find correct data in that file
const u16 font404[] = {
    0xA2AF, 11,
    0xA2C2, 8,
    0xA2D1, 11,
    0xA2EB, 7,
    0xA2FA, 4,
    0xA3A1, 15,
    0xA3BA, 7,
    0xA3DB, 6,
    0xA3FB, 4,
    0xA4F4, 11,
    0xA5F7, 8,
    0xA6B9, 8,
    0xA6D9, 38,
    0xA7C2, 15,
    0xA7F2, 13,
    0xA8C1, 720,
    0xCFD4, 43,
    0xF4A5, 1030,
    0, 0};

// ASCII��SJIS�̕ϊ��p�z��
static const u8 sjis_lookup_81[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x00
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x10
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x20
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x30
    ' ', ',', '.', ',', '.', 0xFF, ':', ';', '?', '!', 0xFF, 0xFF, '\xff', '`', 0xFF, '^',              // 0x40
    0xFF, '_', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '0', 0xFF, '-', '-', 0xFF, 0xFF,      // 0x50
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '\'', '\'', '"', '"', '(', ')', 0xFF, 0xFF, '[', ']', '{',         // 0x60
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '+', '-', 0xFF, '*', 0xFF,     // 0x70
    '/', '=', 0xFF, '<', '>', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '\xff', 0xFF, 0xFF, '\xff', 0xFF,        // 0x80
    '$', 0xFF, 0xFF, '%', '#', '&', '*', '@', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,        // 0x90
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xA0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xB0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '&', '|', '!', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,     // 0xC0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xD0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xE0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xF0
};
static const u8 sjis_lookup_82[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x00
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x10
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x20
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x30
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '0',   // 0x40
    '1', '2', '3', '4', '5', '6', '7', '8', '9', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,           // 0x50
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',                  // 0x60
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,            // 0x70
    0xFF, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',                 // 0x80
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,             // 0x90
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xA0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xB0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xC0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xD0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xE0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0xF0
};
//--------------------------------------------------------------
void setScrTmp(const char *msg0, const char *msg1)
{
	int x, y;
	char temp_txt[64];

	x = SCREEN_MARGIN;
	y = Menu_title_y;
	printXY(setting->Menu_Title, x, y, setting->color[COLOR_TEXT], TRUE, 0);
	sprintf(temp_txt, " \xff\x34 LaunchELF %s \xff\x34", ULE_VERSION);
	printXY(temp_txt, SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH * strlen(temp_txt), y,
	        setting->color[COLOR_FRAME], TRUE, 0);

	strncpy(LastMessage, msg0, MAX_TEXT_LINE);
	LastMessage[MAX_TEXT_LINE] = '\0';
	printXY(msg0, x, Menu_message_y, setting->color[COLOR_SELECT], TRUE, 0);

	if (setting->Menu_Frame)
		drawFrame(SCREEN_MARGIN, Frame_start_y,
		          SCREEN_WIDTH - SCREEN_MARGIN, Frame_end_y, setting->color[COLOR_FRAME]);

	printXY(msg1, x, Menu_tooltip_y, setting->color[COLOR_SELECT], TRUE, 0);
}
//--------------------------------------------------------------
void drawSprite(u64 color, int x1, int y1, int x2, int y2)
{
	gsKit_prim_sprite(gsGlobal, x1, y1, x2, y2, 0, color);
}
//--------------------------------------------------------------
void drawPopSprite(u64 color, int x1, int y1, int x2, int y2)
{
	gsKit_prim_sprite(gsGlobal, x1, y1, x2, y2, 0, color);
}
//--------------------------------------------------------------
//drawOpSprite exists only to eliminate the use of primitive sprite functions
//that are specific to the graphics lib used (currently gsKit). So normally
//it will merely be a 'wrapper' function for one of the lib calls, except
//that it will also perform any coordinate adjustments (if any)implemented for
//the functions drawSprite and drawPopSprite, to keep all of them compatible.
//
void drawOpSprite(u64 color, int x1, int y1, int x2, int y2)
{
	gsKit_prim_sprite(gsGlobal, x1, y1, x2, y2, 0, color);
}
//--------------------------------------------------------------
void drawMsg(const char *msg)
{
	strncpy(LastMessage, msg, MAX_TEXT_LINE);
	LastMessage[MAX_TEXT_LINE] = '\0';
	drawSprite(setting->color[COLOR_BACKGR], 0, Menu_message_y - 1,
	           SCREEN_WIDTH, Frame_start_y);
	printXY(msg, SCREEN_MARGIN, Menu_message_y, setting->color[COLOR_SELECT], TRUE, 0);
	drawScr();
}
//--------------------------------------------------------------
void drawLastMsg(void)
{
	drawSprite(setting->color[COLOR_BACKGR], 0, Menu_message_y - 1,
	           SCREEN_WIDTH, Frame_start_y);
	printXY(LastMessage, SCREEN_MARGIN, Menu_message_y, setting->color[COLOR_SELECT], TRUE, 0);
	drawScr();
}
//--------------------------------------------------------------
static void applyGSParams(void)
{
	switch (gsGlobal->Mode) {
		case GS_MODE_VGA_640_60:
		case GS_MODE_DTV_480P:
			SCREEN_WIDTH = 640;
			SCREEN_HEIGHT = 448;
			gsGlobal->Interlace = GS_NONINTERLACED;
			gsGlobal->Field = GS_FRAME;
			Menu_end_y = Menu_start_y + 22 * FONT_HEIGHT;
			break;
		case GS_MODE_PAL:
			SCREEN_WIDTH = 640;
			SCREEN_HEIGHT = 512;
			gsGlobal->Interlace = GS_INTERLACED;
			gsGlobal->Field = GS_FIELD;
			Menu_end_y = Menu_start_y + 26 * FONT_HEIGHT;
			break;
		default:
		case GS_MODE_NTSC:
			SCREEN_WIDTH = 640;
			SCREEN_HEIGHT = 448;
			gsGlobal->Interlace = GS_INTERLACED;
			gsGlobal->Field = GS_FIELD;
			Menu_end_y = Menu_start_y + 22 * FONT_HEIGHT;
	}  // end TV_Mode change

	Frame_end_y = Menu_end_y + 3;
	Menu_tooltip_y = Frame_end_y + LINE_THICKNESS + 2;

	// Init screen size
	gsGlobal->Width = SCREEN_WIDTH;
	gsGlobal->Height = SCREEN_HEIGHT;
}

static void initScreenParams(void)
{
	// Ensure that the offsets are within valid ranges.
	if (setting->screen_x < -gsGlobal->StartX)
		setting->screen_x = -gsGlobal->StartX;
	else if (setting->screen_x > gsGlobal->StartX)
		setting->screen_x = gsGlobal->StartX;

	if (setting->screen_y < -gsGlobal->StartY)
		setting->screen_y = -gsGlobal->StartY;
	else if (setting->screen_y > gsGlobal->StartY)
		setting->screen_y = gsGlobal->StartY;
}

void setupGS(void)
{
	int New_TV_mode = setting->TV_mode;

	// GS Init
	gsGlobal = gsKit_init_global();

	if (New_TV_mode == TV_mode_AUTO) {         //If no forced request
		New_TV_mode = uLE_InitializeRegion();  //Let console region decide TV_mode
	}

	// Screen display mode
	TV_mode = New_TV_mode;
	switch (New_TV_mode) {
		case TV_mode_PAL:
			gsGlobal->Mode = GS_MODE_PAL;
			break;
		case TV_mode_480P:
			gsGlobal->Mode = GS_MODE_DTV_480P;
			break;
		case TV_mode_VGA:
			gsGlobal->Mode = GS_MODE_VGA_640_60;
			break;
		default:
		case TV_mode_NTSC:
			gsGlobal->Mode = GS_MODE_NTSC;
			break;
	}

	// Screen size
	applyGSParams();

	// Buffer Init
	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsGlobal->DoubleBuffering = GS_SETTING_OFF;
	gsGlobal->ZBuffering = GS_SETTING_OFF;

	// DMAC Init
	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_GIF);

	// Screen Init (will also clear screen once)
	gsKit_init_screen(gsGlobal);

	// Screen Position Init
	initScreenParams();

	gsKit_set_display_offset(gsGlobal, setting->screen_x, setting->screen_y);

	// Screen render mode
	gsKit_mode_switch(gsGlobal, GS_ONESHOT);
}
//--------------------------------------------------------------
void updateScreenMode(void)
{
	int setGS_flag = 0;
	int New_TV_mode = setting->TV_mode;

	if (New_TV_mode == TV_mode_AUTO) {         //If no forced request
		New_TV_mode = uLE_InitializeRegion();  //Let console region decide TV_mode
	}

	if (New_TV_mode != TV_mode) {
		setGS_flag = 1;
		TV_mode = New_TV_mode;

		switch (New_TV_mode) {
			case TV_mode_PAL:
				gsGlobal->Mode = GS_MODE_PAL;
				break;
			case TV_mode_480P:
				gsGlobal->Mode = GS_MODE_DTV_480P;
				break;
			case TV_mode_VGA:
				gsGlobal->Mode = GS_MODE_VGA_640_60;
				break;
			default:
			case TV_mode_NTSC:
				gsGlobal->Mode = GS_MODE_NTSC;
				break;
		}
	}  // end TV_Mode change

	// Init screen parameters
	applyGSParams();

	if (setGS_flag) {
		// Init screen modes
		gsKit_init_screen(gsGlobal);
	}

	// Screen Position Init
	initScreenParams();

	// Init screen position
	gsKit_set_display_offset(gsGlobal, setting->screen_x, setting->screen_y);
}
void loadIcon(void)
{
	TexIcon[0].Width = 16;
	TexIcon[0].Height = 16;
	TexIcon[0].PSM = GS_PSM_CT32;
	TexIcon[0].Mem = (void *)icon_folder;
	gsGlobal->CurrentPointer = 0x280000;
	TexIcon[0].Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(TexIcon[0].Width, TexIcon[0].Height, TexIcon[0].PSM), GSKIT_ALLOC_USERBUFFER);
	TexIcon[0].Filter = GS_FILTER_LINEAR;
	gsKit_texture_upload(gsGlobal, &TexIcon[0]);
	free(TexIcon[0].Mem);

	TexIcon[1].Width = 16;
	TexIcon[1].Height = 16;
	TexIcon[1].PSM = GS_PSM_CT32;
	TexIcon[1].Mem = (void *)icon_warning;
	gsGlobal->CurrentPointer = 0x284000;
	TexIcon[1].Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(TexIcon[0].Width, TexIcon[0].Height, TexIcon[0].PSM), GSKIT_ALLOC_USERBUFFER);
	TexIcon[1].Filter = GS_FILTER_LINEAR;
	gsKit_texture_upload(gsGlobal, &TexIcon[1]);
	free(TexIcon[1].Mem);
}
//--------------------------------------------------------------
int loadFont(char *path_arg)
{
	int fd;

	if (strlen(path_arg) != 0) {
		char FntPath[MAX_PATH];
		genFixPath(path_arg, FntPath);
		fd = genOpen(FntPath, FIO_O_RDONLY);
		if (fd < 0) {
			genClose(fd);
			goto use_default;
		}  // end if failed open file
		genLseek(fd, 0, SEEK_SET);
		if (genLseek(fd, 0, SEEK_END) > 4700) {
			genClose(fd);
			goto use_default;
		}
		genLseek(fd, 0, SEEK_SET);
		u8 FontHeader[100];
		genRead(fd, FontHeader, 100);
		if ((FontHeader[0] == 0x00) &&
		    (FontHeader[1] == 0x02) &&
		    (FontHeader[70] == 0x60) &&
		    (FontHeader[72] == 0x60) &&
		    (FontHeader[83] == 0x90)) {
			genLseek(fd, 1018, SEEK_SET);
			memset(FontBuffer, 0, 32 * 16);
			genRead(fd, FontBuffer + 32 * 16, 224 * 16);  //.fnt files skip 1st 32 chars
			genClose(fd);
			return 1;
		} else {  // end if good fnt file
			genClose(fd);
			goto use_default;
		}     // end else bad fnt file
	} else {  // end if external font file
	use_default:
		memcpy(FontBuffer, &font_uLE, 4096);
	}  // end else build-in font
	return 0;
}
//------------------------------
//endfunc loadFont
//--------------------------------------------------------------
void clrScr(u64 color)
{
	gsKit_prim_sprite(gsGlobal, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, color);
}

//--------------------------------------------------------------
void drawScr(void)
{
	if (updateScr_2) {  //Did we render anything last time
		while (Timer() < updateScr_t + 5)
			;  //if so, delay to complete rendering
	}
	gsKit_sync_flip(gsGlobal);   //Await sync and flip buffers
	gsKit_queue_exec(gsGlobal);  //Start rendering recent transfers for NEXT time
	updateScr_t = Timer();       //Note the time when the rendering started
	updateScr_2 = updateScr_1;   //Note if this rendering had expected updates
	updateScr_1 = 0;             //Note that we've nothing expected for next time
}  //NB: Apparently the GS keeps rendering while we continue with other work
//--------------------------------------------------------------
void drawFrame(int x1, int y1, int x2, int y2, u64 color)
{
	updateScr_1 = 1;

	//Top horizontal edge
	gsKit_prim_sprite(gsGlobal, x1, y1, x2, y1 + LINE_THICKNESS - 1, 1, color);

	//Bottom horizontal
	gsKit_prim_sprite(gsGlobal, x1, y2 - LINE_THICKNESS + 1, x2, y2, 1, color);

	//Left vertical edge
	gsKit_prim_sprite(gsGlobal, x1, y1, x1 + LINE_THICKNESS - 1, y2, 1, color);

	//Right vertical edge
	gsKit_prim_sprite(gsGlobal, x2 - LINE_THICKNESS + 1, y1, x2, y2, 1, color);
}

//--------------------------------------------------------------
// draw a char using the system font (16x16)
void drawChar(unsigned int c, int x, int y, u64 colour)
{
	int i, j, pixBase, pixMask;
	u8 *cm;

	updateScr_1 = 1;

	if (c >= FONT_COUNT)
		c = '_';
	if (c > 0xFF)                  //if char is beyond normal ascii range
		cm = &font_uLE[c * 16];    //  cm points to special char def in default font
	else                           //else char is inside normal ascii range
		cm = &FontBuffer[c * 16];  //  cm points to normal char def in active font

	pixMask = 0x80;
	for (i = 0; i < 8; i++) {  //for i == each pixel column
		pixBase = -1;
		for (j = 0; j < 16; j++) {                     //for j == each pixel row
			if ((pixBase < 0) && (cm[j] & pixMask)) {  //if start of sequence
				pixBase = j;
			} else if ((pixBase > -1) && !(cm[j] & pixMask)) {  //if end of sequence
				gsKit_prim_sprite(gsGlobal, x + i, y + pixBase - 1, x + i + 1, y + j - 1, 1, colour);
				pixBase = -1;
			}
		}                  //ends for j == each pixel row
		if (pixBase > -1)  //if end of sequence including final row
			gsKit_prim_sprite(gsGlobal, x + i, y + pixBase - 1, x + i + 1, y + j - 1, 1, colour);
		pixMask >>= 1;
	}  //ends for i == each pixel column
}
//------------------------------
//endfunc drawChar
//--------------------------------------------------------------
// draw a char using the ELISA font (16x16)
void drawChar2(int n, int x, int y, u64 colour)
{
	unsigned int i, j;
	u8 b;

	updateScr_1 = 1;

	for (i = 0; i < 8; i++) {
		b = elisaFnt[n + i];
		for (j = 0; j < 8; j++) {
			if (b & 0x80) {
				gsKit_prim_sprite(gsGlobal, x + j, y + i * 2 - 2, x + j + 1, y + i * 2, 1, colour);
			}
			b = b << 1;
		}
	}
}
//------------------------------
//endfunc drawChar2
//--------------------------------------------------------------
// draw a string of characters, without shift-JIS support
int printXY(const char *s, int x, int y, u64 colour, int draw, int space)
{
	unsigned int c1, c2;
	int i;
	int text_spacing = 8;

	if (space > 0) {
		while ((strlen(s) * text_spacing) > space)
			if (--text_spacing <= 5)
				break;
	} else {
		while ((strlen(s) * text_spacing) > SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH * 2)
			if (--text_spacing <= 5)
				break;
	}

	i = 0;
	while ((c1 = (unsigned char)s[i++]) != 0) {
		if (c1 != 0xFF) {  // Normal character
			if (draw)
				drawChar(c1, x, y, colour);
			x += text_spacing;
			if (x > SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH)
				break;
			continue;
		}  //End if for normal character
		// Here we got a sequence starting with 0xFF ('\xff')
		if ((c2 = (unsigned char)s[i++]) == 0)
			break;
		if ((c2 < '0') || (c2 > '='))
			continue;
		c1 = (c2 - '0') * 2 + 0x100;
		if (draw) {
			//expand sequence �0=Circle  �1=Cross  �2=Square  �3=Triangle  �4=FilledBox
			//"\xff:"=Pad_Right  "\xff;"=Pad_Down  "\xff<"=Pad_Left  "\xff="=Pad_Up
			drawChar(c1, x, y, colour);
			x += 8;
			if (x > SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH)
				break;
			drawChar(c1 + 1, x, y, colour);
			x += 8;
			if (x > SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH)
				break;
		}
	}  // ends while(1)
	return x;
}
//------------------------------
//endfunc printXY
//--------------------------------------------------------------
// draw a string of characters, with shift-JIS support (only for gamesave titles)
int printXY_sjis(const unsigned char *s, int x, int y, u64 colour, int draw)
{
	int n;
	u8 ascii;
	u16 code;
	int i, j, tmp;

	i = 0;
	while (s[i]) {
		if ((s[i] & 0x80) && s[i + 1]) {  //we have top bit and some more char ?
			// SJIS
			code = s[i++];
			code = (code << 8) + s[i++];

			switch (code) {
				// Circle == "��"
				case 0x819B:
					if (draw) {
						drawChar(0x100, x, y, colour);
						drawChar(0x101, x + 8, y, colour);
					}
					x += 16;
					break;
				// Cross == "\xff~"
				case 0x817E:
					if (draw) {
						drawChar(0x102, x, y, colour);
						drawChar(0x103, x + 8, y, colour);
					}
					x += 16;
					break;
				// Square == "��"
				case 0x81A0:
					if (draw) {
						drawChar(0x104, x, y, colour);
						drawChar(0x105, x + 8, y, colour);
					}
					x += 16;
					break;
				// Triangle == "��"
				case 0x81A2:
					if (draw) {
						drawChar(0x106, x, y, colour);
						drawChar(0x107, x + 8, y, colour);
					}
					x += 16;
					break;
				// FilledBox == "��"
				case 0x81A1:
					if (draw) {
						drawChar(0x108, x, y, colour);
						drawChar(0x109, x + 8, y, colour);
					}
					x += 16;
					break;
				default:
					if (elisaFnt != NULL) {  // elisa font is available ?
						tmp = y;
						if (code <= 0x829A)
							tmp++;
						// SJIS����EUC�ɕϊ�
						if (code >= 0xE000)
							code -= 0x4000;
						code = ((((code >> 8) & 0xFF) - 0x81) << 9) + (code & 0x00FF);
						if ((code & 0x00FF) >= 0x80)
							code--;
						if ((code & 0x00FF) >= 0x9E)
							code += 0x62;
						else
							code -= 0x40;
						code += 0x2121 + 0x8080;

						// EUC����b�����t�H���g�̔ԍ��𐶐�
						n = (((code >> 8) & 0xFF) - 0xA1) * (0xFF - 0xA1) + (code & 0xFF) - 0xA1;
						j = 0;
						while (font404[j]) {
							if (code >= font404[j]) {
								if (code <= font404[j] + font404[j + 1] - 1) {
									n = -1;
									break;
								} else {
									n -= font404[j + 1];
								}
							}
							j += 2;
						}
						n *= 8;

						if (n >= 0 && n <= 55008) {
							if (draw)
								drawChar2(n, x, tmp, colour);
							x += 9;
						} else {
							if (draw)
								drawChar('_', x, y, colour);
							x += 8;
						}
					} else {  //elisa font is not available
						ascii = 0xFF;
						if (code >> 8 == 0x81)
							ascii = sjis_lookup_81[code & 0x00FF];
						else if (code >> 8 == 0x82)
							ascii = sjis_lookup_82[code & 0x00FF];
						if (ascii != 0xFF) {
							if (draw)
								drawChar((char)ascii, x, y, colour);
						} else {
							if (draw)
								drawChar('_', x, y, colour);
						}
						x += 8;
					}
					break;
			}
		} else {  //First char does not have top bit set or no following char
			if (draw)
				drawChar(s[i], x, y, colour);
			i++;
			x += 8;
		}
		if (x > SCREEN_WIDTH - SCREEN_MARGIN - FONT_WIDTH) {
			//x=16; y=y+8;
			return x;
		}
	}
	return x;
}
//------------------------------
//endfunc printXY_sjis
//--------------------------------------------------------------
//translate a string from shift-JIS to ascii (for gamesave titles)
char *transcpy_sjis(char *d, const unsigned char *s)
{
	u8 ascii;
	u16 code1, code2;
	int i, j;

	for (i = 0, j = 0; s[i];) {
		code1 = s[i++];
		if ((code1 & 0x80) && s[i]) {  //we have top bit and some more char (SJIS) ?
			// SJIS
			code2 = s[i++];
			ascii = 0xFF;
			if (code1 == 0x81)
				ascii = sjis_lookup_81[code2];
			else if (code1 == 0x82)
				ascii = sjis_lookup_82[code2];
			if (ascii != 0xFF) {
				d[j++] = (char)ascii;
			} else {
				d[j++] = '_';
			}
		} else {  //First char lacks top bit set or no following char (non-SJIS)
			d[j++] = (char)code1;
		}
	}             //ends for
	d[j] = '\0';  //terminate result string
	return d;
}
//------------------------------
//endfunc transcpy_sjis
//--------------------------------------------------------------
//WriteFont_C is used to save the current font as C source code
//Comment it out if not used
/*
int	WriteFont_C(char *path_arg)
{
	u8  path[MAX_PATH];
	u8  text[80*2], char_info[80];
	u8	*p;
	int ret, tst, i, fd=-1;

	ret=-1; tst=genFixPath(path_arg, path);
	if(tst < 0) goto finish;
	ret=-2; tst=genOpen(path, FIO_O_CREAT | FIO_O_WRONLY | FIO_O_TRUNC);
	if(tst < 0) goto finish;
	fd = tst;
	sprintf(text, "unsigned char font_uLE[] = {\r\n");
	ret=-3; tst = genWrite(fd, text, strlen(text));
	if(tst != strlen(text)) goto finish;
	for(i=0x000; i<0x10A; i++){
		p = font_uLE + i*16;
		text[0] = '\0';
		if((i & 0x07) == 0)
			sprintf(text, "//Font position 0x%03X\r\n", i);
		if((i < 0x20) || (i>0x80 && i<0xA0))
			sprintf(char_info, "//char 0x%03X == '_' (free for use)", i);
		else if(i < 0x100)
			sprintf(char_info, "//char 0x%03X == '%c'", i, i);
		else //(i > 0x0FF)
			sprintf(char_info, "//char 0x%03X == special for uLE", i);
		sprintf(text+strlen(text),
			"	0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, %s\r\n"
			"	0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X"
			, p[0],p[1], p[2],p[3], p[4],p[5], p[6],p[7], char_info
			, p[8],p[9], p[10],p[11], p[12],p[13], p[14],p[15]
		);
		if(i<0x109) strcat(text, ",");
		strcat(text, "\r\n");
		ret=-4; tst = genWrite(fd, text, strlen(text));
		if(tst != strlen(text)) break;
		ret = 0;
	} //ends for
	if(ret == 0){
		sprintf(text,
			"//Font position 0x%03X\r\n"
			"}; //ends font_uLE\r\n", i);
		ret=-5; tst = genWrite(fd, text, strlen(text));
		if(tst == strlen(text)) ret = 0;
	}
finish:
	if(fd >= 0) genClose(fd);
	sprintf(text,"Saving %s => %d\nChoose either option to continue", path, ret);
	ynDialog(text);
	return ret;
}
*/
//------------------------------
//endfunc WriteFont_C
//--------------------------------------------------------------
//End of file: draw.c
//--------------------------------------------------------------
