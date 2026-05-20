//--------------------------------------------------------------
//File name:   main_startup.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "init.h"
#include "main_startup.h"
#include <stdlib.h>

extern char default_OSDSYS_path[30];
extern char default_OSDSYS_path2[30];
extern char ROMVER_data[16];
extern char rough_region;

static unsigned short ROMVersion;

static void normalizeBootPath(char *path);
static void initializeBootExecPath(void);
static void buildBootBlockPath(char *dst, size_t dst_size, const char *prefix, const char *partition, const char *path_part);

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
		memset(buf, 0, IPCONF_MAX_LEN);
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

	memset(if_conf, 0, IPCONF_MAX_LEN);
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

//------------------------------
//Function to infer region from ROMVER and select default TV mode
//------------------------------
int uLE_InitializeRegion(void)
{
	int read_len;
	static int TVMode = -1;
	int i;
	int ROMVER_fd;
	char path_buf[32];
	static const char *romver_paths[] = {
	    "rom0:ROMVER",
	};

	// Keep default mode available even before ROMVER can be read.
	if (TVMode < 0)
		TVMode = TV_mode_NTSC;

	// If we already have ROMVER, keep the cached region/mode.
	if (ROMVER_data[0] != '\0')
		return TVMode;

	ensureCoreIoStackReady();

	memset(ROMVER_data, 0, sizeof(ROMVER_data));

	read_len = -1;
	for (i = 0; i < (int)(sizeof(romver_paths) / sizeof(romver_paths[0])); i++) {
		strncpy(path_buf, romver_paths[i], sizeof(path_buf) - 1);
		path_buf[sizeof(path_buf) - 1] = '\0';
		ROMVER_fd = genOpen(path_buf, FIO_O_RDONLY);
		if (ROMVER_fd < 0)
			continue;
		read_len = genRead(ROMVER_fd, ROMVER_data, sizeof(ROMVER_data) - 1);
		genClose(ROMVER_fd);
		if (read_len > 0)
			break;
		memset(ROMVER_data, 0, sizeof(ROMVER_data));
	}

	if (read_len <= 0 || ROMVER_data[0] == '\0') {
		memset(ROMVER_data, 0, sizeof(ROMVER_data));
		rough_region = 'X';
		return TVMode;
	}

	ROMVER_data[sizeof(ROMVER_data) - 1] = '\0';

	switch (ROMVER_data[4]) {
		case 'J':
			rough_region = 'I';
			break;
		case 'E':
			rough_region = 'E';
			break;
		case 'A':
		case 'H':  //Asia shares the same letter as USA.
			rough_region = 'A';
			break;
		case 'C':
			rough_region = 'C';
			break;
		default:
			rough_region = 'X';
	}

	if (ROMVER_data[4] == 'E')
		TVMode = TV_mode_PAL;  //PAL mode is identified by 'E' for Europe
	else
		TVMode = TV_mode_NTSC;  //All other cases need NTSC

	return TVMode;
}
//------------------------------
//endfunc uLE_InitializeRegion
//---------------------------------------------------------------------------
static void initializeBootExecPath(void)
{
	char file[12];
	char romver_prefix[5];

	uLE_InitializeRegion();
	strncpy(romver_prefix, ROMVER_data, 4);
	romver_prefix[4] = '\0';
	ROMVersion = strtoul(romver_prefix, NULL, 16);

	//Handle special cases, before osdmain.elf was supported.
	switch (ROMVER_data[4]) {
		case 'E':
			if (!strncmp(ROMVER_data, "0120", 4))
				strcpy(file, "osd130.elf");
			else
				strcpy(file, "osdmain.elf");
			break;
		case 'A':
			if (!strncmp(ROMVER_data, "0110", 4))
				strcpy(file, "osd120.elf");
			else
				strcpy(file, "osdmain.elf");
			break;
		case 'J':
			if (!strncmp(ROMVER_data, "0100", 4))
				strcpy(file, "osdsys.elf");
			else if (!strncmp(ROMVER_data, "0101", 4))
				strcpy(file, "osd110.elf");
			else
				strcpy(file, "osdmain.elf");
			break;
		default:  //Asia and China
			strcpy(file, "osdmain.elf");
	}

	sprintf(default_OSDSYS_path, "mc:/B%cEXEC-SYSTEM/%s", rough_region, file);
	if (ROMVersion >= 0x230)
		sprintf(default_OSDSYS_path2, "/Incompatible Unit (0x%03x)", (ROMVersion) & ~0x0F);
	else
		sprintf(default_OSDSYS_path2, "mc:/B%cEXEC-SYSTEM/%s", rough_region, file);
}
//------------------------------
//endfunc initializeBootExecPath
//---------------------------------------------------------------------------
static void normalizeBootPath(char *path)
{
	int i;

	if (path == NULL || path[0] == '\0')
		return;

	for (i = 0; path[i] != '\0'; i++) {
		if (path[i] == '\\')
			path[i] = '/';
	}

	// Legacy SwapMagic path normalization: mass0:/path -> mass:/path
	if (!strncmp(path, "mass0:", 6))
		memmove(path + 4, path + 5, strlen(path + 5) + 1);
}
//------------------------------
//endfunc normalizeBootPath
//---------------------------------------------------------------------------
static void buildBootBlockPath(char *dst, size_t dst_size, const char *prefix, const char *partition, const char *path_part)
{
	size_t len;
	size_t chunk;
	size_t room;

	if (dst == NULL || dst_size == 0)
		return;

	if (prefix == NULL)
		prefix = "";
	if (partition == NULL)
		partition = "";
	if (path_part == NULL)
		path_part = "";

	dst[0] = '\0';
	len = 0;

	room = dst_size - 1 - len;
	chunk = strlen(prefix);
	if (chunk > room)
		chunk = room;
	memcpy(dst + len, prefix, chunk);
	len += chunk;
	dst[len] = '\0';

	room = dst_size - 1 - len;
	chunk = strlen(partition);
	if (chunk > room)
		chunk = room;
	memcpy(dst + len, partition, chunk);
	len += chunk;
	dst[len] = '\0';

	if (path_part[0] != '/') {
		room = dst_size - 1 - len;
		chunk = strlen("/");
		if (chunk > room)
			chunk = room;
		memcpy(dst + len, "/", chunk);
		len += chunk;
		dst[len] = '\0';
	}

	room = dst_size - 1 - len;
	chunk = strlen(path_part);
	if (chunk > room)
		chunk = room;
	memcpy(dst + len, path_part, chunk);
	len += chunk;
	dst[len] = '\0';
}
//------------------------------
//endfunc buildBootBlockPath
//---------------------------------------------------------------------------
enum BOOT_DEVICE prepareBootDeviceAndPath(const char *arg0, char *boot_path, size_t boot_path_len)
{
	enum BOOT_DEVICE boot = BOOT_DEV_UNKNOWN;
	char *p;

