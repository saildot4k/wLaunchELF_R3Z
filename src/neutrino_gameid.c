#include "launchelf.h"
#include "neutrino_gameid.h"

#define ISO_SECTOR_SIZE 2048
#define ISO_PVD_LBA 16
#define ISO_PVD_SEARCH_SECTORS 32
#define ISO_MAX_ROOT_SECTORS 64
#define ISO_CNF_SIZE 4096
#define MMCE_CMD_PING 0x1
#define MMCE_CMD_GET_STATUS 0x2
#define MMCE_CMD_SET_GAMEID 0x8
#define MMCE_SWITCH_POLLS 15

static u32 isoReadLe32(const unsigned char *value)
{
	return (u32)value[0] | ((u32)value[1] << 8) |
	       ((u32)value[2] << 16) | ((u32)value[3] << 24);
}

static int readIsoBytes(int fd, s64 image_size, s64 offset, void *buffer, int size)
{
	int total = 0, count;

	if (offset < 0 || size <= 0 || offset > image_size - size ||
	    genLseek(fd, offset, SEEK_SET) != offset)
		return 0;
	while (total < size) {
		count = genRead(fd, (unsigned char *)buffer + total, size - total);
		if (count <= 0 || count > size - total)
			return 0;
		total += count;
	}
	return 1;
}

static int isoTextMatches(const unsigned char *text, const char *pattern, size_t length)
{
	size_t i;

	for (i = 0; i < length; i++) {
		if (wle_ascii_tolower(text[i]) != wle_ascii_tolower((unsigned char)pattern[i]))
			return 0;
	}
	return 1;
}

static int readIsoSystemCnf(int fd, char *cnf, size_t cnf_size)
{
	unsigned char sector[ISO_SECTOR_SIZE] __attribute__((aligned(16)));
	s64 image_size;
	u32 root_lba, root_size, sector_index, remaining;
	int i, pos, limit, record_len, name_len;

	image_size = genLseek(fd, 0, SEEK_END);
	for (i = 0; i < ISO_PVD_SEARCH_SECTORS; i++) {
		if (!readIsoBytes(fd, image_size, (s64)(ISO_PVD_LBA + i) * ISO_SECTOR_SIZE, sector, sizeof(sector)))
			return 0;
		if (memcmp(sector + 1, "CD001", 5) || sector[6] != 1 || sector[0] == 255)
			return 0;
		if (sector[0] == 1)
			break;
	}
	if (i == ISO_PVD_SEARCH_SECTORS || sector[156] < 34 || sector[157] != 0 || !(sector[181] & 2))
		return 0;
	root_lba = isoReadLe32(sector + 158);
	root_size = isoReadLe32(sector + 166);
	if (root_size == 0 || (s64)root_lba * ISO_SECTOR_SIZE > image_size - root_size)
		return 0;

	remaining = root_size;
	for (sector_index = 0; remaining > 0 && sector_index < ISO_MAX_ROOT_SECTORS; sector_index++) {
		limit = remaining < ISO_SECTOR_SIZE ? (int)remaining : ISO_SECTOR_SIZE;
		if (!readIsoBytes(fd, image_size, ((s64)root_lba + sector_index) * ISO_SECTOR_SIZE, sector, limit))
			return 0;
		for (pos = 0; pos < limit; pos += record_len) {
			record_len = sector[pos];
			if (record_len == 0)
				break; // Padding continues in the next directory sector.
			if (record_len < 34 || record_len > limit - pos)
				return 0;
			name_len = sector[pos + 32];
			if (name_len > record_len - 33)
				return 0;
			if ((sector[pos + 25] & (2 | 0x80)) ||
			    (name_len != 10 && name_len != 12))
				continue;
			if (isoTextMatches(sector + pos + 33, "SYSTEM.CNF", 10) &&
			    (name_len == 10 || !memcmp(sector + pos + 43, ";1", 2))) {
				u32 file_lba = isoReadLe32(sector + pos + 2);
				u32 file_size = isoReadLe32(sector + pos + 10);

				if (sector[pos + 1] != 0 || sector[pos + 26] != 0 || sector[pos + 27] != 0 ||
				    file_size == 0 || file_size >= cnf_size ||
				    !readIsoBytes(fd, image_size, (s64)file_lba * ISO_SECTOR_SIZE, cnf, file_size))
					return 0;
				cnf[file_size] = '\0';
				return 1;
			}
		}
		remaining -= limit;
	}
	return 0;
}

static int validGameID(const char *gameID)
{
	int i;
	unsigned char c;

	if (gameID == NULL || strlen(gameID) != NEUTRINO_GAME_ID_SIZE - 1 ||
	    gameID[4] != '_' || gameID[8] != '.')
		return 0;
	for (i = 0; i < 4; i++) {
		c = wle_ascii_tolower((unsigned char)gameID[i]);
		if (c < 'a' || c > 'z')
			return 0;
	}
	for (i = 5; i < NEUTRINO_GAME_ID_SIZE - 1; i++) {
		if (i != 8 && (gameID[i] < '0' || gameID[i] > '9'))
			return 0;
	}
	return 1;
}

