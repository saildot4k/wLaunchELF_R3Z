#ifdef KELF_SIGNER_HOST_TEST
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define FIO_O_RDONLY O_RDONLY
#define genOpen open
#define genRead read
#define genClose close
#define genLseek lseek
#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

static void *host_memalign(size_t alignment, size_t size)
{
	void *ptr = NULL;

	if (posix_memalign(&ptr, alignment, size) != 0)
		return NULL;
	return ptr;
}
#define memalign host_memalign
#else
#include "launchelf.h"
#endif

#include "kelf_signer.h"

#define KELF_HEADER_SIZE 32
#define KELF_USER_HEADER_SIZE 16
#define KELF_SIGNATURE_SIZE 8
#define KELF_KEY_SIZE 16
#define KELF_CONTENT_TRAILING_SCAN 0x18
#define KELF_CONTENT_EXTRA_BLOCKS 3
#define KELF_ENCRYPTED_TAIL_SIZE 0x10
#define KELF_MG_ZONES_ALL 0xff
#define KELF_APPLICATION_XOSDMAIN 1
#define KELF_FLAGS_KELF 0x022c
#define KELF_BIT_BLOCK_ENCRYPTED 0x01
#define KELF_BIT_BLOCK_SIGNED 0x02
#define KELF_BLOCK_COUNT 2
#define KELF_BIT_TABLE_SIZE ((KELF_BLOCK_COUNT * 2 + 1) * 8)
#define KELF_OUTPUT_HEADER_SIZE (KELF_HEADER_SIZE + KELF_SIGNATURE_SIZE + KELF_KEY_SIZE + KELF_KEY_SIZE + KELF_BIT_TABLE_SIZE + KELF_SIGNATURE_SIZE + KELF_SIGNATURE_SIZE)

#define KELF_KEY_FIELD_COUNT 12
#define KELF_KEY_FIELD_REQUIRED_COUNT 10

typedef struct __attribute__((packed))
{
	u8 user_defined[KELF_USER_HEADER_SIZE];
	u32 content_size;
	u16 header_size;
	u8 system_type;
	u8 application_type;
	u16 flags;
	u16 bit_count;
	u8 mg_zones;
	u8 gap[3];
} KelfHeader;

typedef struct __attribute__((packed))
{
	u32 size;
	u32 flags;
	u8 signature[KELF_SIGNATURE_SIZE];
} KelfBitBlock;

typedef struct __attribute__((packed))
{
	u32 header_size;
	u8 block_count;
	u8 gap[3];
	KelfBitBlock blocks[KELF_BLOCK_COUNT];
} KelfBitTable;

typedef struct
{
	u8 value[KELF_KEY_FIELD_COUNT][KELF_KEY_SIZE];
	u8 present[KELF_KEY_FIELD_COUNT];
} KelfParsedKeys;

typedef struct
{
	u8 sig_master[8];
	u8 sig_hash[8];
	u8 kbit_master[16];
	u8 kbit_iv[8];
	u8 kc_master[16];
	u8 kc_iv[8];
	u8 rootsig_master[8];
	u8 rootsig_hash[16];
	u8 content_table_iv[8];
	u8 content_iv[8];
	u8 override_kbit[16];
	u8 override_kc[16];
	int has_override;
} KelfKeyStore;

typedef struct
{
	const char *name;
	int field;
	int size;
} KelfKeySpec;

enum {
	KEY_SIG_MASTER,
	KEY_SIG_HASH,
	KEY_KBIT_MASTER,
	KEY_KBIT_IV,
	KEY_KC_MASTER,
	KEY_KC_IV,
	KEY_ROOTSIG_MASTER,
	KEY_ROOTSIG_HASH,
	KEY_CONTENT_TABLE_IV,
	KEY_CONTENT_IV,
	KEY_OVERRIDE_KBIT,
	KEY_OVERRIDE_KC
};

static const KelfKeySpec key_specs[] = {
    {"MG_SIG_MASTER_KEY", KEY_SIG_MASTER, 8},
    {"MG_SIG_HASH_KEY", KEY_SIG_HASH, 8},
    {"MG_KBIT_MASTER_KEY", KEY_KBIT_MASTER, 16},
    {"MG_KBIT_IV", KEY_KBIT_IV, 8},
    {"MG_KC_MASTER_KEY", KEY_KC_MASTER, 16},
    {"MG_KC_IV", KEY_KC_IV, 8},
    {"MG_ROOTSIG_MASTER_KEY", KEY_ROOTSIG_MASTER, 8},
    {"MG_ROOTSIG_HASH_KEY", KEY_ROOTSIG_HASH, 16},
    {"MG_CONTENT_TABLE_IV", KEY_CONTENT_TABLE_IV, 8},
    {"MG_CONTENT_IV", KEY_CONTENT_IV, 8},
    {"OVERRIDE_KBIT", KEY_OVERRIDE_KBIT, 16},
    {"OVERRIDE_KC", KEY_OVERRIDE_KC, 16},
};

