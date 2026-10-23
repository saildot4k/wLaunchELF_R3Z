#include "ttf_font.h"

#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb_truetype.h"

#define FONT_GLYPH_COUNT 256
#define FONT_GLYPH_WIDTH 8
#define FONT_GLYPH_HEIGHT 16
#define FONT_ATLAS_SIZE (FONT_GLYPH_COUNT * FONT_GLYPH_HEIGHT)

int ttfRasterizeFont(const unsigned char *font_data, size_t font_size,
                     unsigned char *glyph_buffer, size_t glyph_buffer_size)
{
	stbtt_fontinfo font;
	int font_offset;
	int ascent, descent, line_gap;
	float scale_y, scale_x;
	int codepoint;

	if (font_data == NULL || glyph_buffer == NULL || font_size < 12 ||
	    glyph_buffer_size < FONT_ATLAS_SIZE || font_size > 0x7fffffffU)
		return 0;

	font_offset = stbtt_GetFontOffsetForIndex(font_data, 0);
	if (font_offset < 0 || !stbtt_InitFont(&font, font_data, font_offset))
		return 0;

	memset(glyph_buffer, 0, FONT_ATLAS_SIZE);
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
	(void)descent;
	(void)line_gap;
	scale_y = stbtt_ScaleForPixelHeight(&font, 14.0f);
	scale_x = scale_y * 0.64f;
	if (scale_y <= 0.0f)
		return 0;

	for (codepoint = 32; codepoint < 256; codepoint++) {
		unsigned char *bitmap;
		int width, height, x_offset, y_offset;
		int baseline, x, y;
		unsigned char *glyph;

		if (stbtt_FindGlyphIndex(&font, codepoint) == 0)
			continue;
		bitmap = stbtt_GetCodepointBitmap(&font, scale_x, scale_y, codepoint,
		                                  &width, &height, &x_offset, &y_offset);
		if (bitmap == NULL)
			continue;

		glyph = glyph_buffer + codepoint * FONT_GLYPH_HEIGHT;
		baseline = (int)(ascent * scale_y + 0.5f);
		for (y = 0; y < height; y++) {
			int target_y = baseline + y_offset + y;
			if (target_y < 0 || target_y >= FONT_GLYPH_HEIGHT)
				continue;
			for (x = 0; x < width; x++) {
				int target_x = x_offset + x;
				if (target_x < 0 || target_x >= FONT_GLYPH_WIDTH)
					continue;
				if (bitmap[y * width + x] >= 96)
					glyph[target_y] |= (unsigned char)(0x80U >> target_x);
			}
		}
		stbtt_FreeBitmap(bitmap, NULL);
	}

	/* The renderer reserves 0xFF as a two-byte pad-symbol escape. */
	memset(glyph_buffer + 0xFF * FONT_GLYPH_HEIGHT, 0, FONT_GLYPH_HEIGHT);
	return 1;
}
