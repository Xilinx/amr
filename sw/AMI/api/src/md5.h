// SPDX-License-Identifier: GPL-2.0-only
/*
 * md5.h - MD5 message-digest algorithm (RFC 1321)
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef MD5_H
#define MD5_H

#include <stdint.h>

#define MD5_DIGEST_SIZE  16
#define MD5_BLOCK_SIZE   64

/**
 * struct md5_ctx - MD5 computation context.
 * @state: Current hash state.
 * @count: Total number of bytes processed.
 * @buffer: Partial block buffer.
 */
struct md5_ctx {
	uint32_t state[4];
	uint64_t count;
	uint8_t  buffer[MD5_BLOCK_SIZE];
};

/**
 * md5_init() - Initialize an MD5 context.
 * @ctx: MD5 context to initialize.
 */
void md5_init(struct md5_ctx *ctx);

/**
 * md5_update() - Feed data into the MD5 computation.
 * @ctx: MD5 context.
 * @data: Input data buffer.
 * @len: Length of input data in bytes.
 */
void md5_update(struct md5_ctx *ctx, const uint8_t *data, uint32_t len);

/**
 * md5_final() - Finalize the MD5 computation and output the digest.
 * @ctx: MD5 context.
 * @digest: Output buffer for 16-byte MD5 hash.
 */
void md5_final(struct md5_ctx *ctx, uint8_t digest[MD5_DIGEST_SIZE]);

/**
 * calculate_md5() - Calculate MD5 checksum of a data buffer.
 * @data: Pointer to data buffer.
 * @len: Length of data in bytes.
 * @md5_out: Output buffer for 16-byte MD5 hash.
 */
void calculate_md5(const uint8_t *data, uint32_t len, uint8_t *md5_out);

#endif /* MD5_H */
