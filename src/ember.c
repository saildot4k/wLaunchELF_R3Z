#include "launchelf.h"

static int emberSetMessage(char *message, size_t message_size, const char *format, const char *path)
{
	if (message != NULL && message_size > 0) {
		snprintf(message, message_size, format, (path != NULL) ? path : "");
		message[message_size - 1] = '\0';
	}
	return -1;
}

static int emberPathPrefixEquals(const char *path, const char *prefix)
{
	size_t i;

	for (i = 0; prefix[i] != '\0'; i++) {
		if (path[i] == '\0')
			return 0;
		if (wle_ascii_tolower((unsigned char)path[i]) != wle_ascii_tolower((unsigned char)prefix[i]))
			return 0;
	}

	return 1;
}

static const char *findEmberGamesSegment(const char *path)
{
	const char *p;

	if (path == NULL)
		return NULL;

	for (p = path; *p != '\0'; p++) {
		if ((p == path || p[-1] == '/' || p[-1] == ':') &&
		    emberPathPrefixEquals(p, "games/"))
			return p;
	}

	return NULL;
}

static int parseEmberUsbMassUnit(const char *path, const char *prefix, int prefix_len, int *unit, const char **suffix)
{
	if (strncmp(path, prefix, prefix_len))
		return 0;

	if (path[prefix_len] == ':') {
		*unit = 0;
		*suffix = path + prefix_len + 1;
		return 1;
	}
	if (path[prefix_len] >= '0' && path[prefix_len] <= '9' && path[prefix_len + 1] == ':') {
		*unit = path[prefix_len] - '0';
		*suffix = path + prefix_len + 2;
		return 1;
	}

	return 0;
}

int IsEmberDiscImagePath(const char *path)
{
	return (path != NULL && (genCmpFileExt(path, "BIN") || genCmpFileExt(path, "CUE")));
}

int BuildEmberArg0Path(const char *ember_path, char *arg0_path, size_t arg0_path_size)
{
	const char *suffix;
	int unit = 0;
	int len;

	if (arg0_path == NULL || arg0_path_size == 0)
		return -1;
	arg0_path[0] = '\0';
	if (ember_path == NULL || ember_path[0] == '\0')
		return -1;

	if (!parseEmberUsbMassUnit(ember_path, "usb", 3, &unit, &suffix) &&
	    !parseEmberUsbMassUnit(ember_path, "mass", 4, &unit, &suffix))
		return 0;

	if (*suffix == '\0')
		suffix = "/";

	if (*suffix != '/')
		len = snprintf(arg0_path, arg0_path_size, "usb%d:/%s", unit, suffix);
	else
		len = snprintf(arg0_path, arg0_path_size, "usb%d:%s", unit, suffix);

	return (len > 0 && len < (int)arg0_path_size) ? 1 : -1;
}

int PrepareEmberLaunch(const char *image_path, char *ember_path, size_t ember_path_size,
                       char *game_arg, size_t game_arg_size, char *message, size_t message_size)
{
	const char *games;
	const char *relative_image;
	const char *game_file_sep;
	size_t root_len;
	int len;
	int exec_kind;
	char check_path[MAX_PATH];

	if (ember_path == NULL || ember_path_size == 0 || game_arg == NULL || game_arg_size == 0)
		return emberSetMessage(message, message_size, "Ember launch failed: %s", image_path);
	ember_path[0] = '\0';
	game_arg[0] = '\0';

	if (!IsEmberDiscImagePath(image_path))
		return emberSetMessage(message, message_size, "This file isn't an Ember disc image: %s", image_path);

	games = findEmberGamesSegment(image_path);
	if (games == NULL)
		return emberSetMessage(message, message_size, "Ember images must be under a games folder: %s", image_path);

	relative_image = games + strlen("games/");
	if (relative_image[0] == '\0')
		return emberSetMessage(message, message_size, "Ember image path is incomplete: %s", image_path);

	game_file_sep = strchr(relative_image, '/');
	if (game_file_sep == NULL || game_file_sep == relative_image || game_file_sep[1] == '\0')
		return emberSetMessage(message, message_size, "Ember images must be inside a game folder: %s", image_path);

	root_len = (size_t)(games - image_path);
	if (root_len + strlen("ember.elf") >= ember_path_size)
		return emberSetMessage(message, message_size, "Ember path is too long: %s", image_path);

	memcpy(ember_path, image_path, root_len);
	ember_path[root_len] = '\0';
	strcat(ember_path, "ember.elf");

	len = snprintf(game_arg, game_arg_size, "%s", relative_image);
	if (len <= 0 || len >= (int)game_arg_size)
		return emberSetMessage(message, message_size, "Ember argument is too long: %s", image_path);

	snprintf(check_path, sizeof(check_path), "%s", ember_path);
	exec_kind = checkELFheader(check_path);
	if (exec_kind <= 0)
		return emberSetMessage(message, message_size, "Cannot find Ember ELF: %s", ember_path);

	return 1;
}

