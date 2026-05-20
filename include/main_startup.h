#ifndef MAIN_STARTUP_H
#define MAIN_STARTUP_H

#include <stddef.h>

enum BOOT_DEVICE {
	BOOT_DEVICE_CDVD = 0,
	BOOT_DEVICE_MC,
	BOOT_DEVICE_MASS,
	BOOT_DEVICE_HOST,
	BOOT_DEVICE_HDD,

	BOOT_DEV_UNKNOWN = -1
};

enum BOOT_DEVICE prepareBootDeviceAndPath(const char *arg0, char *boot_path, size_t boot_path_len);
void initializeBootDisplayDefaults(void);
void initializeBootGraphics(void);
void loadConfiguredBootFont(void);
void bringUpBootNetworkStack(enum BOOT_DEVICE boot_device);
unsigned short getROMVersion(void);

#endif