static const u8 user_header_fmcb[KELF_USER_HEADER_SIZE] = {
    0x01, 0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x4a,
    0x00, 0x01, 0x02, 0x19, 0x00, 0x00, 0x00, 0x56};

static const u8 user_header_dnasload[KELF_USER_HEADER_SIZE] = {
    0x01, 0x00, 0x00, 0x04, 0x00, 0x06, 0x00, 0x4a,
    0x00, 0x0e, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02};

static const u8 user_header_dongle[KELF_USER_HEADER_SIZE] = {
    0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00};

static const u8 user_kbit_fmcb[KELF_KEY_SIZE] = {
    0x24, 0x25, 0x1d, 0x05, 0xd1, 0x5e, 0x2d, 0x7d,
    0x94, 0x3f, 0x4a, 0x30, 0x3f, 0x28, 0x24, 0xdb};

static const u8 des_ip[64] = {
    58, 50, 42, 34, 26, 18, 10, 2,
    60, 52, 44, 36, 28, 20, 12, 4,
    62, 54, 46, 38, 30, 22, 14, 6,
    64, 56, 48, 40, 32, 24, 16, 8,
    57, 49, 41, 33, 25, 17, 9, 1,
    59, 51, 43, 35, 27, 19, 11, 3,
    61, 53, 45, 37, 29, 21, 13, 5,
    63, 55, 47, 39, 31, 23, 15, 7};

static const u8 des_fp[64] = {
    40, 8, 48, 16, 56, 24, 64, 32,
    39, 7, 47, 15, 55, 23, 63, 31,
    38, 6, 46, 14, 54, 22, 62, 30,
    37, 5, 45, 13, 53, 21, 61, 29,
    36, 4, 44, 12, 52, 20, 60, 28,
    35, 3, 43, 11, 51, 19, 59, 27,
    34, 2, 42, 10, 50, 18, 58, 26,
    33, 1, 41, 9, 49, 17, 57, 25};

static const u8 des_e[48] = {
    32, 1, 2, 3, 4, 5,
    4, 5, 6, 7, 8, 9,
    8, 9, 10, 11, 12, 13,
    12, 13, 14, 15, 16, 17,
    16, 17, 18, 19, 20, 21,
    20, 21, 22, 23, 24, 25,
    24, 25, 26, 27, 28, 29,
    28, 29, 30, 31, 32, 1};

static const u8 des_p[32] = {
    16, 7, 20, 21,
    29, 12, 28, 17,
    1, 15, 23, 26,
    5, 18, 31, 10,
    2, 8, 24, 14,
    32, 27, 3, 9,
    19, 13, 30, 6,
    22, 11, 4, 25};

static const u8 des_pc1[56] = {
    57, 49, 41, 33, 25, 17, 9,
    1, 58, 50, 42, 34, 26, 18,
    10, 2, 59, 51, 43, 35, 27,
    19, 11, 3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15,
    7, 62, 54, 46, 38, 30, 22,
    14, 6, 61, 53, 45, 37, 29,
    21, 13, 5, 28, 20, 12, 4};

static const u8 des_pc2[48] = {
    14, 17, 11, 24, 1, 5,
    3, 28, 15, 6, 21, 10,
    23, 19, 12, 4, 26, 8,
    16, 7, 27, 20, 13, 2,
    41, 52, 31, 37, 47, 55,
    30, 40, 51, 45, 33, 48,
    44, 49, 39, 56, 34, 53,
    46, 42, 50, 36, 29, 32};

static const u8 des_shifts[16] = {
    1, 1, 2, 2, 2, 2, 2, 2,
    1, 2, 2, 2, 2, 2, 2, 1};

static const u8 des_sbox[8][64] = {
    {
        14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7,
        0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8,
        4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0,
        15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13},
    {
        15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10,
        3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5,
        0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15,
        13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9},
    {
        10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8,
        13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1,
        13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7,
        1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12},
    {
        7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15,
        13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9,
        10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4,
        3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14},
    {
        2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9,
        14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6,
        4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14,
        11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3},
    {
        12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11,
        10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8,
        9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6,
        4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13},
    {
        4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1,
        13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6,
        1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2,
        6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12},
    {
        13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7,
        1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2,
        7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8,
        2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11}};

static int kelf_stricmp(const char *lhs, const char *rhs)
{
	unsigned char a;
	unsigned char b;

	while (*lhs && *rhs) {
		a = (unsigned char)*lhs++;
		b = (unsigned char)*rhs++;
		if (a >= 'A' && a <= 'Z')
			a += 'a' - 'A';
		if (b >= 'A' && b <= 'Z')
			b += 'a' - 'A';
		if (a != b)
			return (int)a - (int)b;
	}

	a = (unsigned char)*lhs;
	b = (unsigned char)*rhs;
	if (a >= 'A' && a <= 'Z')
		a += 'a' - 'A';
	if (b >= 'A' && b <= 'Z')
		b += 'a' - 'A';
	return (int)a - (int)b;
}

