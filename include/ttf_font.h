#ifndef TTF_FONT_H
#define TTF_FONT_H

#include <stddef.h>

/* Rasterize the Latin-1 glyph range into the legacy 8x16, 1-bit font atlas. */
int ttfRasterizeFont(const unsigned char *font_data, size_t font_size,
                     unsigned char *glyph_buffer, size_t glyph_buffer_size);

#endif
