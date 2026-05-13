#ifndef WLE_LIBJPG_COMPAT_H
#define WLE_LIBJPG_COMPAT_H

#include <libjpg_ps2_addons.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * Compatibility wrapper for legacy uLaunchELF JPEG API names.
 * Newer PS2SDK ports provide libjpg_ps2_addons with jpgFrom* entry points.
 */
static inline jpgData *jpgOpenRAW(void *data, int size, int mode)
{
	return jpgFromRAW(data, size, mode);
}

static inline jpgData *jpgOpen(const char *filename, int mode)
{
	return jpgFromFilename(filename, mode);
}

static inline jpgData *jpgOpenFILE(FILE *in_file, int mode)
{
	return jpgFromFILE(in_file, mode);
}

static inline int jpgReadImage(jpgData *jpg, void *buffer)
{
	size_t image_size;

	if (!jpg || !jpg->buffer || !buffer || jpg->width <= 0 || jpg->height <= 0 || jpg->bpp <= 0)
		return -1;

	image_size = (size_t)jpg->width * (size_t)jpg->height * (size_t)(jpg->bpp / 8);
	memcpy(buffer, jpg->buffer, image_size);
	return 0;
}

static inline void jpgClose(jpgData *jpg)
{
	if (!jpg)
		return;

	free(jpg->buffer);
	free(jpg);
}

#endif
