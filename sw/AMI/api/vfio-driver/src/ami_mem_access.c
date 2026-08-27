// SPDX-License-Identifier: MIT
/*
 * ami_mem_access.c - This file contains the implementation of memory read/write logic.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
 */

/* Standard includes */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Public API includes */
#include "ami_mem_access.h"

/* Private API includes */
#include "ami_internal.h"
#include "ami_device_internal.h"

/*****************************************************************************/
/* Local function declarations                                               */
/*****************************************************************************/

/**
 * do_bar_transaction() - Do a PCI BAR read or write via userspace GCQ BAR mapping.
 * @dev: Device handle.
 * @idx: Bar number.
 * @offset: Offset within BAR.
 * @num: Number of BAR registers to read/write.
 * @val: Buffer to write to the BAR or to store values read from the BAR.
 * @is_write: true for write, false for read.
 *
 * Return: AMI_STATUS_OK or AMI_STATUS_ERROR
 */
static int do_bar_transaction(ami_device *dev, uint8_t idx, uint64_t offset,
	uint32_t num, uint32_t *val, bool is_write);

/*****************************************************************************/
/* Local function definitions                                                */
/*****************************************************************************/

/*
 * Do a PCI BAR read or write via userspace GCQ BAR mapping
 */
static int do_bar_transaction(ami_device *dev, uint8_t idx, uint64_t offset,
	uint32_t num, uint32_t *val, bool is_write)
{
	void *bar0;
	uint32_t i;
	volatile uint32_t *ptr;
	extern void *gcq_userspace_get_bar0(gcq_metal_ctx *ctx);

	if (!dev || !val || (num == 0))
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* Only BAR0 is supported via userspace GCQ mapping */
	if (idx != 0)
		return AMI_API_ERROR_M(AMI_ERROR_EINVAL, "Only BAR0 supported via userspace GCQ");

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR;  /* ami_open_cdev sets the last error */

	/* Get BAR0 base address from GCQ context */
	bar0 = gcq_userspace_get_bar0(dev->gcq_ctx);
	if (!bar0)
		return AMI_API_ERROR(AMI_ERROR_EIO);

	/* Calculate address */
	ptr = (volatile uint32_t *)((char *)bar0 + offset);

	/* Perform read or write with memory barriers */
	__sync_synchronize();
	if (is_write) {
		for (i = 0; i < num; i++) {
			ptr[i] = val[i];
		}
	} else {
		for (i = 0; i < num; i++) {
			val[i] = ptr[i];
		}
	}
	__sync_synchronize();

	return AMI_STATUS_OK;
}

/*****************************************************************************/
/* Public API function definitions                                           */
/*****************************************************************************/

/*
 * Read from a PCI bar.
 */
int ami_mem_bar_read(ami_device *dev, uint8_t idx, uint64_t offset, uint32_t *val)
{
	if (!dev || !val)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	return do_bar_transaction(dev, idx, offset, 1, val, false);
}

/*
 * Write to a PCI bar.
 */
int ami_mem_bar_write(ami_device *dev, uint8_t idx, uint64_t offset, uint32_t val)
{
	if (!dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	return do_bar_transaction(dev, idx, offset, 1, &val, true);
}

/*
 * Read block from a PCI bar.
 */
int ami_mem_bar_read_range(ami_device *dev, uint8_t idx, uint64_t offset,
	uint32_t num, uint32_t *val)
{
	if (!dev || !val || (num == 0))
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	return do_bar_transaction(dev, idx, offset, num, val, false);
}

/*
 * Write block to a PCI bar.
 */
int ami_mem_bar_write_range(ami_device *dev, uint8_t idx, uint64_t offset,
	uint32_t num, uint32_t *val)
{
	if (!dev || !val || (num == 0))
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	return do_bar_transaction(dev, idx, offset, num, val, true);
}
