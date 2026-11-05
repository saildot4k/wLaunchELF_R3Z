/* Save containers supported by Apollo Save Tool (Bucanero), with bounded
 * parsers for use in the LaunchELF browser. PSU stays in psu_functions.c.
 * MAX/CBS/XPS layout references: root670's CheatDevicePS2 and Apollo. */
#include "save_formats.h"
#include "save_lzari.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zlib.h>

static uint16_t rd16(const uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | ((uint32_t)rd16(p + 2) << 16); }
static void wr16(uint8_t *p, uint16_t n)
{
	p[0] = n;
	p[1] = n >> 8;
}
static void wr32(uint8_t *p, uint32_t n)
{
	wr16(p, n);
	wr16(p + 2, n >> 16);
}
static int span(size_t size, size_t offset, size_t length) { return offset <= size && length <= size - offset; }

int saveNameValid(const char *name, size_t limit)
{
	size_t i;
	if (!name || !*name || !strcmp(name, ".") || !strcmp(name, ".."))
		return 0;
	for (i = 0; name[i]; i++)
		if (i >= limit || (uint8_t)name[i] < 32 || name[i] == '/' || name[i] == '\\' || name[i] == ':')
			return 0;
	return 1;
}

static int nameRead(char *out, const uint8_t *in, size_t width, size_t limit)
{
	size_t n = 0;
	while (n < width && in[n])
		n++;
	if (n > limit)
		return -1;
	memcpy(out, in, n);
	out[n] = 0;
	return saveNameValid(out, limit) ? 0 : -1;
}

void saveArchiveFree(save_archive_t *save)
{
	free(save->files);
	free(save->storage);
	memset(save, 0, sizeof(*save));
}

int saveArchiveValidate(const save_archive_t *s)
{
	size_t total = 0;
	int i, j;
	if ((s->system != 1 && s->system != 2) || s->count < 1 || s->count > SAVE_MAX_FILES || !s->files ||
	    !saveNameValid(s->root.name, s->system == 1 ? 20 : 32))
		return -1;
	if (s->system == 1 && s->count != 1)
		return -1;
	if (s->system == 2 && s->root.mode &&
	    (!(s->root.mode & 0x20) || (s->root.mode & 0x10) || s->root.mode > 0xffff))
		return -1;
	for (i = 0; i < s->count; i++) {
		const save_entry_t *e = s->files + i;
		if (!saveNameValid(e->name, s->system == 1 ? 20 : 32) || (e->mode & 0x20) ||
		    e->size > SAVE_MAX_SIZE - total || (e->size && !e->data))
			return -1;
		if (s->system == 2 && e->mode && (!(e->mode & 0x10) || e->mode > 0xffff))
			return -1;
		total += e->size;
		for (j = 0; j < i; j++)
			if (!strcmp(e->name, s->files[j].name))
				return -1;
	}
	if (s->system == 1 && (s->files[0].size < PS1_BLOCK_SIZE || s->files[0].size > PS1_BLOCK_SIZE * 15 ||
	                       s->files[0].size % PS1_BLOCK_SIZE || (s->files[0].data[0] & ~0x20) != 'S' || (s->files[0].data[1] & ~0x20) != 'C'))
		return -1;
	return 0;
}

