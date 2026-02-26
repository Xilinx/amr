// SPDX-License-Identifier: GPL-2.0-only
/*
 * md5.c - MD5 message-digest algorithm (RFC 1321)
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
 */

#include <string.h>
#include "md5.h"


#define F(x, y, z)	(((x) & (y)) | (~(x) & (z)))
#define G(x, y, z)	(((x) & (z)) | ((y) & ~(z)))
#define H(x, y, z)	((x) ^ (y) ^ (z))
#define I(x, y, z)	((y) ^ ((x) | ~(z)))

#define ROTL(x, n)	(((x) << (n)) | ((x) >> (32 - (n))))

#define MD5_STEP(f, a, b, c, d, x, t, s)      \
do {                                          \
	(a) += f((b), (c), (d)) + (x) + (t);  \
	(a)  = ROTL((a), (s));                \
	(a) += (b);                           \
} while (0)

static const uint8_t md5_padding[MD5_BLOCK_SIZE] = { 0x80 };

/**
 * md5_transform() - Transform the state based on the block of data.
 * @state: Current hash state.
 * @block: Block of data to transform.
 */
static void md5_transform(uint32_t state[4], const uint8_t block[MD5_BLOCK_SIZE])
{
	uint32_t a = state[0];
	uint32_t b = state[1];
	uint32_t c = state[2];
	uint32_t d = state[3];
	uint32_t m[16];
	unsigned int i;

	for (i = 0; i < 16; i++)
		m[i] = (uint32_t)block[i * 4]
			| ((uint32_t)block[i * 4 + 1] << 8)
			| ((uint32_t)block[i * 4 + 2] << 16)
			| ((uint32_t)block[i * 4 + 3] << 24);

	/* Round 1 */
	MD5_STEP(F, a, b, c, d, m[ 0], 0xd76aa478,  7);
	MD5_STEP(F, d, a, b, c, m[ 1], 0xe8c7b756, 12);
	MD5_STEP(F, c, d, a, b, m[ 2], 0x242070db, 17);
	MD5_STEP(F, b, c, d, a, m[ 3], 0xc1bdceee, 22);
	MD5_STEP(F, a, b, c, d, m[ 4], 0xf57c0faf,  7);
	MD5_STEP(F, d, a, b, c, m[ 5], 0x4787c62a, 12);
	MD5_STEP(F, c, d, a, b, m[ 6], 0xa8304613, 17);
	MD5_STEP(F, b, c, d, a, m[ 7], 0xfd469501, 22);
	MD5_STEP(F, a, b, c, d, m[ 8], 0x698098d8,  7);
	MD5_STEP(F, d, a, b, c, m[ 9], 0x8b44f7af, 12);
	MD5_STEP(F, c, d, a, b, m[10], 0xffff5bb1, 17);
	MD5_STEP(F, b, c, d, a, m[11], 0x895cd7be, 22);
	MD5_STEP(F, a, b, c, d, m[12], 0x6b901122,  7);
	MD5_STEP(F, d, a, b, c, m[13], 0xfd987193, 12);
	MD5_STEP(F, c, d, a, b, m[14], 0xa679438e, 17);
	MD5_STEP(F, b, c, d, a, m[15], 0x49b40821, 22);

	/* Round 2 */
	MD5_STEP(G, a, b, c, d, m[ 1], 0xf61e2562,  5);
	MD5_STEP(G, d, a, b, c, m[ 6], 0xc040b340,  9);
	MD5_STEP(G, c, d, a, b, m[11], 0x265e5a51, 14);
	MD5_STEP(G, b, c, d, a, m[ 0], 0xe9b6c7aa, 20);
	MD5_STEP(G, a, b, c, d, m[ 5], 0xd62f105d,  5);
	MD5_STEP(G, d, a, b, c, m[10], 0x02441453,  9);
	MD5_STEP(G, c, d, a, b, m[15], 0xd8a1e681, 14);
	MD5_STEP(G, b, c, d, a, m[ 4], 0xe7d3fbc8, 20);
	MD5_STEP(G, a, b, c, d, m[ 9], 0x21e1cde6,  5);
	MD5_STEP(G, d, a, b, c, m[14], 0xc33707d6,  9);
	MD5_STEP(G, c, d, a, b, m[ 3], 0xf4d50d87, 14);
	MD5_STEP(G, b, c, d, a, m[ 8], 0x455a14ed, 20);
	MD5_STEP(G, a, b, c, d, m[13], 0xa9e3e905,  5);
	MD5_STEP(G, d, a, b, c, m[ 2], 0xfcefa3f8,  9);
	MD5_STEP(G, c, d, a, b, m[ 7], 0x676f02d9, 14);
	MD5_STEP(G, b, c, d, a, m[12], 0x8d2a4c8a, 20);

	/* Round 3 */
	MD5_STEP(H, a, b, c, d, m[ 5], 0xfffa3942,  4);
	MD5_STEP(H, d, a, b, c, m[ 8], 0x8771f681, 11);
	MD5_STEP(H, c, d, a, b, m[11], 0x6d9d6122, 16);
	MD5_STEP(H, b, c, d, a, m[14], 0xfde5380c, 23);
	MD5_STEP(H, a, b, c, d, m[ 1], 0xa4beea44,  4);
	MD5_STEP(H, d, a, b, c, m[ 4], 0x4bdecfa9, 11);
	MD5_STEP(H, c, d, a, b, m[ 7], 0xf6bb4b60, 16);
	MD5_STEP(H, b, c, d, a, m[10], 0xbebfbc70, 23);
	MD5_STEP(H, a, b, c, d, m[13], 0x289b7ec6,  4);
	MD5_STEP(H, d, a, b, c, m[ 0], 0xeaa127fa, 11);
	MD5_STEP(H, c, d, a, b, m[ 3], 0xd4ef3085, 16);
	MD5_STEP(H, b, c, d, a, m[ 6], 0x04881d05, 23);
	MD5_STEP(H, a, b, c, d, m[ 9], 0xd9d4d039,  4);
	MD5_STEP(H, d, a, b, c, m[12], 0xe6db99e5, 11);
	MD5_STEP(H, c, d, a, b, m[15], 0x1fa27cf8, 16);
	MD5_STEP(H, b, c, d, a, m[ 2], 0xc4ac5665, 23);

	/* Round 4 */
	MD5_STEP(I, a, b, c, d, m[ 0], 0xf4292244,  6);
	MD5_STEP(I, d, a, b, c, m[ 7], 0x432aff97, 10);
	MD5_STEP(I, c, d, a, b, m[14], 0xab9423a7, 15);
	MD5_STEP(I, b, c, d, a, m[ 5], 0xfc93a039, 21);
	MD5_STEP(I, a, b, c, d, m[12], 0x655b59c3,  6);
	MD5_STEP(I, d, a, b, c, m[ 3], 0x8f0ccc92, 10);
	MD5_STEP(I, c, d, a, b, m[10], 0xffeff47d, 15);
	MD5_STEP(I, b, c, d, a, m[ 1], 0x85845dd1, 21);
	MD5_STEP(I, a, b, c, d, m[ 8], 0x6fa87e4f,  6);
	MD5_STEP(I, d, a, b, c, m[15], 0xfe2ce6e0, 10);
	MD5_STEP(I, c, d, a, b, m[ 6], 0xa3014314, 15);
	MD5_STEP(I, b, c, d, a, m[13], 0x4e0811a1, 21);
	MD5_STEP(I, a, b, c, d, m[ 4], 0xf7537e82,  6);
	MD5_STEP(I, d, a, b, c, m[11], 0xbd3af235, 10);
	MD5_STEP(I, c, d, a, b, m[ 2], 0x2ad7d2bb, 15);
	MD5_STEP(I, b, c, d, a, m[ 9], 0xeb86d391, 21);

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

/**
 * md5_init() - Initialize an MD5 context.
 * @ctx: MD5 context to initialize.
 */
void md5_init(struct md5_ctx *ctx)
{
	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xefcdab89;
	ctx->state[2] = 0x98badcfe;
	ctx->state[3] = 0x10325476;
	ctx->count = 0;
}

/**
 * md5_update() - Feed data into the MD5 computation.
 * @ctx: MD5 context.
 * @data: Input data buffer.
 * @len: Length of input data in bytes.
 */
void md5_update(struct md5_ctx *ctx, const uint8_t *data, uint32_t len)
{
	uint32_t index = (uint32_t)(ctx->count & 0x3F);
	uint32_t i = 0;

	ctx->count += len;

	if (index) {
		uint32_t part_len = MD5_BLOCK_SIZE - index;

		if (len >= part_len) {
			memcpy(&ctx->buffer[index], data, part_len);
			md5_transform(ctx->state, ctx->buffer);
			i = part_len;
		} else {
			memcpy(&ctx->buffer[index], data, len);
			return;
		}
	}

	for (; i + MD5_BLOCK_SIZE <= len; i += MD5_BLOCK_SIZE)
		md5_transform(ctx->state, &data[i]);

	if (i < len)
		memcpy(ctx->buffer, &data[i], len - i);
}

/**
 * md5_final() - Finalize the MD5 computation and output the digest.
 * @ctx: MD5 context.
 * @digest: Output buffer for 16-byte MD5 hash.
 */
void md5_final(struct md5_ctx *ctx, uint8_t digest[MD5_DIGEST_SIZE])
{
	uint32_t index = (uint32_t)(ctx->count & 0x3F);
	uint64_t bits = ctx->count * 8;
	uint32_t pad_len = (index < 56) ? (56 - index) : (120 - index);
	unsigned int i;

	md5_update(ctx, md5_padding, pad_len);

	/* Append length in bits as 64-bit little-endian */
	for (i = 0; i < 8; i++)
		ctx->buffer[56 + i] = (uint8_t)(bits >> (i * 8));

	md5_transform(ctx->state, ctx->buffer);

	for (i = 0; i < 4; i++) {
		digest[i * 4]     = (uint8_t)(ctx->state[i]);
		digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 8);
		digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 16);
		digest[i * 4 + 3] = (uint8_t)(ctx->state[i] >> 24);
	}
}

/**
 * calculate_md5() - Calculate MD5 checksum of a data buffer.
 * @data: Pointer to data buffer.
 * @len: Length of data in bytes.
 * @md5_out: Output buffer for 16-byte MD5 hash.
 */
void calculate_md5(const uint8_t *data, uint32_t len, uint8_t *md5_out)
{
	struct md5_ctx ctx;

	if (!data || !md5_out)
		return;

	md5_init(&ctx);
	md5_update(&ctx, data, len);
	md5_final(&ctx, md5_out);
}
