// SPDX-License-Identifier: GPL-2.0-only
/*
 * ami_eeprom_access.c - VFIO userspace driver EEPROM access interface.
 *
 * Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
 */

/* Standard includes */
#include <stdint.h>

/* Public API includes */
#include "ami_eeprom_access.h"

/* Private API includes */
#include "ami_internal.h"
#include "ami_device_internal.h"
#include "ami_gcq_common.h"

/*****************************************************************************/
/* Public API function definitions                                           */
/*****************************************************************************/

/*
 * ami_eeprom_read() - Read one or more bytes of data from the EEPROM via GCQ.
 */
int ami_eeprom_read(ami_device *dev, uint8_t offset, uint8_t num, uint8_t *val)
{
	int ret;

	if (!dev || !val)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR; /* last error is set by ami_open_cdev */

	ret = ami_gcq_eeprom_read(dev->gcq_ctx, offset, num, val);
	return (ret == 0) ? AMI_STATUS_OK : AMI_STATUS_ERROR;
}

/*
 * ami_eeprom_write() - Write one or more bytes of data to the EEPROM via GCQ.
 */
int ami_eeprom_write(ami_device *dev, uint8_t offset, uint8_t num, uint8_t *val)
{
	int ret;

	if (!dev || !val)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR; /* last error is set by ami_open_cdev */

	ret = ami_gcq_eeprom_write(dev->gcq_ctx, offset, num, val);
	return (ret == 0) ? AMI_STATUS_OK : AMI_STATUS_ERROR;
}
