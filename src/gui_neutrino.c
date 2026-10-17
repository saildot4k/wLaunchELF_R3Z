#include "launchelf.h"
#include "gui_neutrino.h"

static const struct {
	int mode;
	const char *label;
} compatibility_options[] = {
	{0, "Fast disc reads"},
	{2, "Synchronous disc reads"},
	{3, "Unhook system calls"},
	{5, "Emulate dual-layer DVD"},
	{7, "Fix game buffer overrun"}
};

static const struct {
	const char *argument;
	const char *label;
} video_modes[] = {
	{"", "Original (480i / 576i)"},
	{"fp1", "240p / 288p (Auto PAL/NTSC)"},
	{"fp2", "480p / 576p (Auto PAL/NTSC)"},
	{"1080ix1", "1080i (Width x1)"},
	{"1080ix2", "1080i (Width x2)"},
	{"1080ix3", "1080i (Width x3)"}
};

static const char *video_compatibility_modes[] = {
	"Off",
	"Field flipping 1 (GSM/OPL)",
	"Field flipping 2",
	"Field flipping 3"
};

enum {
	NEUTRINO_COMPAT_COUNT = sizeof(compatibility_options) / sizeof(compatibility_options[0]),
	NEUTRINO_VIDEO_COUNT = sizeof(video_modes) / sizeof(video_modes[0]),
	NEUTRINO_VIDEO_COMPAT_COUNT = sizeof(video_compatibility_modes) / sizeof(video_compatibility_modes[0]),
	NEUTRINO_VIDEO_MODE = NEUTRINO_COMPAT_COUNT,
	NEUTRINO_VIDEO_COMPATIBILITY,
	NEUTRINO_DEBUG_COLORS,
	NEUTRINO_LOGO,
	NEUTRINO_RUN,
	NEUTRINO_CANCEL
};

int buildNeutrinoOptionArgs(const NEUTRINO_LAUNCH_OPTIONS *options, NEUTRINO_OPTION_ARGS *args)
{
	unsigned int allowed_modes = 0;
	int i, pos, argc = 0;

	if (options == NULL || args == NULL)
		return -1;
	memset(args, 0, sizeof(*args));
	for (i = 0; i < NEUTRINO_COMPAT_COUNT; i++)
		allowed_modes |= 1u << compatibility_options[i].mode;
	if ((options->game_compatibility & ~allowed_modes) ||
	    options->video_mode < 0 || options->video_mode >= NEUTRINO_VIDEO_COUNT ||
	    options->video_compatibility < 0 || options->video_compatibility >= NEUTRINO_VIDEO_COMPAT_COUNT)
		return -1;

	if (options->game_compatibility) {
		strcpy(args->game_arg, "-gc=");
		pos = 4;
		for (i = 0; i < NEUTRINO_COMPAT_COUNT; i++) {
			if (options->game_compatibility & (1u << compatibility_options[i].mode))
				args->game_arg[pos++] = '0' + compatibility_options[i].mode;
		}
		args->game_arg[pos] = '\0';
		args->argv[argc++] = args->game_arg;
	}
	if (options->video_mode || options->video_compatibility) {
		if (options->video_compatibility)
			snprintf(args->video_arg, sizeof(args->video_arg), "-gsm=%s:%d",
			         video_modes[options->video_mode].argument, options->video_compatibility);
		else
			snprintf(args->video_arg, sizeof(args->video_arg), "-gsm=%s", video_modes[options->video_mode].argument);
		args->argv[argc++] = args->video_arg;
	}
	if (options->debug_colors)
		args->argv[argc++] = "-dbc";
	if (options->logo)
		args->argv[argc++] = "-logo";
	return argc;
}

static int moveNeutrinoSelection(int selection, int direction)
{
	if (direction < 0) {
		if (selection == 0)
			return NEUTRINO_CANCEL;
		if (selection >= NEUTRINO_RUN)
			return selection - 2;
		if (selection >= NEUTRINO_DEBUG_COLORS)
			return NEUTRINO_VIDEO_COMPATIBILITY;
		return selection - 1;
	}
	if (selection >= NEUTRINO_RUN)
		return 0;
	if (selection >= NEUTRINO_DEBUG_COLORS)
		return selection + 2;
	return selection + 1;
}

static void drawNeutrinoHeading(const char *heading, int y)
{
	int width = printXY(heading, 0, 0, 0, FALSE, 0);

	printXY(heading, (SCREEN_WIDTH - width) / 2, y, setting->color[COLOR_TEXT], TRUE, 0);
}

static void drawNeutrinoOption(const char *text, int item, int selection, int x, int y)
{
	u64 color = setting->color[item == selection ? COLOR_SELECT : COLOR_TEXT];

	printXY(text, x, y, color, TRUE, 0);
	if (item == selection)
		drawChar(LEFT_CUR, x - 2 * FONT_WIDTH, y, color);
}

