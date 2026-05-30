//--------------------------------------------------------------
//File name:   main_boot.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "init.h"
#include "main_boot.h"

void captureBootArguments(int argc, char *argv[], int *boot_argc, char *boot_argv[], int boot_argv_cap)
{
	int i;

	if (boot_argc == NULL || boot_argv == NULL || boot_argv_cap <= 0)
		return;

	*boot_argc = argc;
	for (i = 0; (i < argc) && (i < boot_argv_cap); i++)
		boot_argv[i] = argv[i];
}

enum BOOT_DEVICE performEarlyBootInitialization(const char *arg0, char *boot_path, size_t boot_path_len, char *main_msg, char *cnf_path, size_t cnf_path_len, int *cnf_error)
{
	enum BOOT_DEVICE boot;
	int local_cnf_error;
	char local_cnf_path[MAX_NAME];
	char *cnf_path_buf;
	size_t cnf_path_buf_len;

	Reset();
	Init_Default_Language();
	if (wleExists("rom0:PSXVER")) {
		console_is_PSX = 1;
		DPRINTF("# Console is PSX-DESR\n");
	}
	boot = prepareBootDeviceAndPath(arg0, boot_path, boot_path_len);
	if (boot == BOOT_DEVICE_MASS && (!strncmp(LaunchElfDir, "mass", 4) || !strncmp(LaunchElfDir, "usb", 3))) {
		DPRINTF("Loading USB modules for USB boot\n");
		loadUsbModules();
	}
	initializeBootDisplayDefaults();

	cnf_path_buf = cnf_path;
	cnf_path_buf_len = cnf_path_len;
	if (cnf_path_buf == NULL || cnf_path_buf_len == 0) {
		cnf_path_buf = local_cnf_path;
		cnf_path_buf_len = sizeof(local_cnf_path);
	}
	snprintf(cnf_path_buf, cnf_path_buf_len, "%s", "LAUNCHELF.CNF");
	local_cnf_error = loadConfig(main_msg, cnf_path_buf);
	if (local_cnf_error < 0) {
		/* No config loaded: default pad mapping from ROM region.
		 * ROMVER_data[4] is the region letter.
		 * J/C => Circle=OK, Cross=Cancel (swapKeys=FALSE)
		 * others => Cross=OK, Circle=Cancel (swapKeys=TRUE)
		 */
		if (ROMVER_data[0] == '\0')
			uLE_InitializeRegion();
		setting->swapKeys = ((ROMVER_data[4] == 'J') || (ROMVER_data[4] == 'C')) ? FALSE : TRUE;
	}
	bringUpBootNetworkStack(boot);
	initializeBootGraphics();

	swapKeys = setting->swapKeys;
	if (cnf_error != NULL)
		*cnf_error = local_cnf_error;

	return boot;
}