static u64 bytes_to_u64_be(const u8 in[8])
{
	u64 v = 0;
	int i;

	for (i = 0; i < 8; i++)
		v = (v << 8) | in[i];
	return v;
}

static void u64_to_bytes_be(u64 v, u8 out[8])
{
	int i;

	for (i = 7; i >= 0; i--) {
		out[i] = (u8)(v & 0xff);
		v >>= 8;
	}
}

static u64 des_permute(u64 input, const u8 *table, int out_bits, int in_bits)
{
	u64 output = 0;
	int i;

	for (i = 0; i < out_bits; i++) {
		output <<= 1;
		output |= (input >> (in_bits - table[i])) & 1;
	}

	return output;
}

static u32 rotl28(u32 v, int bits)
{
	return ((v << bits) | (v >> (28 - bits))) & 0x0fffffff;
}

static void des_make_subkeys(const u8 key[8], u64 subkeys[16])
{
	u64 key64;
	u64 key56;
	u32 c;
	u32 d;
	int i;

	key64 = bytes_to_u64_be(key);
	key56 = des_permute(key64, des_pc1, 56, 64);
	c = (u32)((key56 >> 28) & 0x0fffffff);
	d = (u32)(key56 & 0x0fffffff);

	for (i = 0; i < 16; i++) {
		u64 cd;

		c = rotl28(c, des_shifts[i]);
		d = rotl28(d, des_shifts[i]);
		cd = ((u64)c << 28) | d;
		subkeys[i] = des_permute(cd, des_pc2, 48, 56);
	}
}

static u32 des_f(u32 r, u64 subkey)
{
	u64 expanded;
	u32 s_out = 0;
	int i;

	expanded = des_permute((u64)r, des_e, 48, 32) ^ subkey;
	for (i = 0; i < 8; i++) {
		u8 six = (u8)((expanded >> (42 - 6 * i)) & 0x3f);
		int row = ((six & 0x20) >> 4) | (six & 1);
		int col = (six >> 1) & 0x0f;

		s_out = (s_out << 4) | des_sbox[i][row * 16 + col];
	}

	return (u32)des_permute((u64)s_out, des_p, 32, 32);
}

static void des_crypt_block(const u8 input[8], u8 output[8], const u64 subkeys[16], int decrypt)
{
	u64 block;
	u64 ip;
	u32 l;
	u32 r;
	int i;

	block = bytes_to_u64_be(input);
	ip = des_permute(block, des_ip, 64, 64);
	l = (u32)(ip >> 32);
	r = (u32)(ip & 0xffffffff);

	for (i = 0; i < 16; i++) {
		u32 prev_r = r;
		u64 subkey = decrypt ? subkeys[15 - i] : subkeys[i];

		r = l ^ des_f(r, subkey);
		l = prev_r;
	}

	u64_to_bytes_be(des_permute(((u64)r << 32) | l, des_fp, 64, 64), output);
}

static void des_cbc_crypt(u8 *dst, const u8 *src, int size, const u8 key[8], const u8 iv[8], int decrypt)
{
	u64 subkeys[16];
	u8 chain[8];
	u8 in[8];
	u8 out[8];
	int pos;
	int i;

	des_make_subkeys(key, subkeys);
	memcpy(chain, iv, sizeof(chain));

	for (pos = 0; pos < size; pos += 8) {
		memcpy(in, src + pos, sizeof(in));
		if (decrypt) {
			des_crypt_block(in, out, subkeys, 1);
			for (i = 0; i < 8; i++)
				dst[pos + i] = out[i] ^ chain[i];
			memcpy(chain, in, sizeof(chain));
		} else {
			for (i = 0; i < 8; i++)
				in[i] ^= chain[i];
			des_crypt_block(in, out, subkeys, 0);
			memcpy(dst + pos, out, sizeof(out));
			memcpy(chain, out, sizeof(chain));
		}
	}
}