int showNeutrinoLaunchOptions(NEUTRINO_LAUNCH_OPTIONS *options)
{
	char text[128];
	int selection = 0, event = 1, post_event = 0;
	int width = SCREEN_WIDTH - 2 * SCREEN_MARGIN;
	int height = 17 * FONT_HEIGHT;
	int left = (SCREEN_WIDTH - width) / 2;
	int top = (SCREEN_HEIGHT - height) / 2;
	int x = left + 3 * FONT_WIDTH;
	int right_x = left + width / 2 + 3 * FONT_WIDTH;
	int y = top + FONT_HEIGHT / 2;
	int i, direction;

	if (options == NULL)
		return FALSE;
	memset(options, 0, sizeof(*options));
	while (1) {
		waitPadReady(0, 0);
		if (readpad()) {
			if ((new_pad & PAD_TRIANGLE) || (!swapKeys && (new_pad & PAD_CROSS)) ||
			    (swapKeys && (new_pad & PAD_CIRCLE)))
				return FALSE;
			if (new_pad & PAD_UP) {
				selection = moveNeutrinoSelection(selection, -1);
				event = 1;
			} else if (new_pad & PAD_DOWN) {
				selection = moveNeutrinoSelection(selection, 1);
				event = 1;
			} else if (new_pad & (PAD_LEFT | PAD_RIGHT)) {
				direction = (new_pad & PAD_LEFT) ? -1 : 1;
				if (selection == NEUTRINO_VIDEO_MODE)
					options->video_mode = (options->video_mode + direction + NEUTRINO_VIDEO_COUNT) % NEUTRINO_VIDEO_COUNT;
				else if (selection == NEUTRINO_VIDEO_COMPATIBILITY)
					options->video_compatibility = (options->video_compatibility + direction + NEUTRINO_VIDEO_COMPAT_COUNT) % NEUTRINO_VIDEO_COMPAT_COUNT;
				else if (selection == NEUTRINO_DEBUG_COLORS || selection == NEUTRINO_LOGO)
					selection = (selection == NEUTRINO_DEBUG_COLORS) ? NEUTRINO_LOGO : NEUTRINO_DEBUG_COLORS;
				else if (selection == NEUTRINO_RUN || selection == NEUTRINO_CANCEL)
					selection = (selection == NEUTRINO_RUN) ? NEUTRINO_CANCEL : NEUTRINO_RUN;
				event = 1;
			} else if ((swapKeys && (new_pad & PAD_CROSS)) || (!swapKeys && (new_pad & PAD_CIRCLE))) {
				if (selection < NEUTRINO_COMPAT_COUNT)
					options->game_compatibility ^= 1u << compatibility_options[selection].mode;
				else if (selection == NEUTRINO_VIDEO_MODE)
					options->video_mode = (options->video_mode + 1) % NEUTRINO_VIDEO_COUNT;
				else if (selection == NEUTRINO_VIDEO_COMPATIBILITY)
					options->video_compatibility = (options->video_compatibility + 1) % NEUTRINO_VIDEO_COMPAT_COUNT;
				else if (selection == NEUTRINO_DEBUG_COLORS)
					options->debug_colors = !options->debug_colors;
				else if (selection == NEUTRINO_LOGO)
					options->logo = !options->logo;
				else
					return selection == NEUTRINO_RUN;
				event = 1;
			}
		}

		if (event || post_event) {
			clrScr(setting->color[COLOR_BACKGR]);
			setScrTmp("", "");
			drawPopSprite(setting->color[COLOR_BACKGR], left, top, left + width, top + height);
			drawFrame(left, top, left + width, top + height, setting->color[COLOR_FRAME]);
			drawNeutrinoHeading("Game Compatibility", y);
			for (i = 0; i < NEUTRINO_COMPAT_COUNT; i++) {
				snprintf(text, sizeof(text), "[%c] %s",
				         (options->game_compatibility & (1u << compatibility_options[i].mode)) ? 'X' : ' ',
				         compatibility_options[i].label);
				drawNeutrinoOption(text, i, selection, x, y + (i + 1) * FONT_HEIGHT);
			}
			drawNeutrinoHeading("Video Modes", y + 7 * FONT_HEIGHT);
			snprintf(text, sizeof(text), "Video Mode: < %s >", video_modes[options->video_mode].label);
			drawNeutrinoOption(text, NEUTRINO_VIDEO_MODE, selection, x, y + 8 * FONT_HEIGHT);
			snprintf(text, sizeof(text), "Video Compatibility: < %s >", video_compatibility_modes[options->video_compatibility]);
			drawNeutrinoOption(text, NEUTRINO_VIDEO_COMPATIBILITY, selection, x, y + 9 * FONT_HEIGHT);
			snprintf(text, sizeof(text), "Debug Colors: %s", options->debug_colors ? LNG(YES) : LNG(NO));
			drawNeutrinoOption(text, NEUTRINO_DEBUG_COLORS, selection, x, y + 11 * FONT_HEIGHT);
			snprintf(text, sizeof(text), "PS2 Logo: %s", options->logo ? LNG(YES) : LNG(NO));
			drawNeutrinoOption(text, NEUTRINO_LOGO, selection, right_x, y + 11 * FONT_HEIGHT);
			drawNeutrinoOption("Run", NEUTRINO_RUN, selection, x, y + 13 * FONT_HEIGHT);
			drawNeutrinoOption(LNG(Cancel), NEUTRINO_CANCEL, selection, right_x, y + 13 * FONT_HEIGHT);
			snprintf(text, sizeof(text), "D-pad: %s  \xFF" "%c: %s  \xFF" "3: %s",
			         LNG(Select), swapKeys ? '1' : '0', LNG(Change), LNG(Cancel));
			printXY(text, x, y + 15 * FONT_HEIGHT, setting->color[COLOR_TEXT], TRUE, 0);
		}
		drawScr();
		post_event = event;
		event = 0;
	}
}