	LaunchElfDir[0] = '\0';
	if (boot_path != NULL && boot_path_len > 0)
		boot_path[0] = '\0';

	if (arg0 != NULL && arg0[0] != '\0') {
		snprintf(LaunchElfDir, sizeof(LaunchElfDir), "%s", arg0);
		if (boot_path != NULL && boot_path_len > 0)
			snprintf(boot_path, boot_path_len, "%s", arg0);
		normalizeBootPath(LaunchElfDir);
		if (boot_path != NULL && boot_path_len > 0)
			normalizeBootPath(boot_path);

		if (!strncmp(LaunchElfDir, "mass", 4) || !strncmp(LaunchElfDir, "usb", 3)) {
			boot = BOOT_DEVICE_MASS;
		} else if (!strncmp(LaunchElfDir, "mc", 2)) {
			boot = BOOT_DEVICE_MC;
#ifdef MMCE
		} else if (!strncmp(LaunchElfDir, "mmce", 4)) {
			loadMmceModules();
			boot = BOOT_DEVICE_MASS;
#endif
#ifdef MX4SIO
		} else if (!strncmp(LaunchElfDir, "mx4sio", 6)) {
			loadMx4sioModules();
			boot = BOOT_DEVICE_MASS;
#endif
		} else if (!strncmp(LaunchElfDir, "ata", 3)) {
			boot = BOOT_DEVICE_MASS;
#ifdef EXFAT
			loadAtaModules();
#endif
		} else if (!strncmp(LaunchElfDir, "cd", 2)) {
			boot = BOOT_DEVICE_CDVD;
			snprintf(LaunchElfDir, sizeof(LaunchElfDir), "%s", "mc0:/SYS-CONF/");  //Default to mc0 as a writable location.
		} else if (!strncmp(LaunchElfDir, "hdd0:", 5) && boot_path != NULL && boot_path_len > 0) {
			//Booting from the HDD requires special handling for HDD-based paths.
			char temp[MAX_PATH];
			char *t;
			/* Change boot_path to contain a path to the block device.
				Standard HDD path format: hdd0:partition:pfs:path/to/file
				However, (older) homebrew may not use this format. */
			snprintf(temp, sizeof(temp), "%s", boot_path + 5);  //Skip "hdd0:" when copying.
			t = strchr(temp, ':');                              //Check if the separator between the block device & the path exists.
				if (t != NULL) {
					char *path_part;
					*(t) = 0;                      //If it does, get the block device name.
					path_part = strchr(t + 1, ':');  //Get the path to the file
					if (path_part != NULL) {
						buildBootBlockPath(LaunchElfDir, sizeof(LaunchElfDir), "hdd0:/", temp, path_part + 1);
					}
				}

			boot = BOOT_DEVICE_HDD;
#ifdef DVRP
		} else if (console_is_PSX && !strncmp(LaunchElfDir, "dvr_hdd0:", 9) && boot_path != NULL && boot_path_len > 0) {
			//Booting from the HDD requires special handling for HDD-based paths.
			char temp[MAX_PATH];
			char *t;
			/* Change boot_path to contain a path to the block device.
				Standard HDD path format: dvr_hdd0:partition:pfs:path/to/file
				However, (older) homebrew may not use this format. */
			snprintf(temp, sizeof(temp), "%s", boot_path + 9);  //Skip "dvr_hdd0:" when copying.
			t = strchr(temp, ':');                              //Check if the separator between the block device & the path exists.
				if (t != NULL) {
					char *path_part;
					*(t) = 0;                      //If it does, get the block device name.
					path_part = strchr(t + 1, ':');  //Get the path to the file
					if (path_part != NULL) {
						buildBootBlockPath(LaunchElfDir, sizeof(LaunchElfDir), "dvr_hdd0:/", temp, path_part + 1);
					}
				}

			boot = BOOT_DEVICE_HDD;
#endif
		}
	}

#if defined(ETH) || defined(UDPFS)
#ifdef ETH
	if (!strncmp(LaunchElfDir, "host", 4))
		boot = BOOT_DEVICE_HOST;
#endif
#ifdef UDPFS
	if (!strncmp(LaunchElfDir, "udpfs", 5))
		boot = BOOT_DEVICE_HOST;
#endif
#endif
	DPRINTF("Boot device is %d\n", boot);