static int tdes_cbc_crypt(u8 *dst, const u8 *src, int size, const u8 *keys, int key_count, const u8 iv[8], int decrypt)
{
	u8 chain[8];
	u8 in[8];
	u8 stage1[8];
	u8 stage2[8];
	u8 stage3[8];
	u8 tmp_iv[8] = {0};
	int pos;
	int i;

	if (size < 0 || (size & 7) != 0)
		return -EINVAL;
	if (key_count < 1 || key_count > 3)
		return -EINVAL;

	memcpy(chain, iv, sizeof(chain));
	for (pos = 0; pos < size; pos += 8) {
		memcpy(in, src + pos, sizeof(in));
		if (decrypt) {
			des_cbc_crypt(stage1, in, 8, keys, tmp_iv, 1);
			if (key_count == 1)
				memcpy(stage3, stage1, sizeof(stage3));
			else {
				des_cbc_crypt(stage2, stage1, 8, keys + 8, tmp_iv, 0);
				if (key_count == 2)
					des_cbc_crypt(stage3, stage2, 8, keys, tmp_iv, 1);
				else {
					des_cbc_crypt(stage1, stage2, 8, keys + 16, tmp_iv, 1);
					memcpy(stage3, stage1, sizeof(stage3));
				}
			}
			for (i = 0; i < 8; i++)
				dst[pos + i] = stage3[i] ^ chain[i];
			memcpy(chain, in, sizeof(chain));
		} else {
			for (i = 0; i < 8; i++)
				in[i] ^= chain[i];
			des_cbc_crypt(stage1, in, 8, keys, tmp_iv, 0);
			if (key_count == 1)
				memcpy(stage3, stage1, sizeof(stage3));
			else {
				des_cbc_crypt(stage2, stage1, 8, keys + 8, tmp_iv, 1);
				if (key_count == 2)
					des_cbc_crypt(stage3, stage2, 8, keys, tmp_iv, 0);
				else {
					des_cbc_crypt(stage1, stage2, 8, keys + 16, tmp_iv, 0);
					memcpy(stage3, stage1, sizeof(stage3));
				}
			}
			memcpy(dst + pos, stage3, sizeof(stage3));
			memcpy(chain, stage3, sizeof(chain));
		}
	}

	return 0;
}

static char *trim_space(char *s)
{
	char *end;

	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		s++;

	end = s + strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
		*--end = '\0';

	return s;
}

static int hex_value(int ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	return -1;
}

static int decode_hex_key(const char *src, u8 *out, int expected_size)
{
	int nibbles = 0;
	int high = -1;
	int out_pos = 0;

	while (*src != '\0') {
		int v;

		if (*src == ';' || *src == '#')
			break;
		if (*src == ' ' || *src == '\t' || *src == '\r' || *src == '\n') {
			src++;
			continue;
		}

		v = hex_value((unsigned char)*src++);
		if (v < 0)
			return -EINVAL;

		if (high < 0)
			high = v;
		else {
			if (out_pos >= expected_size)
				return -E2BIG;
			out[out_pos++] = (u8)((high << 4) | v);
			high = -1;
		}
		nibbles++;
	}

	if (nibbles == 0)
		return 0;
	if (high >= 0)
		return -EINVAL;
	if (out_pos != expected_size)
		return -EINVAL;

	return out_pos;
}

static const KelfKeySpec *find_key_spec(const char *name)
{
	unsigned int i;

	for (i = 0; i < sizeof(key_specs) / sizeof(key_specs[0]); i++) {
		if (!kelf_stricmp(name, key_specs[i].name))
			return &key_specs[i];
	}

	return NULL;
}

static int load_text_file(const char *path, char **out, int *out_size)
{
	char *buffer;
	int fd;
	int size;
	int read_size;

	fd = genOpen(path, FIO_O_RDONLY);
	if (fd < 0)
		return fd;

	size = genLseek(fd, 0, SEEK_END);
	if (size <= 0) {
		genClose(fd);
		return -EIO;
	}

	if (genLseek(fd, 0, SEEK_SET) < 0) {
		genClose(fd);
		return -EIO;
	}

	buffer = malloc(size + 1);
	if (buffer == NULL) {
		genClose(fd);
		return -ENOMEM;
	}

	read_size = genRead(fd, buffer, size);
	genClose(fd);
	if (read_size != size) {
		free(buffer);
		return -EIO;
	}

	buffer[size] = '\0';
	*out = buffer;
	if (out_size != NULL)
		*out_size = size;
	return 0;
}

static void parsed_keys_set(KelfParsedKeys *keys, int field, const u8 *value)
{
	memcpy(keys->value[field], value, KELF_KEY_SIZE);
	keys->present[field] = 1;
}

static void parsed_keys_overlay(KelfParsedKeys *dst, const KelfParsedKeys *src)
{
	int i;

	for (i = 0; i < KELF_KEY_FIELD_COUNT; i++) {
		if (src->present[i])
			parsed_keys_set(dst, i, src->value[i]);
	}
}

