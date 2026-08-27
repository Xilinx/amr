// SPDX-License-Identifier: MIT
/*
 * ami_mfg_info.c - This file contains the public interface for board info logic.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
 */

/* Standard includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Public API includes */
#include "ami_mfg_info.h"
#include "ami_sdr.h"

/* Private API includes */
#include "ami_internal.h"
#include "ami_device_internal.h"
#include "ami_gcq_common.h"

/*****************************************************************************/
/* Private functions                                                         */
/*****************************************************************************/

/**
 * get_mfg_field_from_gcq() - Get manufacturing field via GCQ (for VFIO driver)
 * @dev: Device handle.
 * @field: Field to retrieve.
 * @buf: Output buffer.
 *
 * Return: AMI_STATUS_OK or AMI_STATUS_ERROR.
 */
static int get_mfg_field_from_gcq(ami_device *dev, enum ami_mfg_field field,
	char buf[AMI_MFG_INFO_MAX_STR]);

/**
 * format_field_value() - Format a field_info structure into a string
 * @field: Field info structure.
 * @buf: Output buffer.
 * @field_type: Field type for special formatting.
 *
 * Return: AMI_STATUS_OK or AMI_STATUS_ERROR.
 */
static int format_field_value(struct field_info *field, char *buf, enum ami_mfg_field field_type);

/*
 * Format a field_info structure into a string.
 */
static int format_field_value(struct field_info *field, char *buf, enum ami_mfg_field field_type)
{
	int i;

	if (!field || !buf || field->len == 0)
		return AMI_STATUS_ERROR;

	memset(buf, 0, AMI_MFG_INFO_MAX_STR);

	/* Handle special formatting for certain fields */
	switch (field_type) {
	case AMI_MFG_MAC_ADDR:
	case AMI_MFG_MAC_ADDR_N:
		/* Format as MAC address: XX:XX:XX:XX:XX:XX */
		if (field->len >= 6) {
			snprintf(buf, AMI_MFG_INFO_MAX_STR, "%02x:%02x:%02x:%02x:%02x:%02x",
				field->bytes[0], field->bytes[1], field->bytes[2],
				field->bytes[3], field->bytes[4], field->bytes[5]);
		}
		break;

	case AMI_MFG_UUID:
		/* Format as GUID: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx */
		if (field->len >= 16) {
			snprintf(buf, AMI_MFG_INFO_MAX_STR,
				"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
				field->bytes[0], field->bytes[1], field->bytes[2], field->bytes[3],
				field->bytes[4], field->bytes[5],
				field->bytes[6], field->bytes[7],
				field->bytes[8], field->bytes[9],
				field->bytes[10], field->bytes[11], field->bytes[12], field->bytes[13],
				field->bytes[14], field->bytes[15]);
		}
		break;

	case AMI_MFG_M_DATE:
		/* Format manufacturing date from binary timestamp */
		if (field->len >= 4) {
			uint32_t timestamp = *((uint32_t *)field->bytes);
			time_t t = (time_t)timestamp;
			char *timestr = ctime(&t);
			if (timestr) {
				/* Remove newline from ctime output */
				strncpy(buf, timestr, AMI_MFG_INFO_MAX_STR - 1);
				buf[strcspn(buf, "\n")] = '\0';
			}
		}
		break;

	case AMI_MFG_CONFIG_MODE:
		/* Convert config mode byte to string */
		if (field->bytes[0] == 7)
			strncpy(buf, "Master SPI x4 (QSPIx4)", AMI_MFG_INFO_MAX_STR - 1);
		else if (field->bytes[0] == 8)
			strncpy(buf, "OSPI", AMI_MFG_INFO_MAX_STR - 1);
		else
			snprintf(buf, AMI_MFG_INFO_MAX_STR, "0x%02x", field->bytes[0]);
		break;

	case AMI_MFG_POWER_MODE:
		/* Convert power mode to wattage */
		switch (field->bytes[0]) {
		case 0: strncpy(buf, "75W", AMI_MFG_INFO_MAX_STR - 1); break;
		case 1: strncpy(buf, "150W", AMI_MFG_INFO_MAX_STR - 1); break;
		case 2: strncpy(buf, "225W", AMI_MFG_INFO_MAX_STR - 1); break;
		case 3: strncpy(buf, "350W", AMI_MFG_INFO_MAX_STR - 1); break;
		default: snprintf(buf, AMI_MFG_INFO_MAX_STR, "%d", field->bytes[0]); break;
		}
		break;

	case AMI_MFG_PCIE_ID:
		/* Format as vendor:device:subvendor:subdevice */
		if (field->len >= 8) {
			snprintf(buf, AMI_MFG_INFO_MAX_STR, "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
				field->bytes[1], field->bytes[0],
				field->bytes[3], field->bytes[2],
				field->bytes[5], field->bytes[4],
				field->bytes[7], field->bytes[6]);
		}
		break;

	default:
		/* ASCII or binary - copy as string if ASCII type
		 * Type 1 = ASCII in SDR spec, Type 3 = ASCII in ASDM */
		if (field->type == SDR_VALUE_TYPE_ASCII || field->type == 1 || field->type == 3) {
			strncpy(buf, (char *)field->bytes,
				(field->len < AMI_MFG_INFO_MAX_STR) ? field->len : (AMI_MFG_INFO_MAX_STR - 1));
		} else {
			/* Binary - format as hex */
			for (i = 0; i < field->len && (i * 3) < (AMI_MFG_INFO_MAX_STR - 3); i++) {
				snprintf(buf + (i * 3), 4, "%02x ", field->bytes[i]);
			}
		}
		break;
	}

	return AMI_STATUS_OK;
}

