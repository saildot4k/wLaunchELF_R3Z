//--------------------------------------------------------------
//File name:   main_console_info.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "main_console_info.h"

static int copy_printable_prefix(char *out, size_t out_len, const char *in, int max_chars)
{
	int i;
	int n = 0;

	if (out_len == 0)
		return 0;

	if (in != NULL) {
		for (i = 0; in[i] != '\0' && i < max_chars && n < (int)out_len - 1; i++) {
			unsigned char ch = (unsigned char)in[i];

			if (ch < 0x20 || ch >= 0x7f)
				break;
			out[n++] = (char)ch;
		}
	}

	out[n] = '\0';
	return n;
}

static void get_early_model_name(const char *romver, char *model, size_t model_len)
{
	int fd;
	int read_len;

	if (!strncmp(romver, "0100", 4)) {
		snprintf(model, model_len, "SCPH-10000");
		return;
	}

	fd = genOpen("rom0:OSDSYS", FIO_O_RDONLY);
	if (fd < 0) {
		snprintf(model, model_len, "Unknown");
		return;
	}

	if (genLseek(fd, 0x8C808, SEEK_SET) < 0) {
		genClose(fd);
		snprintf(model, model_len, "Unknown");
		return;
	}

	read_len = genRead(fd, model, (int)model_len - 1);
	genClose(fd);
	if (read_len <= 0) {
		snprintf(model, model_len, "Unknown");
		return;
	}

	model[read_len] = '\0';
	copy_printable_prefix(model, model_len, model, (int)model_len - 1);
	if (model[0] == '\0')
		snprintf(model, model_len, "Unknown");
}

static void get_model_fallback_name(const char *romver, char *model, size_t model_len)
{
	(void)romver;
	snprintf(model, model_len, "Unknown");
}

static void initialize_model_cdvd_rpc(void)
{
	u8 mecha_version[3];
	u32 stat = 0;
	int i;

	loadCdModules();
	sceCdInit(SCECdINoD);

	for (i = 0; i <= 100; i++) {
		if (sceCdMV(mecha_version, &stat) != 0 && !(stat & 0x80))
			return;
	}
}

int IsDtlConsoleIdentity(const char *romver, const char *model)
{
	if (romver != NULL && !strncmp(romver, "0180C", 5))
		return 1;
	if (romver != NULL && strlen(romver) > 5 && romver[5] == 'D')
		return 1;

	return (model != NULL && (!strncmp(model, "DTL-", 4) || !strncmp(model, "DTH-", 4)));
}

static int read_model_name_scmd17(char *model, size_t model_len, u32 *stat)
{
	unsigned char rdata[9];
	unsigned char offset;
	char raw_model[17];
	int result1, result2;

	if (model == NULL || model_len == 0 || stat == NULL)
		return 0;

	memset(raw_model, 0, sizeof(raw_model));
	memset(rdata, 0, sizeof(rdata));

	offset = 0;
	result1 = sceCdApplySCmd(0x17, &offset, 1, rdata);
	*stat = rdata[0];
	memcpy(raw_model, &rdata[1], 8);

	memset(rdata, 0, sizeof(rdata));
	offset = 8;
	result2 = sceCdApplySCmd(0x17, &offset, 1, rdata);
	*stat |= rdata[0];
	memcpy(&raw_model[8], &rdata[1], 8);

	if (result1 == 0 || result2 == 0 || (*stat & (0x80 | 0x40)))
		return 0;

	copy_printable_prefix(model, model_len, raw_model, (int)sizeof(raw_model) - 1);
	return model[0] != '\0';
}

void GetConsoleModelName(const char *romver, char *model, size_t model_len)
{
	u32 stat = 0;

	if (model_len == 0)
		return;

	model[0] = '\0';
	if (romver != NULL && !strncmp(romver, "010", 3)) {
		get_early_model_name(romver, model, model_len);
		if (!strcmp(model, "Unknown"))
			get_model_fallback_name(romver, model, model_len);
		return;
	}

	initialize_model_cdvd_rpc();

	if (read_model_name_scmd17(model, model_len, &stat))
		return;

	get_model_fallback_name(romver, model, model_len);
}

void FormatConsoleBootrom(char *dst, size_t dst_size, const char *romver)
{
	if (dst == NULL || dst_size == 0)
		return;

	if (romver != NULL && strlen(romver) >= 5)
		snprintf(dst, dst_size, "BOOTROM: %c.%c%c %c", romver[1], romver[2], romver[3], romver[4]);
	else
		snprintf(dst, dst_size, "BOOTROM: ?.?? ?");
}

void GetConsoleDvdVersion(char *dst, size_t dst_size)
{
	char dvdver[16];
	int fd, read_len;
	size_t i, version_len;

	if (dst == NULL || dst_size == 0)
		return;

	fd = genOpen("rom1:DVDVER", FIO_O_RDONLY);
	if (fd < 0) {
		snprintf(dst, dst_size, "DVD: <unavailable>");
		return;
	}

	memset(dvdver, 0, sizeof(dvdver));
	read_len = genRead(fd, dvdver, sizeof(dvdver) - 1);
	genClose(fd);
	if (read_len <= 0) {
		snprintf(dst, dst_size, "DVD: <unavailable>");
		return;
	}

	dvdver[read_len] = '\0';
	for (i = 0; dvdver[i] != '\0'; i++) {
		if (dvdver[i] == '\r' || dvdver[i] == '\n') {
			dvdver[i] = '\0';
			break;
		}
	}
	version_len = strlen(dvdver);

	/* ROMs use either compact 0200E or already-readable 2.00E versions. */
	if (version_len == 5 && dvdver[0] == '0' &&
	    dvdver[1] >= '0' && dvdver[1] <= '9' &&
	    dvdver[2] >= '0' && dvdver[2] <= '9' &&
	    dvdver[3] >= '0' && dvdver[3] <= '9' &&
	    ((dvdver[4] >= 'A' && dvdver[4] <= 'Z') || (dvdver[4] >= 'a' && dvdver[4] <= 'z')))
		snprintf(dst, dst_size, "DVD: %c.%c%c %c", dvdver[1], dvdver[2], dvdver[3], dvdver[4]);
	else if (version_len == 5 && dvdver[1] == '.' &&
	         dvdver[0] >= '0' && dvdver[0] <= '9' &&
	         dvdver[2] >= '0' && dvdver[2] <= '9' &&
	         dvdver[3] >= '0' && dvdver[3] <= '9' &&
	         ((dvdver[4] >= 'A' && dvdver[4] <= 'Z') || (dvdver[4] >= 'a' && dvdver[4] <= 'z')))
		snprintf(dst, dst_size, "DVD: %.4s %c", dvdver, dvdver[4]);
	else
		snprintf(dst, dst_size, "DVD: %s", dvdver);
}
