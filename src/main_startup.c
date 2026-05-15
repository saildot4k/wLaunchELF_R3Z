//--------------------------------------------------------------
//File name:   main_startup.c
//--------------------------------------------------------------
#include "launchelf.h"

// Parse network configuration from IPCONFIG.DAT
// Now completely rewritten to fix some problems
void getIpConfig(void)
{
	int fd;
	int i;
	int len;
	int preferred_port;
	int ports_to_try[2];
	int port_ix;
	char c;
	char buf[IPCONF_MAX_LEN];
	char path[MAX_PATH];
	char candidate[MAX_PATH];
	size_t dir_len;
	static const char ipconfig_name[] = "IPCONFIG.DAT";

	fd = -1;
	len = 0;

	// Prefer IPCONFIG in the same directory as the launched ELF.
	dir_len = strnlen(LaunchElfDir, sizeof(candidate));
	if (dir_len < sizeof(candidate) && (dir_len + sizeof(ipconfig_name)) <= sizeof(candidate)) {
		memcpy(candidate, LaunchElfDir, dir_len);
		memcpy(candidate + dir_len, ipconfig_name, sizeof(ipconfig_name));
		if (genFixPath(candidate, path) >= 0)
			fd = genOpen(path, FIO_O_RDONLY);
	}

	// Fallback to SYS-CONF on memory cards, preferring current MC slot.
	if (fd < 0) {
		preferred_port = 0;
		if (!strncmp(LaunchElfDir, "mc1", 3))
			preferred_port = 1;
		ports_to_try[0] = preferred_port;
		ports_to_try[1] = preferred_port ^ 1;

		for (port_ix = 0; port_ix < 2 && fd < 0; port_ix++) {
			snprintf(candidate, sizeof(candidate), "mc%d:/SYS-CONF/IPCONFIG.DAT", ports_to_try[port_ix]);
			if (genFixPath(candidate, path) >= 0)
				fd = genOpen(path, FIO_O_RDONLY);
		}
	}

	if (fd >= 0) {
		bzero(buf, IPCONF_MAX_LEN);
		len = genRead(fd, buf, IPCONF_MAX_LEN - 1);  //Save a byte for termination
		genClose(fd);
	}

	if ((fd >= 0) && (len > 0)) {
		buf[len] = '\0';                          //Ensure string termination, regardless of file content
		for (i = 0; ((c = buf[i]) != '\0'); i++)  //Clear out spaces and any CR/LF
			if ((c == ' ') || (c == '\r') || (c == '\n'))
				buf[i] = '\0';
		snprintf(ip, sizeof(ip), "%.15s", buf);
		i = strlen(ip) + 1;
		snprintf(netmask, sizeof(netmask), "%.15s", buf + i);
		i += strlen(netmask) + 1;
		snprintf(gw, sizeof(gw), "%.15s", buf + i);
	}

	bzero(if_conf, IPCONF_MAX_LEN);
	i = 0;
	memcpy(if_conf + i, ip, strlen(ip) + 1);
	i += strlen(ip) + 1;
	memcpy(if_conf + i, netmask, strlen(netmask) + 1);
	i += strlen(netmask) + 1;
	memcpy(if_conf + i, gw, strlen(gw) + 1);
	i += strlen(gw) + 1;
	if_conf_len = i;
	snprintf(netConfig, sizeof(netConfig), "%s:  %-15s %-15s %-15s", LNG(Net_Config), ip, netmask, gw);
}

//scanSystemCnf will check for a standard variable of a SYSTEM.CNF file
static int scanSystemCnf(char *name, char *value)
{
	if (!strcmp(name, "BOOT"))
		strncat(SystemCnf_BOOT, value, MAX_PATH - 1);
	else if (!strcmp(name, "BOOT2"))
		strncat(SystemCnf_BOOT2, value, MAX_PATH - 1);
	else if (!strcmp(name, "VER"))
		strncat(SystemCnf_VER, value, 9);
	else if (!strcmp(name, "VMODE"))
		strncat(SystemCnf_VMODE, value, 9);
	else
		return 0;  //when no matching variable
	return 1;      //when matching variable found
}

//readSystemCnf will read standard settings from a SYSTEM.CNF file
int readSystemCnf(void)
{
	int var_cnt;
	char *RAM_p, *CNF_p, *name, *value;

	BootDiscType = 0;
	SystemCnf_BOOT[0] = '\0';
	SystemCnf_BOOT2[0] = '\0';
	SystemCnf_VER[0] = '\0';
	SystemCnf_VMODE[0] = '\0';

	if ((RAM_p = preloadCNF("cdrom0:\\SYSTEM.CNF;1")) != NULL) {
		CNF_p = RAM_p;
		for (var_cnt = 0; get_CNF_string(&CNF_p, &name, &value); var_cnt++)
			scanSystemCnf(name, value);
		free(RAM_p);
	}

	if (SystemCnf_BOOT2[0])
		BootDiscType = 2;
	else if (SystemCnf_BOOT[0])
		BootDiscType = 1;

	if (!SystemCnf_BOOT[0])
		strcpy(SystemCnf_BOOT, "???");
	if (!SystemCnf_VER[0])
		strcpy(SystemCnf_VER, "???");

	if (RAM_p == NULL) {  //if SYSTEM.CNF was not found test for PS1 special cases
		if (wleExists("cdrom0:\\PSXMYST\\MYST.CCS;1")) {
			strcpy(SystemCnf_BOOT, "SLPS_000.24");
			BootDiscType = 1;
		} else if (wleExists("cdrom0:\\CDROM\\LASTPHOT\\ALL_C.NBN;1")) {
			strcpy(SystemCnf_BOOT, "SLPS_000.65");
			BootDiscType = 1;
		} else if (wleExists("cdrom0:\\PSX.EXE;1")) {
			BootDiscType = 1;
		}
	}

	return BootDiscType;  //0==none, 1==PS1, 2==PS2
}