static int parse_key_store(const char *path, const char *keyset, KelfKeyStore *ks)
{
	KelfParsedKeys defaults;
	KelfParsedKeys selected;
	KelfParsedKeys merged;
	char *file;
	char *line;
	int in_default = 0;
	int in_selected = 0;
	int found_selected = 0;
	int ret;
	int i;

	memset(&defaults, 0, sizeof(defaults));
	memset(&selected, 0, sizeof(selected));
	memset(&merged, 0, sizeof(merged));
	memset(ks, 0, sizeof(*ks));

	ret = load_text_file(path, &file, NULL);
	if (ret < 0)
		return ret;

	line = file;
	while (line != NULL && *line != '\0') {
		char *next = strpbrk(line, "\r\n");
		char *s;

		if (next != NULL) {
			*next++ = '\0';
			if (*next == '\n')
				next++;
		}

		s = trim_space(line);
		if (*s == '[') {
			char *end = strchr(s, ']');

			in_default = 0;
			in_selected = 0;
			if (end != NULL) {
				*end = '\0';
				s = trim_space(s + 1);
				in_default = !kelf_stricmp(s, "default");
				in_selected = !kelf_stricmp(s, keyset);
				if (in_selected)
					found_selected = 1;
			}
		} else if (*s != '\0' && *s != ';' && *s != '#') {
			char *eq = strchr(s, '=');

			if (eq != NULL && (in_default || in_selected)) {
				const KelfKeySpec *spec;
				u8 value[KELF_KEY_SIZE];
				int decoded;

				*eq = '\0';
				s = trim_space(s);
				spec = find_key_spec(s);
				if (spec != NULL) {
					memset(value, 0, sizeof(value));
					decoded = decode_hex_key(trim_space(eq + 1), value, spec->size);
					if (decoded < 0) {
						free(file);
						return decoded;
					}
					if (decoded > 0) {
						if (in_default)
							parsed_keys_set(&defaults, spec->field, value);
						if (in_selected)
							parsed_keys_set(&selected, spec->field, value);
					}
				}
			}
		}

		line = next;
	}

	free(file);

	if (!found_selected)
		return -ENOENT;

	parsed_keys_overlay(&merged, &defaults);
	parsed_keys_overlay(&merged, &selected);

	for (i = 0; i < KELF_KEY_FIELD_REQUIRED_COUNT; i++) {
		if (!merged.present[i])
			return -EINVAL;
	}

	memcpy(ks->sig_master, merged.value[KEY_SIG_MASTER], sizeof(ks->sig_master));
	memcpy(ks->sig_hash, merged.value[KEY_SIG_HASH], sizeof(ks->sig_hash));
	memcpy(ks->kbit_master, merged.value[KEY_KBIT_MASTER], sizeof(ks->kbit_master));
	memcpy(ks->kbit_iv, merged.value[KEY_KBIT_IV], sizeof(ks->kbit_iv));
	memcpy(ks->kc_master, merged.value[KEY_KC_MASTER], sizeof(ks->kc_master));
	memcpy(ks->kc_iv, merged.value[KEY_KC_IV], sizeof(ks->kc_iv));
	memcpy(ks->rootsig_master, merged.value[KEY_ROOTSIG_MASTER], sizeof(ks->rootsig_master));
	memcpy(ks->rootsig_hash, merged.value[KEY_ROOTSIG_HASH], sizeof(ks->rootsig_hash));
	memcpy(ks->content_table_iv, merged.value[KEY_CONTENT_TABLE_IV], sizeof(ks->content_table_iv));
	memcpy(ks->content_iv, merged.value[KEY_CONTENT_IV], sizeof(ks->content_iv));

	if (merged.present[KEY_OVERRIDE_KBIT] != merged.present[KEY_OVERRIDE_KC])
		return -EINVAL;
	if (!kelf_stricmp(keyset, "arcade") && !merged.present[KEY_OVERRIDE_KBIT])
		return -EINVAL;
	if (merged.present[KEY_OVERRIDE_KBIT] && merged.present[KEY_OVERRIDE_KC]) {
		memcpy(ks->override_kbit, merged.value[KEY_OVERRIDE_KBIT], sizeof(ks->override_kbit));
		memcpy(ks->override_kc, merged.value[KEY_OVERRIDE_KC], sizeof(ks->override_kc));
		ks->has_override = 1;
	}

	return 0;
}

int KelfSignValidateKeys(const char *keys_path, const char *keyset)
{
	KelfKeyStore ks;

	if (keys_path == NULL || keyset == NULL)
		return -EINVAL;

	return parse_key_store(keys_path, keyset, &ks);
}

static int count_trailing_zeroes(const u8 *content, int size)
{
	int limit;
	int count = 0;

	limit = (size < KELF_CONTENT_TRAILING_SCAN) ? size : KELF_CONTENT_TRAILING_SCAN;
	while (count < limit && content[size - 1 - count] == 0)
		count++;

	return count;
}

static int padded_content_size(int original_size, int trailing_zeroes)
{
	if (original_size <= 0 || trailing_zeroes < 0 || trailing_zeroes > original_size)
		return -EINVAL;

	return (((original_size - trailing_zeroes) / 8) + KELF_CONTENT_EXTRA_BLOCKS) * 8;
}