static void cbsCrypt(uint8_t *data, size_t size)
{
	uint8_t key[256] = {
	    0x5f, 0x1f, 0x85, 0x6f, 0x31, 0xaa, 0x3b, 0x18,
	    0x21, 0xb9, 0xce, 0x1c, 0x07, 0x4c, 0x9c, 0xb4,
	    0x81, 0xb8, 0xef, 0x98, 0x59, 0xae, 0xf9, 0x26,
	    0xe3, 0x80, 0xa3, 0x29, 0x2d, 0x73, 0x51, 0x62,
	    0x7c, 0x64, 0x46, 0xf4, 0x34, 0x1a, 0xf6, 0xe1,
	    0xba, 0x3a, 0x0d, 0x82, 0x79, 0x0a, 0x5c, 0x16,
	    0x71, 0x49, 0x8e, 0xac, 0x8c, 0x9f, 0x35, 0x19,
	    0x45, 0x94, 0x3f, 0x56, 0x0c, 0x91, 0x00, 0x0b,
	    0xd7, 0xb0, 0xdd, 0x39, 0x66, 0xa1, 0x76, 0x52,
	    0x13, 0x57, 0xf3, 0xbb, 0x4e, 0xe5, 0xdc, 0xf0,
	    0x65, 0x84, 0xb2, 0xd6, 0xdf, 0x15, 0x3c, 0x63,
	    0x1d, 0x89, 0x14, 0xbd, 0xd2, 0x36, 0xfe, 0xb1,
	    0xca, 0x8b, 0xa4, 0xc6, 0x9e, 0x67, 0x47, 0x37,
	    0x42, 0x6d, 0x6a, 0x03, 0x92, 0x70, 0x05, 0x7d,
	    0x96, 0x2f, 0x40, 0x90, 0xc4, 0xf1, 0x3e, 0x3d,
	    0x01, 0xf7, 0x68, 0x1e, 0xc3, 0xfc, 0x72, 0xb5,
	    0x54, 0xcf, 0xe7, 0x41, 0xe4, 0x4d, 0x83, 0x55,
	    0x12, 0x22, 0x09, 0x78, 0xfa, 0xde, 0xa7, 0x06,
	    0x08, 0x23, 0xbf, 0x0f, 0xcc, 0xc1, 0x97, 0x61,
	    0xc5, 0x4a, 0xe6, 0xa0, 0x11, 0xc2, 0xea, 0x74,
	    0x02, 0x87, 0xd5, 0xd1, 0x9d, 0xb7, 0x7e, 0x38,
	    0x60, 0x53, 0x95, 0x8d, 0x25, 0x77, 0x10, 0x5e,
	    0x9b, 0x7f, 0xd8, 0x6e, 0xda, 0xa2, 0x2e, 0x20,
	    0x4f, 0xcd, 0x8f, 0xcb, 0xbe, 0x5a, 0xe0, 0xed,
	    0x2c, 0x9a, 0xd4, 0xe2, 0xaf, 0xd0, 0xa9, 0xe8,
	    0xad, 0x7a, 0xbc, 0xa8, 0xf2, 0xee, 0xeb, 0xf5,
	    0xa6, 0x99, 0x28, 0x24, 0x6c, 0x2b, 0x75, 0x5d,
	    0xf8, 0xd3, 0x86, 0x17, 0xfb, 0xc0, 0x7b, 0xb3,
	    0x58, 0xdb, 0xc7, 0x4b, 0xff, 0x04, 0x50, 0xe9,
	    0x88, 0x69, 0xc9, 0x2a, 0xab, 0xfd, 0x5b, 0x1b,
	    0x8a, 0xd9, 0xec, 0x27, 0x44, 0x0e, 0x33, 0xc8,
	    0x6b, 0x93, 0x32, 0x48, 0xb6, 0x30, 0x43, 0xa5

	};
	size_t off;
	uint8_t x = 0, y = 0, t;
	for (off = 0; off < size; off++) {
		x++;
		y += key[x];
		t = key[x];
		key[x] = key[y];
		key[y] = t;
		data[off] ^= key[(uint8_t)(key[x] + key[y])];
	}
}

static int decodePsv(save_archive_t *s, const uint8_t *p, size_t n)
{
	uint32_t count, i, offset, length;
	if (n < 64)
		return -1;
	s->system = rd32(p + 60);
	if (s->system == 1) {
		if (n < 132 || rd32(p + 56) != 0x14 || rd32(p + 68) != 132 || rd32(p + 64) != n - 132 || rd32(p + 92) != n - 132 ||
		    nameRead(s->root.name, p + 100, 20, 20) < 0)
			return -1;
		s->count = 1;
		s->files[0] = s->root;
		s->files[0].size = n - 132;
		s->files[0].data = p + 132;
		return 0;
	}
	if (s->system != 2 || n < 160 || rd32(p + 56) != 0x2c)
		return -1;
	count = rd32(p + 100);
	if (!count || count > SAVE_MAX_FILES || rd32(p + 120) != count + 2 || !span(n, 160, count * 60) ||
	    nameRead(s->root.name, p + 128, 32, 32) < 0)
		return -1;
	s->count = count;
	s->root.mode = rd32(p + 124);
	memcpy(s->root.created, p + 104, 8);
	memcpy(s->root.modified, p + 112, 8);
	for (i = 0; i < count; i++) {
		const uint8_t *f = p + 160 + i * 60;
		save_entry_t *e = s->files + i;
		length = rd32(f + 16);
		offset = rd32(f + 56);
		if (offset < 160 + count * 60 || !span(n, offset, length) || nameRead(e->name, f + 24, 32, 32) < 0)
			return -1;
		memcpy(e->created, f, 8);
		memcpy(e->modified, f + 8, 8);
		e->size = length;
		e->mode = rd32(f + 20);
		e->data = p + offset;
	}
	return 0;
}