/*
 * Get manufacturing field via GCQ (for VFIO driver).
 */
/**
 * get_cached_bdinfo() - Return the device's BDINFO board record, fetching it
 *   over GCQ once and caching it for the lifetime of the device handle.
 * @dev: Device handle (userspace GCQ path).
 *
 * Board manufacturing data (serial, part number, MAC, UUID, ...) is static,
 * so a single fetch is reused across every mfg-info field read. This avoids a
 * full BDINFO SDR round-trip to the AMC on every ami_mfg_get_info() call.
 *
 * Return: pointer to the cached bd_info record, or NULL on failure.
 */
static struct bd_info_record *get_cached_bdinfo(ami_device *dev)
{
	struct sdr_repo *repo = NULL;

	if (dev->bdinfo_cache && dev->bdinfo_cache->bd_info)
		return dev->bdinfo_cache->bd_info;

	/* Don't re-attempt a fetch that already failed this handle — on a card
	 * whose AMC answers IDENTIFY but not SDR reads, each attempt costs a
	 * full GCQ timeout. */
	if (dev->bdinfo_failed)
		return NULL;

	repo = (struct sdr_repo *)calloc(1, sizeof(*repo));
	if (!repo)
		return NULL;

	if (ami_gcq_get_sdr(dev->gcq_ctx, SDR_TYPE_BDINFO, repo) != 0 ||
	    !repo->bd_info) {
		ami_free_sdr_repo(repo);
		free(repo);
		dev->bdinfo_failed = true;
		return NULL;
	}

	dev->bdinfo_cache = repo;
	return repo->bd_info;
}

static int get_mfg_field_from_gcq(ami_device *dev, enum ami_mfg_field field,
	char buf[AMI_MFG_INFO_MAX_STR])
{
	struct bd_info_record *bd_info = NULL;
	struct field_info *field_data = NULL;
	struct field_info last_mac_computed = {0};
	int ret;

	if (!dev || !buf)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (!dev->gcq_ctx)
		return AMI_API_ERROR(AMI_ERROR_ENODEV);

	bd_info = get_cached_bdinfo(dev);
	if (!bd_info)
		return AMI_API_ERROR(AMI_ERROR_EIO);

	/* Map field enum to structure member */
	switch (field) {
	case AMI_MFG_EEPROM_VERSION:
		field_data = &bd_info->eeprom_version;
		break;
	case AMI_MFG_PRODUCT_NAME:
		field_data = &bd_info->product_name;
		break;
	case AMI_MFG_BOARD_REV:
		field_data = &bd_info->board_rev;
		break;
	case AMI_MFG_BOARD_SERIAL:
		field_data = &bd_info->board_serial;
		break;
	case AMI_MFG_MAC_ADDR_C:
		field_data = &bd_info->mac_addr_count;
		break;
	case AMI_MFG_MAC_ADDR:
		field_data = &bd_info->first_mac_addr;
		break;
	case AMI_MFG_MAC_ADDR_N:
		/* Last MAC = first MAC + count - 1 */
		if (bd_info->first_mac_addr.len >= 6 &&
		    bd_info->mac_addr_count.len > 0) {
			uint8_t count = bd_info->mac_addr_count.bytes[0];

			/* Copy first MAC address */
			memcpy(last_mac_computed.bytes,
			       bd_info->first_mac_addr.bytes, 6);
			last_mac_computed.type = bd_info->first_mac_addr.type;
			last_mac_computed.len = 6;

			/* Add (count - 1) to the last byte of MAC address */
			if (count > 0) {
				uint16_t last_byte = last_mac_computed.bytes[5] + (count - 1);

				/* Handle overflow into previous bytes */
				last_mac_computed.bytes[5] = last_byte & 0xFF;
				if (last_byte > 0xFF) {
					last_mac_computed.bytes[4] += (last_byte >> 8);
				}
			}

			field_data = &last_mac_computed;
		} else {
			field_data = &bd_info->first_mac_addr;
		}
		break;
	case AMI_MFG_ACTIVE_STATE:
		field_data = &bd_info->active_state;
		break;
	case AMI_MFG_CONFIG_MODE:
		field_data = &bd_info->config_mode;
		break;
	case AMI_MFG_M_DATE:
		field_data = &bd_info->manufacturing_date;
		break;
	case AMI_MFG_PART_NUM:
		field_data = &bd_info->part_number;
		break;
	case AMI_MFG_UUID:
		field_data = &bd_info->uuid;
		break;
	case AMI_MFG_PCIE_ID:
		field_data = &bd_info->pcie_id;
		break;
	case AMI_MFG_POWER_MODE:
		field_data = &bd_info->power_mode;
		break;
	case AMI_MFG_MEMORY_SIZE:
		field_data = &bd_info->memory_size;
		break;
	case AMI_MFG_OEM_ID:
		field_data = &bd_info->oem_id;
		break;
	case AMI_MFG_CAPABILITY:
		field_data = &bd_info->capability;
		break;
	case AMI_MFG_M_PART_NUM:
		field_data = &bd_info->mfg_part_number;
		break;
	default:
		return AMI_API_ERROR(AMI_ERROR_EINVAL);
	}

	/* Format and copy field value */
	if (field_data && field_data->len > 0) {
		ret = format_field_value(field_data, buf, field);
	} else {
		/* Field not available - return empty to suppress "Unknown" */
		memset(buf, 0, AMI_MFG_INFO_MAX_STR);
		ret = AMI_STATUS_OK;  /* Return OK with empty string to hide field */
	}

	return ret;
}