int KelfSignGetOutputSize(const char *elf_path, int *out_size)
{
	u8 tail[KELF_CONTENT_TRAILING_SCAN];
	int fd;
	int size;
	int tail_size;
	int read_size;
	int trailing;
	int content_size;

	if (elf_path == NULL || out_size == NULL)
		return -EINVAL;

	fd = genOpen(elf_path, FIO_O_RDONLY);
	if (fd < 0)
		return fd;

	size = genLseek(fd, 0, SEEK_END);
	if (size <= 0) {
		genClose(fd);
		return -EIO;
	}

	tail_size = (size < (int)sizeof(tail)) ? size : (int)sizeof(tail);
	if (genLseek(fd, size - tail_size, SEEK_SET) < 0) {
		genClose(fd);
		return -EIO;
	}

	read_size = genRead(fd, tail, tail_size);
	genClose(fd);
	if (read_size != tail_size)
		return -EIO;

	trailing = count_trailing_zeroes(tail, tail_size);
	content_size = padded_content_size(size, trailing);
	if (content_size < KELF_ENCRYPTED_TAIL_SIZE)
		return -EINVAL;

	*out_size = KELF_OUTPUT_HEADER_SIZE + content_size;
	return 0;
}

static void xor8(const u8 *a, const u8 *b, u8 *out)
{
	int i;

	for (i = 0; i < 8; i++)
		out[i] = a[i] ^ b[i];
}

static void kelf_header_signature(const KelfKeyStore *ks, const KelfHeader *header, u8 out[8])
{
	u8 zero_iv[8] = {0};
	u8 encrypted[KELF_HEADER_SIZE];

	tdes_cbc_crypt(encrypted, (const u8 *)header, KELF_HEADER_SIZE, ks->sig_master, 1, zero_iv, 0);
	memcpy(out, encrypted + KELF_HEADER_SIZE - 8, 8);
	tdes_cbc_crypt(out, out, 8, ks->sig_hash, 1, zero_iv, 1);
	tdes_cbc_crypt(out, out, 8, ks->sig_master, 1, zero_iv, 0);
}

static void derive_kek(const KelfKeyStore *ks, const KelfHeader *header, u8 kek[16])
{
	u8 zero_iv[8] = {0};
	u8 header_data[8];

	xor8(((const u8 *)header), ((const u8 *)header) + 8, header_data);
	xor8(ks->kbit_iv, header_data, kek);
	xor8(ks->kc_iv, header_data, kek + 8);

	tdes_cbc_crypt(kek, kek, 8, ks->kbit_master, 2, zero_iv, 0);
	tdes_cbc_crypt(kek + 8, kek + 8, 8, ks->kc_master, 2, zero_iv, 0);
}

static void encrypt_kelf_keys(u8 kbit[16], u8 kc[16], const u8 kek[16])
{
	u8 zero_iv[8] = {0};

	tdes_cbc_crypt(kbit, kbit, 8, kek, 2, zero_iv, 0);
	tdes_cbc_crypt(kbit + 8, kbit + 8, 8, kek, 2, zero_iv, 0);
	tdes_cbc_crypt(kc, kc, 8, kek, 2, zero_iv, 0);
	tdes_cbc_crypt(kc + 8, kc + 8, 8, kek, 2, zero_iv, 0);
}

static void sign_content_block(const KelfKeyStore *ks, const u8 *content, const KelfBitBlock *block, u8 signature[8])
{
	u8 zero_iv[8] = {0};
	u8 sig_key[16];
	u32 pos;
	int i;

	memset(signature, 0, KELF_SIGNATURE_SIZE);
	for (pos = 0; pos < block->size; pos += 8) {
		for (i = 0; i < 8; i++)
			signature[i] ^= content[pos + i];
	}

	memcpy(sig_key, ks->sig_master, 8);
	memcpy(sig_key + 8, ks->sig_hash, 8);
	tdes_cbc_crypt(signature, signature, 8, sig_key, 2, zero_iv, 0);
}

static void kelf_bit_table_signature(const KelfKeyStore *ks, const KelfBitTable *bit_table, const u8 kbit[16], const u8 kc[16], u8 out[8])
{
	u8 zero_iv[8] = {0};
	u8 sig_key[16];
	u8 hash[8];
	int blocks_8;
	int i;
	int j;

	memcpy(hash, kbit, 8);
	if (memcmp(kbit, kbit + 8, 8) != 0) {
		for (i = 0; i < 8; i++)
			hash[i] ^= kbit[8 + i];
	}
	for (i = 0; i < 8; i++)
		hash[i] ^= kc[i];
	if (memcmp(kc, kc + 8, 8) != 0) {
		for (i = 0; i < 8; i++)
			hash[i] ^= kc[8 + i];
	}

	blocks_8 = bit_table->block_count * 2 + 1;
	for (i = 0; i < blocks_8; i++) {
		const u8 *src = ((const u8 *)bit_table) + i * 8;

		for (j = 0; j < 8; j++)
			hash[j] ^= src[j];
	}

	memcpy(sig_key, ks->sig_master, 8);
	memcpy(sig_key + 8, ks->sig_hash, 8);
	tdes_cbc_crypt(out, hash, 8, sig_key, 2, zero_iv, 0);
}

