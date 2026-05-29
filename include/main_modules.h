#ifndef MAIN_MODULES_H
#define MAIN_MODULES_H

#include "main_startup.h"

void initializeRuntimeInputModules(enum BOOT_DEVICE boot_device);
void initializeRuntimeDisplayModules(int cnf_error, char *main_msg);
void validateConfiguredCnfPath(void);

#endif
