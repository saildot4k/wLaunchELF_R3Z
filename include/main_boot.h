#ifndef MAIN_BOOT_H
#define MAIN_BOOT_H

#include <stddef.h>
#include "main_startup.h"

void captureBootArguments(int argc, char *argv[], int *boot_argc, char *boot_argv[], int boot_argv_cap);
enum BOOT_DEVICE performEarlyBootInitialization(const char *arg0, char *boot_path, size_t boot_path_len, char *main_msg, char *cnf_path, size_t cnf_path_len, int *cnf_error);

#endif