static void kelf_root_signature(const KelfKeyStore *ks, const KelfBitTable *bit_table, const u8 header_sig[8], const u8 bit_table_sig[8], u8 out[8])
{
	u8 zero_iv[8] = {0};
	u8 signatures[8 + 8 + KELF_BLOCK_COUNT * 8];
	u8 encrypted[sizeof(signatures)];
	int size = 0;
	int i;

	memcpy(signatures + size, header_sig, 8);
	size += 8;
	memcpy(signatures + size, bit_table_sig, 8);
	size += 8;
	for (i = 0; i < bit_table->block_count; i++) {
		if (bit_table->blocks[i].flags & KELF_BIT_BLOCK_SIGNED) {
			memcpy(signatures + size, bit_table->blocks[i].signature, 8);
			size += 8;
		}
	}

	tdes_cbc_crypt(encrypted, signatures, size, ks->rootsig_master, 1, zero_iv, 0);
	tdes_cbc_crypt(out, encrypted + size - 8, 8, ks->rootsig_hash, 2, zero_iv, 1);
}

static int load_elf_into_output(const char *elf_path, u8 **out_buffer, int *out_size, int *content_size)
{
	u8 *buffer;
	u8 *content;
	int fd;
	int original_size;
	int max_content_size;
	int read_size;
	int trailing;
	int final_content_size;

	fd = genOpen(elf_path, FIO_O_RDONLY);
	if (fd < 0)
		return fd;

	original_size = genLseek(fd, 0, SEEK_END);
	if (original_size <= 0) {
		genClose(fd);
		return -EIO;
	}

	max_content_size = ((original_size / 8) + KELF_CONTENT_EXTRA_BLOCKS) * 8;
	buffer = memalign(64, KELF_OUTPUT_HEADER_SIZE + max_content_size);
	if (buffer == NULL) {
		genClose(fd);
		return -ENOMEM;
	}

	memset(buffer, 0, KELF_OUTPUT_HEADER_SIZE + max_content_size);
	content = buffer + KELF_OUTPUT_HEADER_SIZE;

	if (genLseek(fd, 0, SEEK_SET) < 0) {
		free(buffer);
		genClose(fd);
		return -EIO;
	}

	read_size = genRead(fd, content, original_size);
	genClose(fd);
	if (read_size != original_size) {
		free(buffer);
		return -EIO;
	}

	trailing = count_trailing_zeroes(content, original_size);
	final_content_size = padded_content_size(original_size, trailing);
	if (final_content_size < KELF_ENCRYPTED_TAIL_SIZE || final_content_size > max_content_size) {
		free(buffer);
		return -EINVAL;
	}

	*out_buffer = buffer;
	*out_size = KELF_OUTPUT_HEADER_SIZE + final_content_size;
	*content_size = final_content_size;
	return 0;
}