static int parseIsoBootPath(const char *value, const char *end, char *gameID)
{
	const char *name, *token_end, *version, *p;
	char candidate[NEUTRINO_GAME_ID_SIZE];
	int i;

	if (value < end && *value == '"')
		value++;
	if (end - value < 6 || !isoTextMatches((const unsigned char *)value, "cdrom", 5))
		return 0;
	value += 5;
	if (value < end && *value == '0')
		value++;
	if (value == end || *value++ != ':')
		return 0;
	name = value;
	for (token_end = value; token_end < end && *token_end != ' ' && *token_end != '\t' && *token_end != '"'; token_end++) {
		if (*token_end == '/' || *token_end == '\\')
			name = token_end + 1;
	}
	version = memchr(name, ';', token_end - name);
	if (version != NULL) {
		if (version + 1 == token_end)
			return 0;
		for (p = version + 1; p < token_end; p++) {
			if (*p < '0' || *p > '9')
				return 0;
		}
	} else {
		version = token_end;
	}
	if (version - name != NEUTRINO_GAME_ID_SIZE - 1)
		return 0;
	memcpy(candidate, name, sizeof(candidate) - 1);
	candidate[sizeof(candidate) - 1] = '\0';
	if (!validGameID(candidate))
		return 0;
	for (i = 0; i < 4; i++)
		candidate[i] = wle_ascii_tolower((unsigned char)candidate[i]) - ('a' - 'A');
	memcpy(gameID, candidate, sizeof(candidate));
	return 1;
}

static int parseIsoGameID(const char *cnf, char *gameID)
{
	const char *line, *end, *value;

	for (line = cnf; *line != '\0'; line = end) {
		end = line;
		while (*end != '\0' && *end != '\r' && *end != '\n')
			end++;
		while (line < end && (*line == ' ' || *line == '\t'))
			line++;
		if (end - line >= 5 && isoTextMatches((const unsigned char *)line, "BOOT2", 5)) {
			value = line + 5;
			while (value < end && (*value == ' ' || *value == '\t'))
				value++;
			if (value < end && *value == '=') {
				value++;
				while (value < end && (*value == ' ' || *value == '\t'))
					value++;
				return parseIsoBootPath(value, end, gameID);
			}
		}
		while (*end == '\r' || *end == '\n')
			end++;
	}
	return 0;
}

int prepareNeutrinoMmceGameID(const char *iso_path, const char *elf_path, char *gameID, size_t gameID_len)
{
	char cnf[ISO_CNF_SIZE + 1] __attribute__((aligned(16)));
	int fd, result;

	if (gameID == NULL || gameID_len == 0)
		return 0;
	gameID[0] = '\0';
	if (gameID_len < NEUTRINO_GAME_ID_SIZE || iso_path == NULL || elf_path == NULL ||
	    iso_path[0] == '\0' || elf_path[0] == '\0' ||
	    !strncmp(iso_path, "mx4sio", 6) || !strncmp(elf_path, "mx4sio", 6))
		return 0;
	/* Load before opening the ISO or preparing the ELF: switching from MX4SIO resets the IOP. */
	if (!loadMmceModules())
		return 0;
#ifdef ETH
	if (!strncmp(iso_path, "host:", 5))
		initHOST();
#endif
#ifdef UDPFS
	if (!strncmp(iso_path, "udpfs:", 6) && !load_udpfs())
		return 0;
#endif
	fd = openPathForRead(iso_path, NULL, 0);
	if (fd < 0)
		return 0;
	result = readIsoSystemCnf(fd, cnf, sizeof(cnf)) && parseIsoGameID(cnf, gameID);
	genClose(fd);
	return result;
}

void setNeutrinoMmceGameID(const char *gameID, const char *elf_path)
{
	char device[] = "mmce0:";
	int port, poll, status;

	if (!validGameID(gameID) || elf_path == NULL || !strncmp(elf_path, "mx4sio", 6))
		return;
#ifdef MX4SIO
	if (mx4sio_driver_running)
		return;
#endif
	/* Preparing an ELF on another device can reload the IOP stack. */
	if (!loadMmceModules())
		return;
	for (port = 0; port < 2; port++) {
		if (!strncmp(elf_path, port == 0 ? "mc0:" : "mc1:", 4))
			continue; // The loader still needs to read Neutrino from this VMC.
		device[4] = '0' + port;
		if (fileXioDevctl(device, MMCE_CMD_PING, NULL, 0, NULL, 0) < 0 ||
		    fileXioDevctl(device, MMCE_CMD_SET_GAMEID, (void *)gameID, NEUTRINO_GAME_ID_SIZE, NULL, 0) < 0)
			continue;
		for (poll = 0; poll < MMCE_SWITCH_POLLS; poll++) {
			DelayThread(1000000);
			status = fileXioDevctl(device, MMCE_CMD_GET_STATUS, NULL, 0, NULL, 0);
			if (status < 0 || !(status & 1))
				break;
		}
		DPRINTF("Neutrino MMCE: %s game=%s polls=%d\n", device, gameID, poll);
	}
}