static int decodeMax(save_archive_t *s, const uint8_t *p, size_t n)
{
	uint32_t size, count, i, offset = 0;
	uint8_t *out;
	int result;
	if (n < 92 || nameRead(s->root.name, p + 16, 32, 32) < 0)
		return -1;
	size = rd32(p + 88);
	count = rd32(p + 84);
	if (!size || size > SAVE_MAX_SIZE || !count || count > SAVE_MAX_FILES)
		return -1;
	out = malloc(size);
	if (!out)
		return -1;
	result = unlzari((uint8_t *)p + 88, n - 88, out, size);
	free(s->storage);
	s->storage = out;
	if (result != (int)size)
		return -1;
	s->system = 2;
	s->count = count;
	s->root.mode = 0x8427;
	for (i = 0; i < count; i++) {
		save_entry_t *e = s->files + i;
		if (!span(size, offset, 36))
			return -1;
		e->size = rd32(out + offset);
		e->mode = 0x8497;
		if (nameRead(e->name, out + offset + 4, 32, 32) < 0)
			return -1;
		offset += 36;
		if (!span(size, offset, e->size))
			return -1;
		e->data = out + offset;
		offset = ((offset + e->size + 23) & ~15U) - 8;
	}
	return 0;
}

static int decodeCbs(save_archive_t *s, const uint8_t *p, size_t n)
{
	uLongf size;
	size_t offset = 0;
	uint8_t *out;
	if (n < 296 || rd32(p + 8) != 296 || nameRead(s->root.name, p + 20, 32, 32) < 0)
		return -1;
	size = rd32(p + 12);
	if (!size || size > SAVE_MAX_SIZE)
		return -1;
	s->system = 2;
	s->root.mode = rd32(p + 72);
	memcpy(s->root.created, p + 52, 8);
	memcpy(s->root.modified, p + 60, 8);
	out = malloc(size);
	if (!out)
		return -1;
	cbsCrypt(s->storage + 296, n - 296);
	if (uncompress(out, &size, s->storage + 296, n - 296) != Z_OK) {
		free(out);
		return -1;
	}
	free(s->storage);
	s->storage = out;
	while (offset < size) {
		save_entry_t *e;
		if (s->count == SAVE_MAX_FILES || !span(size, offset, 64))
			return -1;
		e = s->files + s->count++;
		memcpy(e->created, out + offset, 8);
		memcpy(e->modified, out + offset + 8, 8);
		e->size = rd32(out + offset + 16);
		e->mode = rd32(out + offset + 20);
		if (nameRead(e->name, out + offset + 32, 32, 32) < 0)
			return -1;
		offset += 64;
		if (!span(size, offset, e->size))
			return -1;
		e->data = out + offset;
		offset += e->size;
	}
	return 0;
}

static int xpsEntry(save_entry_t *e, const uint8_t *p)
{
	uint32_t mode = rd32(p + 78);
	if (nameRead(e->name, p + 2, 64, 32) < 0)
		return -1;
	e->size = rd32(p + 66);
	e->mode = ((mode & 255) << 8) | ((mode >> 8) & 255);
	memcpy(e->created, p + 82, 8);
	memcpy(e->modified, p + 90, 8);
	return 0;
}

static int decodeXps(save_archive_t *s, const uint8_t *p, size_t n)
{
	size_t off = 21, header;
	uint32_t length, i, count;
	for (i = 0; i < 2; i++) {
		if (!span(n, off, 4))
			return -1;
		length = rd32(p + off);
		off += 4;
		if (!span(n, off, length))
			return -1;
		off += length;
	}
	if (!span(n, off, 8 + 250))
		return -1;
	off += 8;
	header = rd16(p + off);
	if (header < 250 || !span(n, off, header) || xpsEntry(&s->root, p + off) < 0)
		return -1;
	count = s->root.size;
	if (count < 3 || count > SAVE_MAX_FILES + 2)
		return -1;
	s->system = 2;
	s->count = count - 2;
	off += header;
	for (i = 0; i < (uint32_t)s->count; i++) {
		save_entry_t *e = s->files + i;
		if (!span(n, off, 250))
			return -1;
		header = rd16(p + off);
		if (header < 250 || !span(n, off, header) || xpsEntry(e, p + off) < 0)
			return -1;
		off += header;
		if (!span(n, off, e->size))
			return -1;
		e->data = p + off;
		off += e->size;
	}
	return 0;
}