int KelfSignElfToMemory(const char *elf_path,
                        const char *keys_path,
                        const char *keyset,
                        int header_id,
                        int system_type,
                        void **out_buf,
                        int *out_size)
{
	KelfKeyStore ks;
	KelfHeader header;
	KelfBitTable bit_table;
	u8 *buffer;
	u8 *content;
	u8 *cursor;
	u8 kbit[16];
	u8 kc[16];
	u8 kek[16];
	u8 header_sig[8];
	u8 bit_table_sig[8];
	u8 root_sig[8];
	int content_size;
	int total_size;
	int ret;

	if (elf_path == NULL || keys_path == NULL || keyset == NULL || out_buf == NULL || out_size == NULL)
		return -EINVAL;
	if (header_id != KELF_SIGN_HEADER_FMCB &&
	    header_id != KELF_SIGN_HEADER_DNASLOAD &&
	    header_id != KELF_SIGN_HEADER_DONGLE)
		return -EINVAL;
	if (system_type != KELF_SIGN_SYSTEM_PS2 && system_type != KELF_SIGN_SYSTEM_PSX)
		return -EINVAL;

	ret = parse_key_store(keys_path, keyset, &ks);
	if (ret < 0)
		return ret;

	ret = load_elf_into_output(elf_path, &buffer, &total_size, &content_size);
	if (ret < 0)
		return ret;

	content = buffer + KELF_OUTPUT_HEADER_SIZE;
	memset(&header, 0, sizeof(header));
	if (header_id == KELF_SIGN_HEADER_DNASLOAD)
		memcpy(header.user_defined, user_header_dnasload, sizeof(header.user_defined));
	else if (header_id == KELF_SIGN_HEADER_DONGLE)
		memcpy(header.user_defined, user_header_dongle, sizeof(header.user_defined));
	else
		memcpy(header.user_defined, user_header_fmcb, sizeof(header.user_defined));
	header.content_size = content_size;
	header.header_size = KELF_OUTPUT_HEADER_SIZE;
	header.system_type = (u8)system_type;
	header.application_type = KELF_APPLICATION_XOSDMAIN;
	header.flags = KELF_FLAGS_KELF;
	header.bit_count = 0;
	header.mg_zones = KELF_MG_ZONES_ALL;

	memset(&bit_table, 0, sizeof(bit_table));
	bit_table.header_size = KELF_OUTPUT_HEADER_SIZE;
	bit_table.block_count = KELF_BLOCK_COUNT;
	bit_table.blocks[0].size = content_size - KELF_ENCRYPTED_TAIL_SIZE;
	bit_table.blocks[0].flags = 0;
	bit_table.blocks[1].size = KELF_ENCRYPTED_TAIL_SIZE;
	bit_table.blocks[1].flags = KELF_BIT_BLOCK_SIGNED | KELF_BIT_BLOCK_ENCRYPTED;

	memcpy(kbit, user_kbit_fmcb, sizeof(kbit));
	memset(kc, 0, sizeof(kc));
	if (ks.has_override) {
		memcpy(kbit, ks.override_kbit, sizeof(kbit));
		memcpy(kc, ks.override_kc, sizeof(kc));
	}

	sign_content_block(&ks, content + bit_table.blocks[0].size, &bit_table.blocks[1], bit_table.blocks[1].signature);
	tdes_cbc_crypt(content + bit_table.blocks[0].size,
	               content + bit_table.blocks[0].size,
	               bit_table.blocks[1].size,
	               kc,
	               2,
	               ks.content_iv,
	               0);

	kelf_header_signature(&ks, &header, header_sig);
	kelf_bit_table_signature(&ks, &bit_table, kbit, kc, bit_table_sig);
	kelf_root_signature(&ks, &bit_table, header_sig, bit_table_sig, root_sig);

	tdes_cbc_crypt((u8 *)&bit_table,
	               (const u8 *)&bit_table,
	               KELF_BIT_TABLE_SIZE,
	               kbit,
	               2,
	               ks.content_table_iv,
	               0);

	derive_kek(&ks, &header, kek);
	encrypt_kelf_keys(kbit, kc, kek);

	cursor = buffer;
	memcpy(cursor, &header, sizeof(header));
	cursor += sizeof(header);
	memcpy(cursor, header_sig, sizeof(header_sig));
	cursor += sizeof(header_sig);
	memcpy(cursor, kbit, sizeof(kbit));
	cursor += sizeof(kbit);
	memcpy(cursor, kc, sizeof(kc));
	cursor += sizeof(kc);
	memcpy(cursor, &bit_table, KELF_BIT_TABLE_SIZE);
	cursor += KELF_BIT_TABLE_SIZE;
	memcpy(cursor, bit_table_sig, sizeof(bit_table_sig));
	cursor += sizeof(bit_table_sig);
	memcpy(cursor, root_sig, sizeof(root_sig));

	*out_buf = buffer;
	*out_size = total_size;
	return 0;
}

#ifdef KELF_SIGNER_SELFTEST
int main(int argc, char **argv)
{
	void *buffer;
	int size;
	int header;
	int system;
	int ret;
	int fd;

	if (argc == 1) {
		u64 subkeys[16];
		u8 key[8] = {0x13, 0x34, 0x57, 0x79, 0x9b, 0xbc, 0xdf, 0xf1};
		u8 plain[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
		u8 cipher[8];
		u8 expected[8] = {0x85, 0xe8, 0x13, 0x54, 0x0f, 0x0a, 0xb4, 0x05};

		des_make_subkeys(key, subkeys);
		des_crypt_block(plain, cipher, subkeys, 0);
		return memcmp(cipher, expected, sizeof(cipher)) == 0 ? 0 : 2;
	}

	if (argc != 7) {
		fprintf(stderr, "usage: %s <keys.dat> <keyset> <fmcb|dnasload|dongle> <ps2|psx> <input.elf> <output.kelf>\n", argv[0]);
		return 1;
	}

	if (!strcmp(argv[3], "dnasload"))
		header = KELF_SIGN_HEADER_DNASLOAD;
	else if (!strcmp(argv[3], "dongle"))
		header = KELF_SIGN_HEADER_DONGLE;
	else
		header = KELF_SIGN_HEADER_FMCB;
	system = !strcmp(argv[4], "psx") ? KELF_SIGN_SYSTEM_PSX : KELF_SIGN_SYSTEM_PS2;
	ret = KelfSignElfToMemory(argv[5], argv[1], argv[2], header, system, &buffer, &size);
	if (ret < 0) {
		fprintf(stderr, "sign failed: %d\n", ret);
		return 1;
	}

	fd = open(argv[6], O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) {
		perror(argv[6]);
		free(buffer);
		return 1;
	}
	if (write(fd, buffer, size) != size) {
		perror("write");
		close(fd);
		free(buffer);
		return 1;
	}
	close(fd);
	free(buffer);
	return 0;
}
#endif
