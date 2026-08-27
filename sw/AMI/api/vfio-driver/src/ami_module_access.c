// SPDX-License-Identifier: MIT
/*
 * ami_module_access.c - This file contains the QSFP module access interface.
 *
 * Copyright (c) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.
 */

/* Standard includes */
#include <stdint.h>
#include <stdio.h>

/* Public API includes */
#include "ami_module_access.h"

/* Private API includes */
#include "ami_internal.h"
#include "ami_device_internal.h"
#include "ami_gcq_common.h"

/*****************************************************************************/
/* Public API function definitions                                           */
/*****************************************************************************/

/*
 * Read one or more bytes of data from to a QSFP module.
 */
int ami_module_read(ami_device *dev, uint8_t device_id, uint8_t page,
	uint8_t offset, uint8_t num, uint8_t *val)
{
	int ret;

	if (!dev || !val || (num == 0))
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR;

	ret = ami_gcq_module_read(dev->gcq_ctx, device_id, page, offset, num, val);
	return (ret == 0) ? AMI_STATUS_OK : AMI_STATUS_ERROR;
}

/*
 * Write one or more bytes of data to a QSFP module
 */
int ami_module_write(ami_device *dev, uint8_t device_id, uint8_t page,
	uint8_t offset, uint8_t num, uint8_t *val)
{
	int ret;

	if (!dev || !val || (num == 0))
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR;

	ret = ami_gcq_module_write(dev->gcq_ctx, device_id, page, offset, num, val);
	return (ret == 0) ? AMI_STATUS_OK : AMI_STATUS_ERROR;
}
