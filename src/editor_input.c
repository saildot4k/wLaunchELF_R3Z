//--------------------------------------------------------------
//File name:   editor_input.c
//--------------------------------------------------------------
#include "editor_private.h"

const int WFONTS = VKEY_EDITOR_COLS,  // Virtual KeyBoard Width.
    HFONTS = VKEY_LAYOUT_ROWS;        // Virtual KeyBoard Height.


static const unsigned char EditorSymbolChars[] = {
    0xA1, 0xA3, 0xBF, 0xB0,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8,
    0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1,
    0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD8, 0xD9, 0xDA, 0xDB,
    0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4,
    0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED,
    0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

static void editorPrepareSingleCharInsert(void)
{
	if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
		Editor_Cur -= 1;  //Entry at LF of CRLF must work at CR instead
	}
	if (Editor_Insert || TextBuffer[Active_Window][Editor_Cur] == '\0') {
		ins1 = 1, ins2 = 0, ins3 = 1, ins4 = 0, ins5 = 1;  //Insert char normally
	} else {
		if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\r' && TextBuffer[Active_Window][Editor_Cur + 1] == '\n') {  //OWrite char at CRLF
			ins1 = 0, ins2 = 0, ins3 = 1, ins4 = 2, ins5 = 1;                                                                                          //OWrite at CR of CRLF
		} else {                                                                                                                                       //OWrite return at normal char
			ins1 = 0, ins2 = 0, ins3 = 1, ins4 = 1, ins5 = 1;                                                                                          //OWrite normal char
		}
	}
}