int saveDecode(const uint8_t *data, size_t size, const char *filename, save_archive_t *s)
{
	const uint8_t *p;
	int result = -1;
	size_t off = 0;
	memset(s, 0, sizeof(*s));
	if (!data || size < 2 || size > SAVE_MAX_SIZE)
		return -1;
	s->files = calloc(SAVE_MAX_FILES, sizeof(*s->files));
	s->storage = malloc(size);
	if (!s->files || !s->storage)
		return -1;
	memcpy(s->storage, data, size);
	p = s->storage;
	if (size >= 4 && !memcmp(p, "\0VSP", 4))
		result = decodePsv(s, p, size);
	else if (size >= 12 && !memcmp(p, "Ps2PowerSave", 12))
		result = decodeMax(s, p, size);
	else if (size >= 4 && !memcmp(p, "CFU\0", 4))
		result = decodeCbs(s, p, size);
	else if (size >= 21 && !memcmp(p + 4, "SharkPortSave\0\0\0", 16))
		result = decodeXps(s, p, size);
	else {
		s->system = 1;
		s->count = 1;
		if (size >= 128 && p[0] == 0x51) {
			off = 128;
			if (nameRead(s->root.name, p + 10, 20, 20) < 0 || rd32(p + 4) != size - 128)
				return -1;
		} else if (size >= 56 && p[54] == 'S' && p[55] == 'C') {
			off = 54;
			if (nameRead(s->root.name, p, 20, 20) < 0)
				return -1;
		} else {
			const char *base = filename ? strrchr(filename, '/') : NULL;
			base = base ? base + 1 : filename;
			if (!base || !saveNameValid(base, 20))
				return -1;
			strcpy(s->root.name, base);
		}
		s->files[0] = s->root;
		s->files[0].data = p + off;
		s->files[0].size = size - off;
		result = 0;
	}
	return result < 0 ? -1 : saveArchiveValidate(s);
}

void savePsvName(char *out, size_t capacity, const char *name)
{
	size_t i, n = 0;
	if (!capacity)
		return;
	for (i = 0; name[i] && n + 5 < capacity; i++) {
		if (i < 12)
			out[n++] = name[i];
		else {
			snprintf(out + n, capacity - n, "%02X", (unsigned char)name[i]);
			n += 2;
		}
	}
	if (n + 5 <= capacity)
		memcpy(out + n, ".PSV", 5);
	else
		out[0] = 0;
}

static void psvHeader(uint8_t *p, int system)
{
	memcpy(p, "\0VSP", 4);
	memcpy(p + 8, "www.bucanero.com.ar", 19);
	wr32(p + 56, system == 1 ? 0x14 : 0x2c);
	wr32(p + 60, system);
}

