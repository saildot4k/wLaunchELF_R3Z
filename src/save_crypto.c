/* PSV/VMP signing follows Apollo Save Tool's psv_resign.c by dots_tb and
 * Bucanero. AES and SHA implementations below use the published algorithms. */
#include "save_formats.h"
#include <string.h>

typedef struct
{
	uint32_t h[8];
	uint64_t bytes;
	uint8_t block[64];
	int sha256;
} hash_t;

static uint32_t rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
static uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static uint32_t be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void hashBlock(hash_t *s, const uint8_t *p)
{
	static const uint32_t k[64] = {
	    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
	uint32_t w[80], a, b, c, d, e, f, g, h, t, u;
	int i;
	for (i = 0; i < 16; i++)
		w[i] = be32(p + i * 4);
	a = s->h[0];
	b = s->h[1];
	c = s->h[2];
	d = s->h[3];
	e = s->h[4];
	if (!s->sha256) {
		for (i = 16; i < 80; i++)
			w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
		for (i = 0; i < 80; i++) {
			if (i < 20) {
				f = (b & c) | (~b & d);
				t = 0x5a827999;
			} else if (i < 40) {
				f = b ^ c ^ d;
				t = 0x6ed9eba1;
			} else if (i < 60) {
				f = (b & c) | (b & d) | (c & d);
				t = 0x8f1bbcdc;
			} else {
				f = b ^ c ^ d;
				t = 0xca62c1d6;
			}
			t += rol(a, 5) + f + e + w[i];
			e = d;
			d = c;
			c = rol(b, 30);
			b = a;
			a = t;
		}
	} else {
		for (i = 16; i < 64; i++) {
			t = w[i - 15];
			u = w[i - 2];
			w[i] = w[i - 16] + (ror(t, 7) ^ ror(t, 18) ^ (t >> 3)) + w[i - 7] + (ror(u, 17) ^ ror(u, 19) ^ (u >> 10));
		}
		f = s->h[5];
		g = s->h[6];
		h = s->h[7];
		for (i = 0; i < 64; i++) {
			t = h + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
			u = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
			h = g;
			g = f;
			f = e;
			e = d + t;
			d = c;
			c = b;
			b = a;
			a = t + u;
		}
		s->h[5] += f;
		s->h[6] += g;
		s->h[7] += h;
	}
	s->h[0] += a;
	s->h[1] += b;
	s->h[2] += c;
	s->h[3] += d;
	s->h[4] += e;
}

static void hashInit(hash_t *s, int sha256)
{
	static const uint32_t initial[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	memset(s, 0, sizeof(*s));
	s->sha256 = sha256;
	if (sha256)
		memcpy(s->h, initial, sizeof(initial));
	else {
		s->h[0] = 0x67452301;
		s->h[1] = 0xefcdab89;
		s->h[2] = 0x98badcfe;
		s->h[3] = 0x10325476;
		s->h[4] = 0xc3d2e1f0;
	}
}

static void hashUpdate(hash_t *s, const uint8_t *p, size_t size)
{
	while (size) {
		size_t pos = s->bytes & 63, n = 64 - pos;
		if (n > size)
			n = size;
		memcpy(s->block + pos, p, n);
		s->bytes += n;
		p += n;
		size -= n;
		if (!(s->bytes & 63))
			hashBlock(s, s->block);
	}
}

static void hashFinish(hash_t *s, uint8_t *out)
{
	uint64_t bits = s->bytes * 8;
	uint8_t pad[128] = {0x80};
	size_t n = 64 - (s->bytes & 63);
	int i;
	if (n < 9)
		n += 64;
	for (i = 0; i < 8; i++)
		pad[n - 1 - i] = (uint8_t)(bits >> (8 * i));
	hashUpdate(s, pad, n);
	for (i = 0; i < (s->sha256 ? 32 : 20); i++)
		out[i] = (uint8_t)(s->h[i / 4] >> (24 - 8 * (i % 4)));
}

void saveSha256(const uint8_t *data, size_t size, uint8_t out[32])
{
	hash_t s;
	hashInit(&s, 1);
	hashUpdate(&s, data, size);
	hashFinish(&s, out);
}

static uint8_t sbox[256], invbox[256];
static int aes_ready;
static uint8_t mul(uint8_t a, uint8_t b)
{
	uint8_t v = 0;
	while (b) {
		if (b & 1)
			v ^= a;
		a = (a << 1) ^ ((a & 128) ? 0x1b : 0);
		b >>= 1;
	}
	return v;
}

static void aesKey(const uint8_t key[16], uint8_t round[176])
{
	int i, j;
	uint8_t r = 1, t[4];
	if (!aes_ready) {
		for (i = 0; i < 256; i++) {
			uint8_t x = 1, b = (uint8_t)i, y;
			int e = 254;
			while (e) {
				if (e & 1)
					x = mul(x, b);
				b = mul(b, b);
				e >>= 1;
			}
			y = x ^ ((x << 1) | (x >> 7)) ^ ((x << 2) | (x >> 6)) ^ ((x << 3) | (x >> 5)) ^ ((x << 4) | (x >> 4)) ^ 0x63;
			sbox[i] = y;
			invbox[y] = (uint8_t)i;
		}
		aes_ready = 1;
	}
	memcpy(round, key, 16);
	for (i = 16; i < 176; i += 4) {
		memcpy(t, round + i - 4, 4);
		if (!(i % 16)) {
			uint8_t v = t[0];
			t[0] = sbox[t[1]] ^ r;
			t[1] = sbox[t[2]];
			t[2] = sbox[t[3]];
			t[3] = sbox[v];
			r = mul(r, 2);
		}
		for (j = 0; j < 4; j++)
			round[i + j] = round[i + j - 16] ^ t[j];
	}
}

static void aesBlock(uint8_t *p, const uint8_t *key, int encrypt)
{
	uint8_t t[16];
	int i, j, r;
	for (i = 0; i < 16; i++)
		p[i] ^= key[(encrypt ? 0 : 160) + i];
	for (r = 1; r <= 10; r++) {
		if (encrypt) {
			for (i = 0; i < 16; i++)
				t[i] = sbox[p[i]];
			for (i = 0; i < 16; i++)
				p[i] = t[(i % 4) + 4 * ((i / 4 + i % 4) % 4)];
		} else {
			memcpy(t, p, 16);
			for (i = 0; i < 16; i++)
				p[i] = invbox[t[(i % 4) + 4 * ((i / 4 + 4 - i % 4) % 4)]];
			for (i = 0; i < 16; i++)
				p[i] ^= key[(10 - r) * 16 + i];
		}
		if (r != 10) {
			memcpy(t, p, 16);
			for (j = 0; j < 16; j += 4)
				for (i = 0; i < 4; i++) {
					if (encrypt)
						p[j + i] = mul(t[j + i], 2) ^ mul(t[j + (i + 1) % 4], 3) ^ t[j + (i + 2) % 4] ^ t[j + (i + 3) % 4];
					else
						p[j + i] = mul(t[j + i], 14) ^ mul(t[j + (i + 1) % 4], 11) ^ mul(t[j + (i + 2) % 4], 13) ^ mul(t[j + (i + 3) % 4], 9);
				}
		}
		if (encrypt)
			for (i = 0; i < 16; i++)
				p[i] ^= key[r * 16 + i];
	}
}

static void cbc(uint8_t *data, size_t size, const uint8_t *key, const uint8_t *iv, int encrypt)
{
	uint8_t round[176], prev[16], next[16];
	size_t off;
	int i;
	aesKey(key, round);
	memcpy(prev, iv, 16);
	for (off = 0; off + 16 <= size; off += 16) {
		memcpy(next, data + off, 16);
		if (encrypt)
			for (i = 0; i < 16; i++)
				data[off + i] ^= prev[i];
		aesBlock(data + off, round, encrypt);
		if (!encrypt)
			for (i = 0; i < 16; i++)
				data[off + i] ^= prev[i];
		memcpy(prev, encrypt ? data + off : next, 16);
	}
}

void saveMcxCrypt(uint8_t *data, size_t size, int encrypt)
{
	static const uint8_t key[16] = {0x81, 0xd9, 0xcc, 0xe9, 0x71, 0xa9, 0x49, 0x9b, 0x04, 0xad, 0xdc, 0x48, 0x30, 0x7f, 0x07, 0x92};
	static const uint8_t iv[16] = {0x13, 0xc2, 0xe7, 0x69, 0x4b, 0xec, 0x69, 0x6d, 0x52, 0xcf, 0x00, 0x09, 0x2a, 0xc1, 0xf2, 0x72};
	cbc(data, size, key, iv, encrypt);
}

void saveSign(uint8_t *data, size_t size, int vmp)
{
	static const uint8_t ps1key[16] = {0xab, 0x5a, 0xbc, 0x9f, 0xc1, 0xf4, 0x9d, 0xe6, 0xa0, 0x51, 0xdb, 0xae, 0xfa, 0x51, 0x88, 0x59};
	static const uint8_t ps2key[16] = {0xea, 0x02, 0xce, 0xef, 0x5b, 0xb4, 0xd2, 0x99, 0x8f, 0x61, 0x19, 0x10, 0xd7, 0x7f, 0x51, 0xc6};
	static const uint8_t iv[16] = {0xb3, 0x0f, 0xfe, 0xed, 0xb7, 0xdc, 0x5e, 0xb7, 0x13, 0x3d, 0xa6, 0x0d, 0x1b, 0x6b, 0x2c, 0xdc};
	uint8_t salt[64] = {0}, round[176], digest[20];
	size_t seed = vmp ? 12 : 8, sig = vmp ? 32 : 28;
	int i;
	hash_t h;
	if (size < 64)
		return;
	if (vmp || data[60] == 1) {
		aesKey(ps1key, round);
		memcpy(salt, data + seed, 16);
		memcpy(salt + 16, salt, 16);
		aesBlock(salt, round, 0);
		aesBlock(salt + 16, round, 1);
		for (i = 0; i < 16; i++)
			salt[i] ^= iv[i];
		for (i = 0; i < 4; i++)
			salt[16 + i] ^= data[seed + 16 + i];
	} else {
		memcpy(salt, data + seed, 20);
		cbc(salt, 64, ps2key, iv, 0);
	}
	memset(salt + 20, 0, 44);
	memset(data + sig, 0, 20);
	for (i = 0; i < 64; i++)
		salt[i] ^= 0x36;
	hashInit(&h, 0);
	hashUpdate(&h, salt, 64);
	hashUpdate(&h, data, size);
	hashFinish(&h, digest);
	for (i = 0; i < 64; i++)
		salt[i] ^= 0x6a;
	hashInit(&h, 0);
	hashUpdate(&h, salt, 64);
	hashUpdate(&h, digest, 20);
	hashFinish(&h, data + sig);
}
