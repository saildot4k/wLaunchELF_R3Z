#ifndef MAIN_TITLE_H
#define MAIN_TITLE_H

#include <stddef.h>

void menuTitleFormat(char *out, size_t out_size);
int menuTitleUpdateAsync(int force);
void menuTitleGetClockFormat(int *use_12h, int *date_format);
void menuTitleFormatClockTime(char *dst, size_t dst_size, int hour, int minute, int second, int use_12h);
void menuTitleFormatClockDate(char *dst, size_t dst_size, int year, int month, int day, int date_format);

#endif