int saveEncode(const save_archive_t *s, int format, uint8_t **out, size_t *size)
{
	uint8_t *p;
	size_t n, off;
	int i, j;
	const save_entry_t *e;
	*out = NULL;
	*size = 0;
	if (saveArchiveValidate(s) < 0 || (s->system == 2 && format != SAVE_PSV))
		return -1;
	if (s->system == 1) {
		off = format == SAVE_PSV ? 132 : format == SAVE_MCS ? 128 :
		                             format == SAVE_PSX     ? 54 :
		                                                      0;
		if (format < SAVE_PSV || format > SAVE_RAW)
			return -1;
		n = off + s->files[0].size;
		p = calloc(1, n);
		if (!p)
			return -1;
		if (format == SAVE_PSV) {
			psvHeader(p, 1);
			wr32(p + 64, s->files[0].size);
			wr32(p + 68, 132);
			wr32(p + 72, 512);
			wr32(p + 92, s->files[0].size);
			wr32(p + 96, 0x9003);
			memcpy(p + 100, s->root.name, strlen(s->root.name));
		} else if (format == SAVE_MCS) {
			p[0] = 0x51;
			wr32(p + 4, s->files[0].size);
			p[8] = p[9] = 255;
			memcpy(p + 10, s->root.name, strlen(s->root.name));
			for (i = 0; i < 127; i++)
				p[127] ^= p[i];
		} else if (format == SAVE_PSX)
			memcpy(p, s->root.name, strlen(s->root.name));
		memcpy(p + off, s->files[0].data, s->files[0].size);
	} else {
		off = 160 + s->count * 60;
		n = off;
		for (i = 0; i < s->count; i++)
			n += s->files[i].size;
		if (n > SAVE_MAX_SIZE)
			return -1;
		p = calloc(1, n);
		if (!p)
			return -1;
		psvHeader(p, 2);
		wr32(p + 100, s->count);
		memcpy(p + 104, s->root.created, 8);
		memcpy(p + 112, s->root.modified, 8);
		wr32(p + 120, s->count + 2);
		wr32(p + 124, s->root.mode ? s->root.mode : 0x8427);
		memcpy(p + 128, s->root.name, strlen(s->root.name));
		for (i = 0; i < s->count; i++) {
			uint8_t *f = p + 160 + i * 60;
			e = s->files + i;
			memcpy(f, e->created, 8);
			memcpy(f + 8, e->modified, 8);
			wr32(f + 16, e->size);
			wr32(f + 20, e->mode ? e->mode : 0x8497);
			memcpy(f + 24, e->name, strlen(e->name));
			wr32(f + 56, off);
			memcpy(p + off, e->data, e->size);
			wr32(p + 64, rd32(p + 64) + e->size);
			if (!strcmp(e->name, "icon.sys")) {
				wr32(p + 68, off);
				wr32(p + 72, e->size);
			}
			off += e->size;
		}
		/* icon.sys stores the view/copy/delete filenames at 0x104/0x144/0x184. */
		for (i = 0; i < s->count; i++)
			if (!strcmp(s->files[i].name, "icon.sys") && s->files[i].size >= 0x1c4) {
				for (j = 0; j < 3; j++) {
					char name[33];
					int k;
					if (nameRead(name, s->files[i].data + 0x104 + j * 64, 64, 32) < 0)
						continue;
					for (k = 0; k < s->count; k++)
						if (!strcmp(name, s->files[k].name)) {
							wr32(p + 76 + j * 8, rd32(p + 160 + k * 60 + 56));
							wr32(p + 80 + j * 8, s->files[k].size);
						}
				}
			}
	}
	if (format == SAVE_PSV)
		saveSign(p, n, 0);
	*out = p;
	*size = n;
	return 0;
}

static void checksum(uint8_t *p)
{
	int i;
	p[127] = 0;
	for (i = 0; i < 127; i++)
		p[127] ^= p[i];
}

void ps1CardFormat(uint8_t *raw)
{
	int i;
	memset(raw, 0, PS1_CARD_SIZE);
	raw[0] = 'M';
	raw[1] = 'C';
	checksum(raw);
	for (i = 1; i <= 15; i++) {
		uint8_t *h = raw + i * 128;
		h[0] = 0xa0;
		h[8] = h[9] = 255;
		checksum(h);
	}
	for (i = 16; i < 36; i++) {
		uint8_t *h = raw + i * 128;
		memset(h, 255, 4);
		h[8] = h[9] = 255;
		checksum(h);
	}
}

static int chain(const uint8_t *raw, int slot, int slots[15])
{
	unsigned int used = 0;
	int n = 0, next = slot;
	uint32_t length;
	if (slot < 0 || slot >= 15 || raw[(slot + 1) * 128] != 0x51)
		return -1;
	length = rd32(raw + (slot + 1) * 128 + 4);
	while (next != 0xffff) {
		const uint8_t *h;
		if (next < 0 || next >= 15 || (used & (1U << next)) || n == 15)
			return -1;
		h = raw + (next + 1) * 128;
		used |= 1U << next;
		slots[n++] = next;
		if (h[0] != (n == 1 ? 0x51 : rd16(h + 8) == 0xffff ? 0x53 :
		                                                     0x52))
			return -1;
		next = rd16(h + 8);
	}
	return length == (uint32_t)n * PS1_BLOCK_SIZE ? n : -1;
}

