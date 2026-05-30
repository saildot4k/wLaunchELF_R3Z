#include "launchelf.h"

int ynDialog(const char *message)
{
	char msg[512];
	int dh, dw, dx, dy;
	int sel = 0, a = 6, b = 4, c = 2, n, tw;
	int i, x, len, ret;
	int event, post_event = 0;

	strcpy(msg, message);

	for (i = 0, n = 1; msg[i] != 0; i++) {
		if (msg[i] == '\n') {
			msg[i] = 0;
			n++;
		}
	}
	for (i = len = tw = 0; i < n; i++) {
		ret = printXY(&msg[len], 0, 0, 0, FALSE, 0);
		if (ret > tw)
			tw = ret;
		len += strlen(&msg[len]) + 1;
	}
	if (tw < 96)
		tw = 96;

	dh = FONT_HEIGHT * (n + 1) + 2 * 2 + a + b + c;
	dw = 2 * 2 + a * 2 + tw;
	dx = (SCREEN_WIDTH - dw) / 2;
	dy = (SCREEN_HEIGHT - dh) / 2;

	event = 1;
	while (1) {
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_LEFT) {
				event |= 2;
				sel = 0;
			} else if (new_pad & PAD_RIGHT) {
				event |= 2;
				sel = 1;
			} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				ret = -1;
				break;
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				ret = (sel == 0) ? 1 : -1;
				break;
			}
		}

		if (event || post_event) {
			drawPopSprite(setting->color[COLOR_BACKGR],
			              dx, dy,
			              dx + dw, (dy + dh));
			drawFrame(dx, dy, dx + dw, (dy + dh), setting->color[COLOR_FRAME]);
			for (i = len = 0; i < n; i++) {
				printXY(&msg[len], dx + 2 + a, (dy + a + 2 + i * 16), setting->color[COLOR_TEXT], TRUE, 0);
				len += strlen(&msg[len]) + 1;
			}

			x = (tw - 96) / 4;
			printXY(LNG(OK), dx + a + x + FONT_WIDTH,
			        (dy + a + b + 2 + n * FONT_HEIGHT), setting->color[COLOR_TEXT], TRUE, 0);
			printXY(LNG(CANCEL), dx + dw - x - (strlen(LNG(CANCEL)) + 1) * FONT_WIDTH,
			        (dy + a + b + 2 + n * FONT_HEIGHT), setting->color[COLOR_TEXT], TRUE, 0);
			if (sel == 0)
				drawChar(LEFT_CUR, dx + a + x, (dy + a + b + 2 + n * FONT_HEIGHT), setting->color[COLOR_TEXT]);
			else
				drawChar(LEFT_CUR, dx + dw - x - (strlen(LNG(CANCEL)) + 2) * FONT_WIDTH - 1,
				         (dy + a + b + 2 + n * FONT_HEIGHT), setting->color[COLOR_TEXT]);
		}
		drawLastMsg();
		post_event = event;
		event = 0;
	}
	drawSprite(setting->color[COLOR_BACKGR], dx, dy, dx + dw + 1, (dy + dh) + 1);
	drawScr();
	drawSprite(setting->color[COLOR_BACKGR], dx, dy, dx + dw + 1, (dy + dh) + 1);
	drawScr();
	return ret;
}

void nonDialog(char *message)
{
	char msg[80 * 30];
	static int dh, dw, dx, dy;
	int a = 6, b = 4, c = 2, n, tw;
	int i, len, ret;

	if (message == NULL) {
		drawSprite(setting->color[COLOR_BACKGR],
		           dx, dy,
		           dx + dw, (dy + dh));
		return;
	}

	strcpy(msg, message);

	for (i = 0, n = 1; msg[i] != 0; i++) {
		if (msg[i] == '\n') {
			msg[i] = 0;
			n++;
		}
	}
	for (i = len = tw = 0; i < n; i++) {
		ret = printXY(&msg[len], 0, 0, 0, FALSE, 0);
		if (ret > tw)
			tw = ret;
		len += strlen(&msg[len]) + 1;
	}
	if (tw < 96)
		tw = 96;

	dh = 16 * n + 2 * 2 + a + b + c;
	dw = 2 * 2 + a * 2 + tw;
	dx = (SCREEN_WIDTH - dw) / 2;
	dy = (SCREEN_HEIGHT - dh) / 2;

	drawPopSprite(setting->color[COLOR_BACKGR],
	              dx, dy,
	              dx + dw, (dy + dh));
	drawFrame(dx, dy, dx + dw, (dy + dh), setting->color[COLOR_FRAME]);
	for (i = len = 0; i < n; i++) {
		printXY(&msg[len], dx + 2 + a, (dy + a + 2 + i * FONT_HEIGHT), setting->color[COLOR_TEXT], TRUE, 0);
		len += strlen(&msg[len]) + 1;
	}
}

