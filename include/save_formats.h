#ifndef SAVE_FORMATS_H
#define SAVE_FORMATS_H

#include <stddef.h>
#include <stdint.h>

#define SAVE_MAX_SIZE (16U * 1024U * 1024U)
#define SAVE_MAX_FILES 1024
#define PS1_CARD_SIZE 131072
#define PS1_BLOCK_SIZE 8192
#define PS1_MAX_BLOCKS 15

enum {
	SAVE_PSV = 1,
	SAVE_MCS,
	SAVE_PSX,
	SAVE_RAW,
	CARD_RAW,
	CARD_VM1,
	CARD_GME,
	CARD_VGS,
	CARD_VMP,
	CARD_MCX
};

typedef struct
{
	char name[33];
	uint32_t size, mode;
	uint8_t created[8], modified[8];
	const uint8_t *data;
} save_entry_t;

typedef struct
{
	int system, count;
	save_entry_t root;
	save_entry_t *files;
	uint8_t *storage;
} save_archive_t;

/* All parsers validate names, lengths and offsets before exposing payloads.
 * Archives own files/storage; free with saveArchiveFree, including on error. */
int saveNameValid(const char *name, size_t limit);
int saveArchiveValidate(const save_archive_t *save);
int saveDecode(const uint8_t *data, size_t size, const char *filename, save_archive_t *save);
void saveArchiveFree(save_archive_t *save);
int saveEncode(const save_archive_t *save, int format, uint8_t **data, size_t *size);
void savePsvName(char *out, size_t capacity, const char *name);

/* PS1 card containers retain their original headers/comments on edits. */
int ps1CardDecode(const uint8_t *data, size_t size, uint8_t *raw, int *format);
int ps1CardEncode(const uint8_t *raw, int format, const uint8_t *original,
                  size_t original_size, uint8_t **data, size_t *size);
void ps1CardFormat(uint8_t *raw);
int ps1CardValidate(const uint8_t *raw);
int ps1CardExtract(const uint8_t *raw, int slot, save_archive_t *save);
int ps1CardFind(const uint8_t *raw, const char *name);
int ps1CardPut(uint8_t *raw, const save_archive_t *save);
int ps1CardDelete(uint8_t *raw, const char *name);
int ps1CardRename(uint8_t *raw, const char *old_name, const char *new_name);
int ps1CardFreeBlocks(const uint8_t *raw);

/* Format signing and MCX encryption; no platform or external crypto dependency. */
void saveSign(uint8_t *data, size_t size, int vmp);
void saveMcxCrypt(uint8_t *data, size_t size, int encrypt);
void saveSha256(const uint8_t *data, size_t size, uint8_t out[32]);

#endif