int ps1CardValidate(const uint8_t *raw)
{
	unsigned int used = 0;
	int i, j, n, slots[15];
	char name[33];
	uint8_t c = 0;
	if (memcmp(raw, "MC", 2))
		return -1;
	for (i = 0; i < 128; i++)
		c ^= raw[i];
	if (c)
		return -1;
	for (i = 0; i < 15; i++)
		if (raw[(i + 1) * 128] == 0x51) {
			if (nameRead(name, raw + (i + 1) * 128 + 10, 20, 20) < 0 || (n = chain(raw, i, slots)) < 0)
				return -1;
			for (j = 0; j < n; j++) {
				if (used & (1U << slots[j]))
					return -1;
				used |= 1U << slots[j];
			}
			for (j = 0; j < i; j++)
				if (raw[(j + 1) * 128] == 0x51 && !strncmp((const char *)raw + (j + 1) * 128 + 10, name, 20))
					return -1;
		}
	for (i = 0; i < 15; i++) {
		uint8_t mode = raw[(i + 1) * 128];
		c = 0;
		for (j = 0; j < 128; j++)
			c ^= raw[(i + 1) * 128 + j];
		if (c)
			return -1;
		if (mode == 0x51 || mode == 0x52 || mode == 0x53) {
			if (!(used & (1U << i)))
				return -1;
		} else if (mode != 0xa0 && mode != 0xa1 && mode != 0xa2 && mode != 0xa3)
			return -1;
	}
	return 0;
}

int ps1CardDecode(const uint8_t *data, size_t size, uint8_t *raw, int *format)
{
	size_t off = 0;
	uint8_t *tmp = NULL;
	int f = CARD_RAW, result = -1;
	if (size == PS1_CARD_SIZE && !memcmp(data, "MC", 2))
		off = 0;
	else if (size == 0x20f40 && !memcmp(data, "123-456-STD", 11)) {
		off = 3904;
		f = CARD_GME;
	} else if (size == 0x20040 && !memcmp(data, "VgsM", 4)) {
		off = 64;
		f = CARD_VGS;
	} else if (size == 0x20080 && !memcmp(data, "\0PMV", 4) && rd32(data + 4) == 128) {
		off = 128;
		f = CARD_VMP;
	} else if (size == 0x200a0) {
		uint8_t digest[32];
		tmp = malloc(size);
		if (!tmp)
			return -1;
		memcpy(tmp, data, size);
		saveMcxCrypt(tmp, size, 0);
		saveSha256(tmp, 0x20080, digest);
		if (memcmp(digest, tmp + 0x20080, 32))
			goto done;
		data = tmp;
		off = 128;
		f = CARD_MCX;
	} else
		return -1;
	if (!ps1CardValidate(data + off)) {
		memcpy(raw, data + off, PS1_CARD_SIZE);
		if (format)
			*format = f;
		result = 0;
	}
done:
	free(tmp);
	return result;
}

int ps1CardEncode(const uint8_t *raw, int format, const uint8_t *original, size_t original_size, uint8_t **out, size_t *size)
{
	size_t off = 0, n = PS1_CARD_SIZE;
	uint8_t *p;
	int i;
	*out = NULL;
	*size = 0;
	if (ps1CardValidate(raw) < 0)
		return -1;
	switch (format) {
		case CARD_RAW:
		case CARD_VM1:
			break;
		case CARD_GME:
			off = 3904;
			break;
		case CARD_VGS:
			off = 64;
			break;
		case CARD_VMP:
		case CARD_MCX:
			off = 128;
			break;
		default:
			return -1;
	}
	n += off + (format == CARD_MCX ? 32 : 0);
	p = calloc(1, n);
	if (!p)
		return -1;
	if (original && original_size == n && format != CARD_MCX)
		memcpy(p, original, off);
	memcpy(p + off, raw, PS1_CARD_SIZE);
	if (format == CARD_GME) {
		memcpy(p, "123-456-STD", 11);
		p[18] = 1;
		p[20] = 1;
		p[21] = 'M';
		for (i = 0; i < 15; i++) {
			p[22 + i] = raw[(i + 1) * 128];
			p[38 + i] = raw[(i + 1) * 128 + 8];
		}
	} else if (format == CARD_VGS) {
		memcpy(p, "VgsM", 4);
		wr32(p + 4, 1);
		wr32(p + 8, 1);
		wr32(p + 12, 1);
		wr32(p + 16, 128);
	} else if (format == CARD_VMP) {
		memcpy(p, "\0PMV", 4);
		wr32(p + 4, 128);
		saveSign(p, n, 1);
	} else if (format == CARD_MCX) {
		saveSha256(p, 0x20080, p + 0x20080);
		saveMcxCrypt(p, n, 1);
	}
	*out = p;
	*size = n;
	return 0;
}

