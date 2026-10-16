#ifndef MAIN_CONSOLE_INFO_H
#define MAIN_CONSOLE_INFO_H

#include <stddef.h>

#define CONSOLE_MODEL_NAME_MAX_LEN 24
#define CONSOLE_INFO_LINE_MAX_LEN 24

void GetConsoleModelName(const char *romver, char *model, size_t model_len);
int IsDtlConsoleIdentity(const char *romver, const char *model);
void FormatConsoleBootrom(char *dst, size_t dst_size, const char *romver);
void GetConsoleDvdVersion(char *dst, size_t dst_size);

#endif