static unsigned char editorSymbolDialog(void)
{
	const int SYMBOL_COLS = 11;
	const int SYMBOL_ROWS = 6;
	const int SYMBOL_COUNT = sizeof(EditorSymbolChars) / sizeof(EditorSymbolChars[0]);
	const int SYMBOL_CELL_W = FONT_WIDTH + 12;
	const int SYMBOL_CELL_H = FONT_HEIGHT + 6;
	const int GRID_W = (SYMBOL_COLS - 1) * SYMBOL_CELL_W + FONT_WIDTH;
	const int GRID_H = (SYMBOL_ROWS - 1) * SYMBOL_CELL_H + FONT_HEIGHT;
	const int BOX_W = GRID_W + 32;
	const int BOX_H = GRID_H + FONT_HEIGHT + 32;
	const int BOX_X = ((SCREEN_WIDTH - BOX_W) / 2) & -2;
	const int BOX_Y = ((SCREEN_HEIGHT - BOX_H) / 2) & -2;
	const int GRID_X = BOX_X + (BOX_W - GRID_W) / 2;
	const int GRID_Y = BOX_Y + LINE_THICKNESS + FONT_HEIGHT + 14;
	int event = 1;
	int post_event = 0;
	int sel = 0;
	int i, x, y;

	while (1) {
		waitPadReady(0, 0);
		if (readpad_no_KB()) {
			if (new_pad)
				event |= 2;

			if (new_pad & PAD_UP) {
				sel = (sel + SYMBOL_COUNT - SYMBOL_COLS) % SYMBOL_COUNT;
			} else if (new_pad & PAD_DOWN) {
				sel = (sel + SYMBOL_COLS) % SYMBOL_COUNT;
			} else if (new_pad & PAD_LEFT) {
				sel = (sel > 0) ? sel - 1 : SYMBOL_COUNT - 1;
			} else if (new_pad & PAD_RIGHT) {
				sel = (sel + 1) % SYMBOL_COUNT;
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				new_pad = 0;
				return EditorSymbolChars[sel];
			} else if (new_pad & PAD_SELECT || new_pad & PAD_TRIANGLE || (!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				new_pad = 0;
				return 0;
			}
		}

		if (event || post_event) {
			drawPopSprite(setting->color[COLOR_BACKGR],
			              BOX_X, BOX_Y,
			              BOX_X + BOX_W - 1, BOX_Y + BOX_H - 1);
			drawFrame(BOX_X, BOX_Y,
			          BOX_X + BOX_W - 1, BOX_Y + BOX_H - 1,
			          setting->color[COLOR_FRAME]);
			printXY(LNG(SPECIAL),
			        BOX_X + (BOX_W - strlen(LNG(SPECIAL)) * FONT_WIDTH) / 2,
			        BOX_Y + LINE_THICKNESS + 4,
			        setting->color[COLOR_TEXT], TRUE, 0);

			for (i = 0; i < SYMBOL_COUNT; i++) {
				x = GRID_X + (i % SYMBOL_COLS) * SYMBOL_CELL_W;
				y = GRID_Y + (i / SYMBOL_COLS) * SYMBOL_CELL_H;
				if (i == sel)
					drawOpSprite(setting->color[COLOR_SELECT],
					             x - 2, y,
					             x + FONT_WIDTH + 1, y + FONT_HEIGHT - 1);
				drawChar(EditorSymbolChars[i], x, y,
				         (i == sel) ? setting->color[COLOR_BACKGR] : setting->color[COLOR_TEXT]);
			}
		}
		drawScr();
		post_event = event;
		event = 0;
	}
}

void editorVirtualKeyboardEntry(void)
{
	int i, Operation;
	unsigned char symbol_char = '\0';

	Operation = 0;

	if (new_pad & PAD_UP) {  // Virtual KeyBoard move up.
		KeyBoard_Cur = getVirtualKeyboardEditorNextKey(setting->virtual_keyboard_layout, KeyBoard_Cur, 0, -1);
		//ends Virtual KeyBoard move up.
	} else if (new_pad & PAD_DOWN) {  // Virtual KeyBoard move down.
		KeyBoard_Cur = getVirtualKeyboardEditorNextKey(setting->virtual_keyboard_layout, KeyBoard_Cur, 0, 1);
		//ends Virtual KeyBoard move down.
	} else if (new_pad & PAD_LEFT) {  // Virtual KeyBoard move left.
		KeyBoard_Cur = getVirtualKeyboardEditorNextKey(setting->virtual_keyboard_layout, KeyBoard_Cur, -1, 0);
		//ends Virtual KeyBoard move left.
	} else if (new_pad & PAD_RIGHT) {  // Virtual KeyBoard move right.
		KeyBoard_Cur = getVirtualKeyboardEditorNextKey(setting->virtual_keyboard_layout, KeyBoard_Cur, 1, 0);
		//ends Virtual KeyBoard move right.
	} else if (new_pad & PAD_L2) {  // Text move left.
		if (Editor_Cur > 0)
			Editor_Cur--;
	} else if (new_pad & PAD_R2) {  // Text move right.
		if (Editor_Cur < Editor_nChar && TextBuffer[Active_Window][Editor_Cur] != '\0')
			Editor_Cur++;
	} else if (new_pad & PAD_SELECT) {  // Virtual KeyBoard Exit.
		Rows_Num += 6;
		KeyBoard_Active = 0;
	} else if (new_pad & PAD_SQUARE) {  // Virtual KeyBoard CAPS.
		KeyBoard_Caps = !KeyBoard_Caps;
	} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {  // Virtual KeyBoard Backspace
		if (Editor_Cur > 0) {
			if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
				Editor_Cur += 1;  //Backspace at LF of CRLF must work after LF instead
			}
			if (Mark[MARK_ON]) {
				Mark[MARK_OUT] = Editor_Cur;
				if (Mark[MARK_OUT] < Mark[MARK_IN]) {
					Mark[MARK_TMP] = Mark[MARK_IN];
					Mark[MARK_IN] = Mark[MARK_OUT];
					Mark[MARK_OUT] = Mark[MARK_TMP];
				} else if (Mark[MARK_IN] == Mark[MARK_OUT])
					goto abort;
				Mark[MARK_SIZE] = Mark[MARK_OUT] - Mark[MARK_IN];
				del1 = -Mark[MARK_SIZE], del2 = 0, del3 = -Mark[MARK_SIZE], del4 = -Mark[MARK_SIZE];
				Mark[MARK_ON] = 0;
			} else if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur - 1] == '\n' && Editor_Cur > 1 && TextBuffer[Active_Window][Editor_Cur - 2] == '\r') {
				del1 = -2, del2 = 0, del3 = -2, del4 = -2;  //Backspace CRLF
			} else {
				del1 = -1, del2 = 0, del3 = -1, del4 = -1;  //Backspace single char
			}
			Operation = -1;
		}
		//ends Virtual KeyBoard Backspace
	} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {  // Virtual KeyBoard Select.
		if (!KeyBoard_Cur) {                                                                // Virtual KeyBoard MARK.
			Mark[MARK_ON] = !Mark[MARK_ON];
			if (Mark[MARK_ON]) {
				if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
					Editor_Cur -= 1;  //Marking at LF of CRLF must start at CR instead
				}
				if (Mark[MARK_COPY] || Mark[MARK_CUT])
					free(TextBuffer[EDIT]);
				Mark[MARK_ON] = 1, Mark[MARK_COPY] = 0, Mark[MARK_CUT] = 0,
				Mark[MARK_IN] = 0, Mark[MARK_OUT] = 0, Mark[MARK_TMP] = 0,
				Mark[MARK_SIZE] = 0, Mark[MARK_PRINT] = 0, Mark[MARK_COLOR] = 0;
				Mark[MARK_IN] = Mark[MARK_OUT] = Editor_Cur;
			}
			Mark[MARK_START] = 1;
			//ends Virtual KeyBoard MARK.
		} else if (KeyBoard_Cur == WFONTS) {  // Virtual KeyBoard COPY.
			if (Mark[MARK_ON]) {
				if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
					Editor_Cur += 1;  //Mark end at LF of CRLF must include LF as well
				}
				Mark[MARK_OUT] = Editor_Cur;
				if (Mark[MARK_OUT] < Mark[MARK_IN]) {
					Mark[MARK_TMP] = Mark[MARK_IN];
					Mark[MARK_IN] = Mark[MARK_OUT];
					Mark[MARK_OUT] = Mark[MARK_TMP];
				} else if (Mark[MARK_IN] == Mark[MARK_OUT])
					goto abort;
				if (Mark[MARK_COPY] || Mark[MARK_CUT])
					free(TextBuffer[EDIT]);
				Mark[MARK_SIZE] = Mark[MARK_OUT] - Mark[MARK_IN];
				TextBuffer[EDIT] = malloc(Mark[MARK_SIZE] + 256);  // 256 To Avoid Crash 256???
				for (i = 0; i < Mark[MARK_SIZE]; i++)
					TextBuffer[EDIT][i] = TextBuffer[Active_Window][i + Mark[MARK_IN]];
				Mark[MARK_COPY] = 1, Mark[MARK_CUT] = 0, Mark[MARK_ON] = 0;
			}
			//ends Virtual KeyBoard COPY.
		} else if (KeyBoard_Cur == 2 * WFONTS) {  // Virtual KeyBoard CUT.
			if (Mark[MARK_ON]) {
				if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
					Editor_Cur += 1;  //Mark end at LF of CRLF must include LF as well
				}
				Mark[MARK_OUT] = Editor_Cur;
				if (Mark[MARK_OUT] < Mark[MARK_IN]) {
					Mark[MARK_TMP] = Mark[MARK_IN];
					Mark[MARK_IN] = Mark[MARK_OUT];
					Mark[MARK_OUT] = Mark[MARK_TMP];
				} else if (Mark[MARK_IN] == Mark[MARK_OUT])
					goto abort;
				if (Mark[MARK_COPY] || Mark[MARK_CUT])
					free(TextBuffer[EDIT]);
				Mark[MARK_SIZE] = Mark[MARK_OUT] - Mark[MARK_IN];
				TextBuffer[EDIT] = malloc(Mark[MARK_SIZE] + 256);  // 256 To Avoid Crash 256???
				for (i = 0; i < Mark[MARK_SIZE]; i++)
					TextBuffer[EDIT][i] = TextBuffer[Active_Window][i + Mark[MARK_IN]];
				del1 = -Mark[MARK_SIZE], del2 = 0, del3 = -Mark[MARK_SIZE], del4 = -Mark[MARK_SIZE];
				Mark[MARK_CUT] = 1, Mark[MARK_COPY] = 0, Mark[MARK_ON] = 0;
				Operation = -1;
			}
		abort:
			Mark[MARK_TMP] = 0;                   // just for compiler warning.
			                                      //ends Virtual KeyBoard CUT.
		} else if (KeyBoard_Cur == 3 * WFONTS) {  // Virtual KeyBoard PASTE.
			if (Mark[MARK_COPY] || Mark[MARK_CUT]) {
				if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n') {
					Editor_Cur -= 1;
					Mark[MARK_SIZE] -= 1;
				}
				ins1 = Mark[MARK_SIZE], ins2 = Mark[MARK_SIZE], ins3 = Mark[MARK_SIZE], ins4 = 0, ins5 = Mark[MARK_SIZE];
				Operation = 1;
			}
			//ends Virtual KeyBoard PASTE.
		} else if (KeyBoard_Cur == 4 * WFONTS) {  // Virtual KeyBoard HOME.
			Editor_Home = 1;
		} else if (KeyBoard_Cur == 1) {  // Virtual KeyBoard LINE UP.
			if (Editor_Cur > 0)
				Editor_PushRows++;
		} else if (KeyBoard_Cur == WFONTS + 1) {  // Virtual KeyBoard LINE DOWN.
			if (Editor_Cur < Editor_nChar)
				Editor_PushRows--;
		} else if (KeyBoard_Cur == 2 * WFONTS + 1) {  // Virtual KeyBoard PAGE UP.
			if (Editor_Cur > 0)
				Editor_PushRows += 1 * (Rows_Num - 1);
		} else if (KeyBoard_Cur == 3 * WFONTS + 1) {  // Virtual KeyBoard PAGE DOWN.
			if (Editor_Cur < Editor_nChar && TextBuffer[Active_Window][Editor_Cur] != '\0')
				Editor_PushRows -= 1 * (Rows_Num - 1);
		} else if (KeyBoard_Cur == 4 * WFONTS + 1) {  // Virtual KeyBoard END.
			Editor_End = 1;
		} else if (KeyBoard_Cur == WFONTS - 1) {  // Virtual KeyBoard INSERT.
			Editor_Insert = !Editor_Insert;
		} else if (KeyBoard_Cur == 2 * WFONTS - 1) {  // Virtual KeyBoard Return Mode CR/LF, LF, CR.
			if ((Editor_RetMode++) >= 4)
				Editor_RetMode = OTHER;
		} else if (KeyBoard_Cur == 4 * WFONTS - 1) {  // Virtual KeyBoard symbol picker.
			symbol_char = editorSymbolDialog();
			if (symbol_char != '\0') {
				editorPrepareSingleCharInsert();
				Operation = 6;
			}
		} else if (KeyBoard_Cur == 5 * WFONTS - 1) {  // Virtual KeyBoard RETURN.

			if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
				Editor_Cur -= 1;  //Entry at LF of CRLF must work at CR instead
			}
			if (Editor_Insert || TextBuffer[Active_Window][Editor_Cur] == '\0')
				if (Editor_RetMode == OTHER)
					ins1 = 2, ins2 = 0, ins3 = 2, ins4 = 0, ins5 = 2;  //Insert CRLF
				else
					ins1 = 1, ins2 = 0, ins3 = 1, ins4 = 0, ins5 = 1;                                                                                           //Insert LF/CR
			else if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\r' && TextBuffer[Active_Window][Editor_Cur + 1] == '\n') {  //OWrite Return at CRLF
				if (Editor_RetMode == OTHER)
					ins1 = 0, ins2 = 0, ins3 = 2, ins4 = 2, ins5 = 2;  //OWrite CRLF at CRLF
				else
					ins1 = 0, ins2 = 0, ins3 = 1, ins4 = 2, ins5 = 1;  //OWrite LF/CR at CRLF
			} else {                                                   //OWrite return at normal char
				if (Editor_RetMode == OTHER)
					ins1 = 1, ins2 = 0, ins3 = 2, ins4 = 1, ins5 = 2;  //OWrite CRLF at char
				else
					ins1 = 0, ins2 = 0, ins3 = 1, ins4 = 1, ins5 = 1;  //OWrite LF/CR at char
			}
			Operation = 2;
			//ends Virtual KeyBoard RETURN.
		} else {  // Virtual KeyBoard Any other char + Tabulation.
			editorPrepareSingleCharInsert();
			if (KeyBoard_Cur == 3 * WFONTS - 1)  // Tabulation.
				Operation = 3;
			else  // Any other char.
				Operation = 5;
		}
		//ends Virtual KeyBoard Select.
	}

	if (Operation > 0) {                                                 // Perform Add Char / Paste. Can Be Simplify???
		TextBuffer[TMP] = malloc(TextSize[Active_Window] + ins1 + 256);  // 256 To Avoid Crash 256???
		strcpy(TextBuffer[TMP], TextBuffer[Active_Window]);
		//memset(TextBuffer[Active_Window], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???		free(TextBuffer[Active_Window]);
		TextBuffer[Active_Window] = malloc(TextSize[Active_Window] + ins1 + 256);  // 256 To Avoid Crash 256???
		strcpy(TextBuffer[Active_Window], TextBuffer[TMP]);
	}

	switch (Operation) {
		case 0:
			break;
		case -1:                                                      // Perform Del Char / Cut. Can Be Simplify???
			TextBuffer[TMP] = malloc(TextSize[Active_Window] + 256);  // 256 To Avoid Crash 256???
			strcpy(TextBuffer[TMP], TextBuffer[Active_Window]);
			TextBuffer[Active_Window][Editor_Cur + del1] = '\0';
			strcat(TextBuffer[Active_Window], TextBuffer[TMP] + (Editor_Cur + del2));
			strcpy(TextBuffer[TMP], TextBuffer[Active_Window]);
			//memset(TextBuffer[Active_Window], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
			free(TextBuffer[Active_Window]);
			TextBuffer[Active_Window] = malloc(TextSize[Active_Window] + del3 + 256);  // 256 To Avoid Crash 256???
			strcpy(TextBuffer[Active_Window], TextBuffer[TMP]);
			//memset(TextBuffer[TMP], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
			free(TextBuffer[TMP]);
			Editor_Cur += del3, TextSize[Active_Window] += del4;
			t = 0;
			break;
		case 1:  // Paste.
			for (i = 0; i < ins2; i++)
				TextBuffer[Active_Window][i + Editor_Cur] = TextBuffer[EDIT][i];
			goto common;
		case 2:  // Return.
			if (Editor_RetMode == OTHER) {
				TextBuffer[Active_Window][Editor_Cur + ins2] = '\r';
				TextBuffer[Active_Window][Editor_Cur + ins2 + 1] = '\n';
			} else if (Editor_RetMode == UNIX) {
				TextBuffer[Active_Window][Editor_Cur + ins2] = '\r';
			} else if (Editor_RetMode == MAC) {
				TextBuffer[Active_Window][Editor_Cur + ins2] = '\n';
			}
			goto common;
		case 3:  // Tabulation.
			TextBuffer[Active_Window][Editor_Cur + ins2] = '\t';
			goto common;
		case 5:  // Any Char.
			TextBuffer[Active_Window][Editor_Cur + ins2] = getVirtualKeyboardEditorChar(setting->virtual_keyboard_layout, KeyBoard_Cur, KeyBoard_Caps);
			goto common;
		case 6:  // Symbol picker char.
			TextBuffer[Active_Window][Editor_Cur + ins2] = (char)symbol_char;
			goto common;
		common:
			TextBuffer[Active_Window][Editor_Cur + ins3] = '\0';
			strcat(TextBuffer[Active_Window], TextBuffer[TMP] + (Editor_Cur + ins4));
			//memset(TextBuffer[TMP], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
			free(TextBuffer[TMP]);
			Editor_Cur += ins5, TextSize[Active_Window] += ins1;
			t = 0;
			break;
	}
}
int editorKeyboardEntry(void)
{
	int i, ret = 0, Operation;
	char KeyPress;

	Operation = 0;

	if (setting->usbkbd_used && ensureUsbKeyboardReady() && PS2KbdRead(&KeyPress)) {  //KeyBoard Response Section.

		ret = 1;  // Equal To event |= pad command.

		if (KeyPress == PS2KBD_ESCAPE_KEY) {
			PS2KbdRead(&KeyPress);
			if (KeyPress)
				t = 0;
			if (KeyPress == 0x29) {  // Key Right.
				if (Editor_Cur < Editor_nChar && TextBuffer[Active_Window][Editor_Cur] != '\0')
					Editor_Cur++;
			} else if (KeyPress == 0x2A) {  // Key Left.
				if (Editor_Cur > 0)
					Editor_Cur--;
			} else if (KeyPress == 0x2C) {  // Key Up.
				if (Editor_Cur > 0)
					Editor_PushRows++;
				else if (Editor_nChar > 0) {
					Editor_Cur = Editor_nChar - 1;
					if (Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur] == '\0')
						Editor_Cur--;
					Editor_PushRows = 0;
				}
			} else if (KeyPress == 0x2B) {  // Key Down.
				if (Editor_Cur < Editor_nChar && TextBuffer[Active_Window][Editor_Cur] != '\0')
					Editor_PushRows--;
				else {
					Editor_Cur = 0;
					Editor_PushRows = 0;
				}
			} else if (KeyPress == 0x24)  // Key Home.
				Editor_Home = 1;
			else if (KeyPress == 0x27)  // Key End.
				Editor_End = 1;
			else if (KeyPress == 0x25) {  // Key PgUp.
				if (Editor_Cur > 0)
					Editor_PushRows += 1 * (Rows_Num - 1);
			} else if (KeyPress == 0x28) {  // Key PgDn.
				if (Editor_Cur < Editor_nChar && TextBuffer[Active_Window][Editor_Cur] != '\0')
					Editor_PushRows -= 1 * (Rows_Num - 1);
			} else if (KeyPress == 0x23)  // Key Insert.
				Editor_Insert = !Editor_Insert;
			else if (KeyPress == 0x26) {  // Key Delete.
				if (Editor_Cur < Editor_nChar) {
					if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
						Editor_Cur -= 1;  //Delete at LF of CRLF must work at CR instead
					}
					if (Mark[MARK_ON]) {
						Mark[MARK_OUT] = Editor_Cur;
						if (Mark[MARK_OUT] < Mark[MARK_IN]) {
							Mark[MARK_TMP] = Mark[MARK_IN];
							Mark[MARK_IN] = Mark[MARK_OUT];
							Mark[MARK_OUT] = Mark[MARK_TMP];
						} else if (Mark[MARK_IN] == Mark[MARK_OUT])
							goto abort;
						Mark[MARK_SIZE] = Mark[MARK_OUT] - Mark[MARK_IN];
						del1 = -Mark[MARK_SIZE], del2 = 0, del3 = -Mark[MARK_SIZE], del4 = -Mark[MARK_SIZE];
						Mark[MARK_ON] = 0;
					} else if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\r' && TextBuffer[Active_Window][Editor_Cur + 1] == '\n') {  //Delete at CRLF
						del1 = 0, del2 = 2, del3 = 0, del4 = -2;                                                                                                          //delete CRLF
					} else if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n') {
						del1 = -1, del2 = 1, del3 = -1, del4 = -2;
					} else {
						del1 = 0, del2 = 1, del3 = 0, del4 = -1;  //delete single char
					}
					Operation = -1;
				}
			} else if (KeyPress == 0x01) {  // Key F1 MENU.
				ret = 2;
			} else if (KeyPress == 0x1B) {  // Key Escape EXIT Editor.
				ret = 3;
			}
		} else {
			if (KeyPress == 0x12) {  // Key Ctrl+r Return Mode CR/LF Or LF Or CR.
				if ((Editor_RetMode++) >= 4)
					Editor_RetMode = OTHER;
			} else if (KeyPress == 0x02) {  // Key Ctrl+b MARK.
				Mark[MARK_ON] = !Mark[MARK_ON];
				if (Mark[MARK_ON]) {
					if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
						Editor_Cur -= 1;  //Marking at LF of CRLF must start at CR instead
					}
					if (Mark[MARK_COPY] || Mark[MARK_CUT])
						free(TextBuffer[EDIT]);
					Mark[MARK_ON] = 1, Mark[MARK_COPY] = 0, Mark[MARK_CUT] = 0,
					Mark[MARK_IN] = 0, Mark[MARK_OUT] = 0, Mark[MARK_TMP] = 0,
					Mark[MARK_SIZE] = 0, Mark[MARK_PRINT] = 0, Mark[MARK_COLOR] = 0;
					Mark[MARK_IN] = Mark[MARK_OUT] = Editor_Cur;
				}
				Mark[MARK_START] = 1;
				//ends Key Ctrl+b MARK.
			} else if (KeyPress == 0x03) {  // Key Ctrl+c COPY.
				if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
					Editor_Cur += 1;  //Mark end at LF of CRLF must include LF as well
				}
				if (Mark[MARK_ON]) {
					Mark[MARK_OUT] = Editor_Cur;
					if (Mark[MARK_OUT] < Mark[MARK_IN]) {
						Mark[MARK_TMP] = Mark[MARK_IN];
						Mark[MARK_IN] = Mark[MARK_OUT];
						Mark[MARK_OUT] = Mark[MARK_TMP];
					} else if (Mark[MARK_IN] == Mark[MARK_OUT])
						goto abort;
					if (Mark[MARK_COPY] || Mark[MARK_CUT])
						free(TextBuffer[EDIT]);
					Mark[MARK_SIZE] = Mark[MARK_OUT] - Mark[MARK_IN];
					TextBuffer[EDIT] = malloc(Mark[MARK_SIZE] + 256);  // 256 To Avoid Crash 256???
					for (i = 0; i < Mark[MARK_SIZE]; i++)
						TextBuffer[EDIT][i] = TextBuffer[Active_Window][i + Mark[MARK_IN]];
					Mark[MARK_COPY] = 1, Mark[MARK_CUT] = 0, Mark[MARK_ON] = 0, Mark[MARK_TMP] = 0;
				}
				//ends Key Ctrl+c COPY.
			} else if (KeyPress == 0x18) {  // Key Ctrl+x CUT.
				if (Mark[MARK_ON]) {
					if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
						Editor_Cur += 1;  //Mark end at LF of CRLF must include LF as well
					}
					Mark[MARK_OUT] = Editor_Cur;
					if (Mark[MARK_OUT] < Mark[MARK_IN]) {
						Mark[MARK_TMP] = Mark[MARK_IN];
						Mark[MARK_IN] = Mark[MARK_OUT];
						Mark[MARK_OUT] = Mark[MARK_TMP];
					} else if (Mark[MARK_IN] == Mark[MARK_OUT])
						goto abort;
					if (Mark[MARK_COPY] || Mark[MARK_CUT])
						free(TextBuffer[EDIT]);
					Mark[MARK_SIZE] = Mark[MARK_OUT] - Mark[MARK_IN];
					TextBuffer[EDIT] = malloc(Mark[MARK_SIZE] + 256);  // 256 To Avoid Crash 256???
					for (i = 0; i < Mark[MARK_SIZE]; i++)
						TextBuffer[EDIT][i] = TextBuffer[Active_Window][i + Mark[MARK_IN]];
					del1 = -Mark[MARK_SIZE], del2 = 0, del3 = -Mark[MARK_SIZE], del4 = -Mark[MARK_SIZE];
					Mark[MARK_CUT] = 1, Mark[MARK_COPY] = 0, Mark[MARK_ON] = 0, Mark[MARK_TMP] = 0;
					Operation = -2;
				}
				//ends Key Ctrl+x CUT.
			} else if (KeyPress == 0x16) {  // Key Ctrl+v PASTE.
				if (Mark[MARK_COPY] || Mark[MARK_CUT]) {
					if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n') {
						Editor_Cur -= 1;
						Mark[MARK_SIZE] -= 1;
					}
					ins1 = Mark[MARK_SIZE], ins2 = Mark[MARK_SIZE], ins3 = Mark[MARK_SIZE], ins4 = 0, ins5 = Mark[MARK_SIZE];
					Operation = 1;
				}
				//ends Key Ctrl+v PASTE.
			} else if (KeyPress == 0x07) {  // Key BackSpace.
				if (Editor_Cur > 0) {
					if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
						Editor_Cur += 1;  //Backspace at LF of CRLF must work after LF
					}
					if (Mark[MARK_ON]) {
						Mark[MARK_OUT] = Editor_Cur;
						if (Mark[MARK_OUT] < Mark[MARK_IN]) {
							Mark[MARK_TMP] = Mark[MARK_IN];
							Mark[MARK_IN] = Mark[MARK_OUT];
							Mark[MARK_OUT] = Mark[MARK_TMP];
						} else if (Mark[MARK_IN] == Mark[MARK_OUT])
							goto abort;
						Mark[MARK_SIZE] = Mark[MARK_OUT] - Mark[MARK_IN];
						del1 = -Mark[MARK_SIZE], del2 = 0, del3 = -Mark[MARK_SIZE], del4 = -Mark[MARK_SIZE];
						Mark[MARK_ON] = 0;
					} else if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur - 1] == '\n' && Editor_Cur > 1 && TextBuffer[Active_Window][Editor_Cur - 2] == '\r') {
						del1 = -2, del2 = 0, del3 = -2, del4 = -2;  //Backspace CRLF
					} else {
						del1 = -1, del2 = 0, del3 = -1, del4 = -1;  //Backspace single char
					}
					Operation = -3;
				}
				//ends Key BackSpace.
			} else if (KeyPress == 0x0A) {  // Key Return.

				if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
					Editor_Cur -= 1;  //Entry at LF of CRLF must work at CR instead
				}
				if (Editor_Insert || TextBuffer[Active_Window][Editor_Cur] == '\0')
					if (Editor_RetMode == OTHER)
						ins1 = 2, ins2 = 0, ins3 = 2, ins4 = 0, ins5 = 2;  //Insert CRLF
					else
						ins1 = 1, ins2 = 0, ins3 = 1, ins4 = 0, ins5 = 1;                                                                                           //Insert LF/CR
				else if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\r' && TextBuffer[Active_Window][Editor_Cur + 1] == '\n') {  //OWrite Return at CRLF
					if (Editor_RetMode == OTHER)
						ins1 = 0, ins2 = 0, ins3 = 2, ins4 = 2, ins5 = 2;  //OWrite CRLF at CRLF
					else
						ins1 = 0, ins2 = 0, ins3 = 1, ins4 = 2, ins5 = 1;  //OWrite LF/CR at CRLF
				} else {                                                   //OWrite return at normal char
					if (Editor_RetMode == OTHER)
						ins1 = 1, ins2 = 0, ins3 = 2, ins4 = 1, ins5 = 2;  //OWrite CRLF at char
					else
						ins1 = 0, ins2 = 0, ins3 = 1, ins4 = 1, ins5 = 1;  //OWrite LF/CR at char
				}
				Operation = 2;
				//ends Key Return.
			} else {  // All Other Keys.
				if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\n' && Editor_Cur > 0 && TextBuffer[Active_Window][Editor_Cur - 1] == '\r') {
					Editor_Cur -= 1;  //Entry at LF of CRLF must work at CR instead
				}
				if (Editor_Insert || TextBuffer[Active_Window][Editor_Cur] == '\0') {
					ins1 = 1, ins2 = 0, ins3 = 1, ins4 = 0, ins5 = 1;  //Insert char normally
				} else {
					if (TextMode[Active_Window] == OTHER && TextBuffer[Active_Window][Editor_Cur] == '\r' && TextBuffer[Active_Window][Editor_Cur + 1] == '\n') {  //OWrite char at CRLF
						ins1 = 0, ins2 = 0, ins3 = 1, ins4 = 2, ins5 = 1;                                                                                          //OWrite at CR of CRLF
					} else {                                                                                                                                       //OWrite return at normal char
						ins1 = 0, ins2 = 0, ins3 = 1, ins4 = 1, ins5 = 1;                                                                                          //OWrite normal char
					}
				}
				Operation = 3;
			}
		}

		if (Operation > 0) {                                                 // Perform Add Char / Paste. Can Be Simplify???
			TextBuffer[TMP] = malloc(TextSize[Active_Window] + ins1 + 256);  // 256 To Avoid Crash 256???
			strcpy(TextBuffer[TMP], TextBuffer[Active_Window]);
			//memset(TextBuffer[Active_Window], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???		free(TextBuffer[Active_Window]);
			TextBuffer[Active_Window] = malloc(TextSize[Active_Window] + ins1 + 256);  // 256 To Avoid Crash 256???
			strcpy(TextBuffer[Active_Window], TextBuffer[TMP]);
		}

		switch (Operation) {
			case 0:
				break;
			case -1:  // Perform Del Char / Cut. Can Be Simplify???
			case -2:
			case -3:
				TextBuffer[TMP] = malloc(TextSize[Active_Window] + 256);  // 256 To Avoid Crash 256???
				strcpy(TextBuffer[TMP], TextBuffer[Active_Window]);
				TextBuffer[Active_Window][Editor_Cur + del1] = '\0';
				strcat(TextBuffer[Active_Window], TextBuffer[TMP] + (Editor_Cur + del2));
				strcpy(TextBuffer[TMP], TextBuffer[Active_Window]);
				//memset(TextBuffer[Active_Window], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
				free(TextBuffer[Active_Window]);
				TextBuffer[Active_Window] = malloc(TextSize[Active_Window] + del3 + 256);  // 256 To Avoid Crash 256???
				strcpy(TextBuffer[Active_Window], TextBuffer[TMP]);
				//memset(TextBuffer[TMP], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
				free(TextBuffer[TMP]);
				Editor_Cur += del3, TextSize[Active_Window] += del4;
				t = 0;
				break;
			case 1:  // Paste.
				for (i = 0; i < ins2; i++)
					TextBuffer[Active_Window][i + Editor_Cur] = TextBuffer[EDIT][i];
				goto common;
			case 2:  // Return.
				if (Editor_RetMode == OTHER) {
					TextBuffer[Active_Window][Editor_Cur + ins2] = '\r';
					TextBuffer[Active_Window][Editor_Cur + ins2 + 1] = '\n';
				} else if (Editor_RetMode == UNIX) {
					TextBuffer[Active_Window][Editor_Cur + ins2] = '\r';
				} else if (Editor_RetMode == MAC) {
					TextBuffer[Active_Window][Editor_Cur + ins2] = '\n';
				}
				goto common;
			case 3:  // Normal characters
				TextBuffer[Active_Window][Editor_Cur + ins2] = KeyPress;
			common:
				TextBuffer[Active_Window][Editor_Cur + ins3] = '\0';
				strcat(TextBuffer[Active_Window], TextBuffer[TMP] + (Editor_Cur + ins4));
				//memset(TextBuffer[TMP], 0, TextSize[Active_Window]+256); // 256 To Avoid Crash 256???
				free(TextBuffer[TMP]);
				Editor_Cur += ins5, TextSize[Active_Window] += ins1;
				t = 0;
				break;
		}

	abort:
		KeyPress = '\0';
	}  //ends if(PS2KbdRead(&KeyPress)).
	return ret;
}
//--------------------------------------------------------------
//End of file: editor_input.c
//--------------------------------------------------------------