int ps1CardFind(const uint8_t *raw, const char *name)
{
	int i;
	for (i = 0; i < 15; i++)
		if (raw[(i + 1) * 128] == 0x51 && !strncmp((const char *)raw + (i + 1) * 128 + 10, name, 20))
			return i;
	return -1;
}

int ps1CardExtract(const uint8_t *raw, int slot, save_archive_t *s)
{
	int slots[15], n, i;
	memset(s, 0, sizeof(*s));
	if ((n = chain(raw, slot, slots)) < 0)
		return -1;
	s->files = calloc(1, sizeof(*s->files));
	s->storage = malloc(n * PS1_BLOCK_SIZE);
	if (!s->files || !s->storage)
		return -1;
	if (nameRead(s->root.name, raw + (slot + 1) * 128 + 10, 20, 20) < 0)
		return -1;
	s->system = 1;
	s->count = 1;
	s->files[0] = s->root;
	s->files[0].size = n * PS1_BLOCK_SIZE;
	s->files[0].data = s->storage;
	for (i = 0; i < n; i++)
		memcpy(s->storage + i * PS1_BLOCK_SIZE, raw + (slots[i] + 1) * PS1_BLOCK_SIZE, PS1_BLOCK_SIZE);
	return saveArchiveValidate(s);
}

int ps1CardFreeBlocks(const uint8_t *raw)
{
	int i, n = 0;
	for (i = 0; i < 15; i++)
		if ((raw[(i + 1) * 128] & 0xf0) == 0xa0)
			n++;
	return n;
}

int ps1CardPut(uint8_t *raw, const save_archive_t *s)
{
	int slots[15], n, i, j = 0;
	if (s->system != 1 || saveArchiveValidate(s) < 0 || ps1CardValidate(raw) < 0 || ps1CardFind(raw, s->root.name) >= 0)
		return -1;
	n = s->files[0].size / PS1_BLOCK_SIZE;
	if (ps1CardFreeBlocks(raw) < n)
		return -1;
	/* Reclaim deleted chains together, so no stale chain points into a new save. */
	for (i = 0; i < 15; i++)
		if ((raw[(i + 1) * 128] & 0xf0) == 0xa0) {
			uint8_t *h = raw + (i + 1) * 128;
			memset(h, 0, 128);
			h[0] = 0xa0;
			h[8] = h[9] = 255;
			checksum(h);
			if (j < n)
				slots[j++] = i;
		}
	for (i = 0; i < n; i++) {
		uint8_t *h = raw + (slots[i] + 1) * 128;
		memset(h, 0, 128);
		h[0] = i == 0 ? 0x51 : i == n - 1 ? 0x53 :
		                                    0x52;
		wr16(h + 8, i == n - 1 ? 0xffff : slots[i + 1]);
		if (i == 0) {
			wr32(h + 4, s->files[0].size);
			memcpy(h + 10, s->root.name, strlen(s->root.name));
		}
		checksum(h);
		memcpy(raw + (slots[i] + 1) * PS1_BLOCK_SIZE, s->files[0].data + i * PS1_BLOCK_SIZE, PS1_BLOCK_SIZE);
	}
	return 0;
}

int ps1CardDelete(uint8_t *raw, const char *name)
{
	int slots[15], n, i, slot = ps1CardFind(raw, name);
	if (slot < 0 || (n = chain(raw, slot, slots)) < 0)
		return -1;
	for (i = 0; i < n; i++) {
		uint8_t *h = raw + (slots[i] + 1) * 128;
		memset(h, 0, 128);
		h[0] = 0xa0;
		h[8] = h[9] = 255;
		checksum(h);
	}
	return 0;
}

int ps1CardRename(uint8_t *raw, const char *old_name, const char *new_name)
{
	int slot = ps1CardFind(raw, old_name);
	uint8_t *h;
	if (slot < 0 || !saveNameValid(new_name, 20) || ps1CardFind(raw, new_name) >= 0)
		return -1;
	h = raw + (slot + 1) * 128;
	memset(h + 10, 0, 20);
	memcpy(h + 10, new_name, strlen(new_name));
	checksum(h);
	return 0;
}
