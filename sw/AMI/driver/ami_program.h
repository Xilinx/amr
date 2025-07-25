// SPDX-License-Identifier: GPL-2.0-only
/*
 * ami_program.h - This file contains functions to program (flash) devices.
 *
 * Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef AMI_PROGRAM_H
#define AMI_PROGRAM_H

#include <linux/types.h>
#include <linux/eventfd.h>

#include "ami_top.h"
#include "ami_amc_control.h"

#define MAX_PARTITION		(15)
#define MAX_DEVICE		(1)

/*
 * Format of flags:
 * 0xAABBCCDD where:
 *   0xAA is the source device (8 bits)
 *   0xBB is the source partition (8 bits)
 *   0xCC is the destination device (8 bits)
 *   0xDD is the destination partition (8 bits)
 */
#define MK_PARTITION_FLAGS(src_device, src_part, dest_device, dest_part) \
				(((uint8_t)src_device << 24) | \
				((uint8_t)src_part    << 16) | \
				((uint8_t)dest_device << 8)  | \
				((uint8_t)dest_part))


#define DEVICE_SRC(flags)		((uint8_t)(flags >> 24))
#define PARTITION_SRC(flags)	((uint8_t)(flags >> 16))
#define DEVICE_DEST(flags)		((uint8_t)(flags >> 8))
#define PARTITION_DEST(flags)	((uint8_t)(flags))

#define FPT_UPDATE_FLAG			(0xAA)  /* uint8 - the other bytes are the boot device, and chunk num */
#define FPT_UPDATE_MAGIC		(0xAAAAAAAA)
#define PDI_PROGRAM_FLAG		(0xBB)  /* uint8 - the other bytes are the boot device, and chunk num */
#define PDI_PROGRAM_MAGIC		(0xBBBBBBBB)
#define PDI_CHUNK_MULTIPLIER	(1024)
#define PDI_CHUNK_SIZE			(6144)	/* Multiple of 1024 */

/*
 * Format of flags:
 * 0xAABBCCCC where:
 *   0xAA is the boot device flag (8 bits)
 *   0xBB is the partition number (8 bits) - this is 0xAA when updating the FPT
 *   0xCCCC is the current chunk number (15 bits) with the MSB set to 1 if this is the last chunk (1 bit)
 *
 * `last` in this macro should be a bool.
 */
#define MK_PDI_FLAGS(boot, part, chunk, last) \
					(((uint8_t)boot << 24) | ((uint8_t)part << 16 ) | ((last) ? \
					((uint16_t)chunk | ((uint16_t)1  << 15)) : \
					((uint16_t)chunk & ~((uint16_t)1 << 15))))
#define PDI_BOOT_DEVICE(flags)		((uint8_t)(flags >> 24))
#define PDI_PARTITION(flags)		((uint8_t)(flags >> 16))
#define PDI_CHUNK(flags)			(((uint16_t)(flags & 0x0000ffff)) & ~((uint16_t)1 << 15))
#define PDI_CHUNK_IS_LAST(flags)	((uint16_t)(flags & 0x0000ffff) >> 15)  /* either 1 or 0 */

/**
 * download_pdi() - Download a PDI bitstream onto a device.
 * @amc_ctrl_ctxt: Pointer to top level AMC data struct.
 * @buf: Bitstream byte buffer.
 * @size: Size of bitstream buffer.
 * @boot_device: Target boot device.
 * @partition: Partition number to flash.
 * @efd_ctx: eventfd context for reporting progress (optional).
 *
 * Return: 0 or negative error code.
 */
int download_pdi(struct amc_control_ctxt *amc_ctrl_ctxt, uint8_t *buf,
	uint32_t size, uint8_t boot_device, uint32_t partition,
	struct eventfd_ctx *efd_ctx);

/**
 * update_fpt() - Download a PDI containing an FPT onto a device.
 * @pf_dev: Device data.
 * @buf: Bitstream byte buffer - must contain valid FPT.
 * @size: Size of bitstream buffer.
 * @boot_device: Target boot device.
 * @efd_ctx: eventfd context for reporting progress (optional).
 *
 * Return: 0 or negative error code.
 */
int update_fpt(struct pf_dev_struct *pf_dev, uint8_t *buf, uint32_t size,
	uint8_t boot_device, struct eventfd_ctx *efd_ctx);

/**
 * device_boot() - Set the device boot partition.
 * @pf_dev: Device data.
 * @partition: Partition number to select.
 *
 * Return: 0 or negative error code.
 */
int device_boot(struct pf_dev_struct *pf_dev, uint32_t partition);

/**
 * copy_partition() - Copy a device partition.
 * @pf_dev: Device data.
 * @src_device: Device to copy from.
 * @src_part: Partition to copy from.
 * @dest_device: Device to copy to.
 * @dest_part: Partition to copy to.
 *
 * Return: 0 or negative error code.
 */
int copy_partition(struct pf_dev_struct *pf_dev, uint32_t src_device,
	uint32_t src_part, uint32_t dest_device, uint32_t dest_part);

#endif  /* AMI_PROGRAM_H */