// keyboard returns string length, or -1 when cancelled.
int keyboard(char *out, int max)
{
	int event, post_event = 0;
	const int
	    WFONTS = 13,
	    HFONTS = 7,
	    KEY_W = LINE_THICKNESS + 12 + (13 * FONT_WIDTH + 12 * 12) + 12 + LINE_THICKNESS,
	    KEY_H = LINE_THICKNESS + 1 + FONT_HEIGHT + 1 + LINE_THICKNESS + 8 + (8 * FONT_HEIGHT) + 8 + LINE_THICKNESS,
	    KEY_X = ((SCREEN_WIDTH - KEY_W) / 2) & -2,
	    KEY_Y = ((SCREEN_HEIGHT - KEY_H) / 2) & -2;
	char *KEY = "ABCDEFGHIJKLM"
	            "NOPQRSTUVWXYZ"
	            "abcdefghijklm"
	            "nopqrstuvwxyz"
	            "0123456789/|\\"
	            "<>(){}[].,:;\""
	            "!@#$%&=+-^*_'";
	int KEY_LEN, KEY_LAST, KEY_OK, KEY_CANCEL;
	int cur = 0, sel = 0, i = 0, x, y, t = 0;
	char tmp[256], *p;
	char KeyPress;

	p = strrchr(out, '.');
	if (p == NULL)
		cur = strlen(out);
	else
		cur = (int)(p - out);
	KEY_LEN = strlen(KEY);
	KEY_LAST = WFONTS * HFONTS - 1;
	KEY_OK = KEY_LAST + 1;
	KEY_CANCEL = KEY_OK + 1;

	event = 1;
	while (1) {
		waitPadReady(0, 0);
		if (readpad_no_KB()) {
			if (new_pad)
				event |= 2;
			if (new_pad & PAD_UP) {
				if (sel <= KEY_LAST) {
					if (sel >= WFONTS)
						sel -= WFONTS;
					else if (sel < 5)
						sel = KEY_OK;
					else
						sel = KEY_CANCEL;
				} else if (sel == KEY_OK)
					sel = WFONTS * (HFONTS - 1);
				else
					sel = KEY_LAST;
			} else if (new_pad & PAD_DOWN) {
				if (sel <= KEY_LAST) {
					if (sel / WFONTS == HFONTS - 1) {
						if (sel % WFONTS < 5)
							sel = KEY_OK;
						else
							sel = KEY_CANCEL;
					} else if (sel / WFONTS <= HFONTS - 2)
						sel += WFONTS;
				} else if (sel == KEY_OK)
					sel = 0;
				else
					sel = WFONTS - 1;
			} else if (new_pad & PAD_LEFT) {
				if (sel <= KEY_LAST) {
					if (sel % WFONTS == 0)
						sel += WFONTS - 1;
					else
						sel--;
				} else if (sel == KEY_OK)
					sel = KEY_CANCEL;
				else
					sel = KEY_OK;
			} else if (new_pad & PAD_RIGHT) {
				if (sel <= KEY_LAST) {
					if (sel % WFONTS == WFONTS - 1)
						sel -= WFONTS - 1;
					else
						sel++;
				} else if (sel == KEY_OK)
					sel = KEY_CANCEL;
				else
					sel = KEY_OK;
			} else if (new_pad & PAD_START) {
				sel = KEY_OK;
			} else if (new_pad & PAD_L1) {
				if (cur > 0)
					cur--;
				t = 0;
			} else if (new_pad & PAD_R1) {
				if (cur < strlen(out))
					cur++;
				t = 0;
			} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				if (cur > 0) {
					strcpy(tmp, out);
					out[cur - 1] = 0;
					strcat(out, &tmp[cur]);
					cur--;
					t = 0;
				}
			} else if (new_pad & PAD_SQUARE) {
				i = strlen(out);
				if (i < max && i < 33) {
					strcpy(tmp, out);
					out[cur] = ' ';
					out[cur + 1] = 0;
					strcat(out, &tmp[cur]);
					cur++;
					t = 0;
				}
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				i = strlen(out);
				if (sel <= KEY_LAST) {
					if (i < max && i < 33) {
						strcpy(tmp, out);
						out[cur] = KEY[sel];
						out[cur + 1] = 0;
						strcat(out, &tmp[cur]);
						cur++;
						t = 0;
					}
				} else if (sel == KEY_OK) {
					break;
				} else
					return -1;
			} else if (new_pad & PAD_TRIANGLE) {
				return -1;
			}
		}

		if (setting->usbkbd_used && ensureUsbKeyboardReady() && PS2KbdRead(&KeyPress)) {
			event |= 2;

			if (KeyPress == PS2KBD_ESCAPE_KEY) {
				PS2KbdRead(&KeyPress);
				if (KeyPress == 0x29) {
					if (cur < strlen(out))
						cur++;
					t = 0;
				} else if (KeyPress == 0x2a) {
					if (cur > 0)
						cur--;
					t = 0;
				} else if (KeyPress == 0x24) {
					cur = 0;
					t = 0;
				} else if (KeyPress == 0x27) {
					cur = strlen(out);
					t = 0;
				} else if (KeyPress == 0x26) {
					if (strlen(out) > cur) {
						strcpy(tmp, out);
						out[cur] = 0;
						strcat(out, &tmp[cur + 1]);
						t = 0;
					}
				} else if (KeyPress == 0x1b) {
					return -1;
				}
			} else {
				if (KeyPress == 0x07) {
					if (cur > 0) {
						strcpy(tmp, out);
						out[cur - 1] = 0;
						strcat(out, &tmp[cur]);
						cur--;
						t = 0;
					}
				} else if (KeyPress == 0x0a) {
					break;
				} else {
					i = strlen(out);
					if (i < max && i < 33) {
						strcpy(tmp, out);
						out[cur] = KeyPress;
						out[cur + 1] = 0;
						strcat(out, &tmp[cur]);
						cur++;
						t = 0;
					}
				}
			}
			KeyPress = '\0';
		}

		t++;
		if (t & 0x0F)
			event |= 4;

		if (event || post_event) {
			drawPopSprite(setting->color[COLOR_BACKGR],
			              KEY_X, KEY_Y,
			              KEY_X + KEY_W - 1, KEY_Y + KEY_H - 1);
			drawFrame(
			    KEY_X, KEY_Y,
			    KEY_X + KEY_W - 1, KEY_Y + KEY_H - 1, setting->color[COLOR_FRAME]);
			drawOpSprite(setting->color[COLOR_FRAME],
			             KEY_X, KEY_Y + LINE_THICKNESS + 1 + FONT_HEIGHT + 1,
			             KEY_X + KEY_W - 1, KEY_Y + LINE_THICKNESS + 1 + FONT_HEIGHT + 1 + LINE_THICKNESS - 1);
			printXY(out, KEY_X + LINE_THICKNESS + 3, KEY_Y + LINE_THICKNESS + 1, setting->color[COLOR_TEXT], TRUE, 0);
			if (((event | post_event) & 4) && (t & 0x10)) {
				drawOpSprite(setting->color[COLOR_SELECT],
				             KEY_X + LINE_THICKNESS + 1 + cur * 8,
				             KEY_Y + LINE_THICKNESS + 2,
				             KEY_X + LINE_THICKNESS + 1 + cur * 8 + LINE_THICKNESS - 1,
				             KEY_Y + LINE_THICKNESS + 2 + (FONT_HEIGHT - 2) - 1);
			}
			for (i = 0; i < KEY_LEN; i++)
				drawChar(KEY[i],
				         KEY_X + LINE_THICKNESS + 12 + (i % WFONTS) * (FONT_WIDTH + 12),
				         KEY_Y + LINE_THICKNESS + 1 + FONT_HEIGHT + 1 + LINE_THICKNESS + 8 + (i / WFONTS) * FONT_HEIGHT, setting->color[COLOR_TEXT]);
			printXY(LNG(OK),
			        KEY_X + LINE_THICKNESS + 12,
			        KEY_Y + LINE_THICKNESS + 1 + FONT_HEIGHT + 1 + LINE_THICKNESS + 8 + HFONTS * FONT_HEIGHT, setting->color[COLOR_TEXT], TRUE, 0);
			printXY(LNG(CANCEL),
			        KEY_X + KEY_W - 1 - (strlen(LNG(CANCEL)) + 2) * FONT_WIDTH,
			        KEY_Y + LINE_THICKNESS + 1 + FONT_HEIGHT + 1 + LINE_THICKNESS + 8 + HFONTS * FONT_HEIGHT, setting->color[COLOR_TEXT], TRUE, 0);

			if (sel <= KEY_OK)
				x = KEY_X + LINE_THICKNESS + 12 + (sel % WFONTS) * (FONT_WIDTH + 12) - 8;
			else
				x = KEY_X + KEY_W - 2 - (strlen(LNG(CANCEL)) + 3) * FONT_WIDTH;
			y = KEY_Y + LINE_THICKNESS + 1 + FONT_HEIGHT + 1 + LINE_THICKNESS + 8 + (sel / WFONTS) * FONT_HEIGHT;
			drawChar(LEFT_CUR, x, y, setting->color[COLOR_SELECT]);

			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[COLOR_BACKGR], 0, y - 1, SCREEN_WIDTH, y + FONT_HEIGHT);

			if (swapKeys) {
				sprintf(tmp, "\xFF"
				             "1:%s \xFF"
				             "0",
				        LNG(Use));
			} else {
				sprintf(tmp, "\xFF"
				             "0:%s \xFF"
				             "1",
				        LNG(Use));
			}
			sprintf(tmp + strlen(tmp), ":%s \xFF"
			                           "2:%s L1:%s R1:%s START:%s \xFF"
			                           "3:%s",
			        LNG(BackSpace), LNG(SPACE), LNG(Left), LNG(Right), LNG(Enter), LNG(Exit));
			printXY(tmp, x, y, setting->color[COLOR_SELECT], TRUE, 0);
		}
		drawScr();
		post_event = event;
		event = 0;
	}
	return strlen(out);
}