/**
 * get_name_for_mfg_field() - Get the name of a SYSFS EEPROM node.
 * @field: EEPROM field type to lookup.
 *
 * Return: The SYSFS name or empty string.
 */
static char *get_name_for_mfg_field(enum ami_mfg_field field)
{
	switch (field) {
	case AMI_MFG_EEPROM_VERSION: return "eeprom_version";
	case AMI_MFG_PRODUCT_NAME:   return "product_name";
	case AMI_MFG_BOARD_REV:      return "board_rev";
	case AMI_MFG_BOARD_SERIAL:   return "board_serial";
	case AMI_MFG_MEMORY_SIZE:    return "memory_size";
	case AMI_MFG_PART_NUM:       return "part_num";
	case AMI_MFG_M_PART_NUM:     return "mfg_part_num";
	case AMI_MFG_MAC_ADDR_C:     return "mac_addr_count";
	case AMI_MFG_MAC_ADDR:       return "mac_addr";
	case AMI_MFG_MAC_ADDR_N:     return "mac_addr_n";
	case AMI_MFG_ACTIVE_STATE:   return "active_state";
	case AMI_MFG_CONFIG_MODE:    return "fpga_config_mode";
	case AMI_MFG_M_DATE:         return "mfg_date";
	case AMI_MFG_UUID:           return "mfg_uuid";
	case AMI_MFG_PCIE_ID:        return "pcie_id";
	case AMI_MFG_POWER_MODE:     return "power_mode";
	case AMI_MFG_OEM_ID:         return "oem_id";
	case AMI_MFG_CAPABILITY:     return "mfg_capability";
	default:                     break;
	}

	return "";
}

/*****************************************************************************/
/* Public API function definitions                                           */
/*****************************************************************************/

/*
 * Get the manufacturing info.
 */
int ami_mfg_get_info(ami_device *dev, enum ami_mfg_field field,
	char buf[AMI_MFG_INFO_MAX_STR])
{
	int ret = AMI_STATUS_ERROR;
	char *field_name = NULL;
	char raw_buf[AMI_SYSFS_STR_MAX] = { 0 };

	if (!dev || !buf)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* Use GCQ path for VFIO driver */
	if (dev->use_userspace_gcq) {
		/*
		 * overview only needs BDF/UUID/state; do not force GCQ open for mfg
		 * fields (avoids stderr spam when AMC is in ERROR after a bad program).
		 */
		if (dev->cdev == AMI_INVALID_FD)
			return AMI_STATUS_ERROR;

		return get_mfg_field_from_gcq(dev, field, buf);
	}

	/* Legacy sysfs path for kernel driver */
	field_name = get_name_for_mfg_field(field);
	if (!field_name || !strlen(field_name))
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_read_sysfs(dev, field_name, raw_buf) == AMI_STATUS_OK) {
		/* Strip newline */
		raw_buf[strcspn(raw_buf, "\r\n")] = 0;
		memset(buf, 0x00, AMI_MFG_INFO_MAX_STR);
		strncpy(buf, raw_buf, AMI_MFG_INFO_MAX_STR - 1);
		ret = AMI_STATUS_OK;
	}

	return ret;
}