	if (((p = strrchr(LaunchElfDir, '/')) == NULL) && ((p = strrchr(LaunchElfDir, '\\')) == NULL))
		p = strrchr(LaunchElfDir, ':');
	if (p != NULL)
		*(p + 1) = 0;
	//The above cuts away the ELF filename from LaunchElfDir, leaving a pure path

	LastDir[0] = 0;

	return boot;
}
//------------------------------
//endfunc prepareBootDeviceAndPath
//---------------------------------------------------------------------------
void initializeBootDisplayDefaults(void)
{
	TV_mode = uLE_InitializeRegion();  //Let console region decide default TV_mode
	Frame_end_y = Menu_end_y + 4;
	Menu_tooltip_y = Frame_end_y + LINE_THICKNESS + 2;
	initializeBootExecPath();
}
//------------------------------
//endfunc initializeBootDisplayDefaults
//---------------------------------------------------------------------------
void bringUpBootNetworkStack(enum BOOT_DEVICE boot_device)
{
#if defined(ETH) || defined(UDPFS)
	if (boot_device == BOOT_DEVICE_HOST) {
		//If booted from a network filesystem device, bring that stack up now.
		getIpConfig();
#if defined(ETH) && defined(UDPFS)
		if (!strncmp(LaunchElfDir, "udpfs", 5))
			load_udpfs();
		else
			initHOST();
#elif defined(ETH)
		initHOST();
#else
		load_udpfs();
#endif
	}
#else
	(void)boot_device;
#endif
}
//------------------------------
//endfunc bringUpBootNetworkStack
//---------------------------------------------------------------------------
void initializeBootGraphics(void)
{
	DPRINTF("setupGS()\n");
	setupGS();
	gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00));
	loadFont("");
	setInitPhaseComplete();
}
//------------------------------
//endfunc initializeBootGraphics
//---------------------------------------------------------------------------
void loadConfiguredBootFont(void)
{
	loadFont(setting->font_file);
}
//------------------------------
//endfunc loadConfiguredBootFont
//---------------------------------------------------------------------------
unsigned short getROMVersion(void)
{
	return ROMVersion;
}
