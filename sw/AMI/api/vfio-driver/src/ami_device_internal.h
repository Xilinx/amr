// SPDX-License-Identifier: MIT
/*
 * ami_device_internal.h - This file contains the private interface for device related logic
 *
 * Copyright (c) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef AMI_DEVICE_INTERNAL_H
#define AMI_DEVICE_INTERNAL_H

/* Standard includes */
#include <stdint.h>

/* Public API includes */
#include "ami_device.h"

/* Private API includes */
#include "ami_internal.h"
#include "ami_sensor_internal.h"
#include "gcq_userspace.h"

/*****************************************************************************/
/* Defines                                                                   */
/*****************************************************************************/

#define AMI_DEV_SYSFS_DIR	"/sys/bus/pci/drivers/ami/0000:%02x:%02x.%1x"
#define AMI_DEV_SYSFS_NODE	AMI_DEV_SYSFS_DIR "/%s"
#define AMI_SYSFS_PATH_MAX	(256)
#define AMI_SYSFS_STR_MAX	(256)

/* PCIe BAR size */
#define XILINX_ENDPOINT_BAR_SGCQ_LEN	0x800000  /* 8MB */

/* Upper bound on cached SDR repositories (temp, voltage, current, power,
 * total_power, fpt). Sized with headroom so repo indices used by sensor
 * reads always fit. */
#define AMI_SDR_MAX_REPOS	(8)

/*****************************************************************************/
/* Structs                                                                   */
/*****************************************************************************/

/**
 * struct ami_device - represents a single PCI device
 * @bdf: device BDF
 * @cap_override: bypass permission checks when communicating with the driver
 * @cdev_num: character device number
 * @cdev: character device file handle (may be invalid)
 * @hwmon_num: hwmon device number
 * @num_sensors: number of suported sensors (eg. vccint, 12v_pex, etc...)
 * @num_total_sensors: total number of sensors  (e.g. vccint temp, vccint power, etc...)
 * @sensors: list of supported sensors (head)
 *
 * If `cap_override` is set to true, all IOCTL's (and any other relevant API)
 * issued using this device handle will bypass any permission checks
 * and execute code that would, normally, only be reachable with root/sudo!
 */
struct ami_device {
	uint16_t            bdf;
	bool                cap_override;
	int                 cdev_num;
	int                 cdev;
	int                 hwmon_num;
	int                 num_sensors;
	int                 num_total_sensors;
	struct ami_sensor  *sensors;
	struct ami_sensor  *last_sensor;       /* Per-device "last found sensor"
	                                         * cache used by find_sensor_by_name()
	                                         * to skip the linear search on
	                                         * repeated lookups of the same
	                                         * sensor. Must live here (per
	                                         * device), not as a process-wide
	                                         * static: a shared cache would let
	                                         * concurrent lookups on two
	                                         * different devices for a sensor
	                                         * with the same name (e.g.
	                                         * "temp1") race and hand back the
	                                         * wrong device's sensor pointer. */
	char                cdev_name[AMI_DEV_NAME_MAX];

	/* Userspace GCQ support */
	bool                use_userspace_gcq;  /* True if using userspace GCQ path */
	gcq_metal_ctx      *gcq_ctx;            /* GCQ userspace context */
	int                 vfio_group_num;     /* VFIO group number */

	/* Cached device info for userspace GCQ (read once at open) */
	bool                info_cached;        /* True if cached data is valid */
	char                cached_uuid[33];    /* UUID string */
	char                cached_state[33];   /* State string */
	char                cached_name[33];    /* Device name */

	/* Cached AMC version from IDENTIFY (avoid repeated slow IDENTIFY calls).
	 * IDENTIFY is expensive (~1s on some AMC firmware), so the result is
	 * cached with a short freshness window (AMC_VERSION_CACHE_TTL_MS): reads
	 * within the window reuse it, later reads re-probe so a crashed/updated
	 * AMC is still detected. */
	bool                amc_version_cached; /* True if IDENTIFY response is cached */
	uint8_t             amc_ver_major;      /* AMC firmware major version */
	uint8_t             amc_ver_minor;      /* AMC firmware minor version */
	uint8_t             amc_ver_patch;      /* AMC firmware patch version */
	uint64_t            amc_ver_cache_ms;   /* Monotonic ms when version cached */
	bool                amc_probe_failed;   /* True if last IDENTIFY probe failed */
	uint64_t            amc_probe_fail_ms;  /* Monotonic ms of that failure — used
	                                         * to skip re-probing an unresponsive
	                                         * AMC within the freshness window
	                                         * (avoids repeated 5s timeouts). */

	/* SDR (Sensor Data Records) cache for userspace GCQ path */
	struct sdr_repo    *sdr_repos;          /* Array of SDR repositories */
	int                 num_sdr_repos;      /* Number of repos cached */
	uint64_t            sdr_repo_fetch_ms[AMI_SDR_MAX_REPOS];
	                                        /* Monotonic ms of last GCQ fetch
	                                         * per repo index; bounds how often
	                                         * a whole repo is refetched so a
	                                         * single `sensors` dump makes one
	                                         * GET_SDR per repo, not one per
	                                         * sensor value. */

	/* Cached BDINFO SDR repo (static board data - serial, part no, mac,
	 * uuid, etc). Fetched once over GCQ then reused so mfg-info reads
	 * don't re-hit the AMC on every field. Freed in ami_dev_delete(). */
	struct sdr_repo    *bdinfo_cache;
	bool                bdinfo_failed;  /* True once a BDINFO fetch failed;
	                                     * avoids re-timing-out on a device
	                                     * whose AMC answers IDENTIFY but not
	                                     * SDR reads (e.g. broken FPT). */

	/* When true, gcq_userspace_open() will not block waiting for AMC ready.
	 * Set by ami_dev_find_next() so that device enumeration never stalls. */
	bool                quick_open;
};

/*****************************************************************************/
/* Private API function declarations                                         */
/*****************************************************************************/

/*
 * ami_open_cdev() - Open a character device file if not already opened.
 * @dev: AMI device handle to which the cdev file belongs.
 *
 * Return: AMI_STATUS_OK if opened, AMI_STATUS_ERROR otherwise
 */
int ami_open_cdev(ami_device *dev);

/*
 * ami_close_cdev() - Close a character device file if not already closed.
 * @dev: AMI device handle to which the cdev file belongs.
 *
 * Return: AMI_STATUS_OK if closed, AMI_STATUS_ERROR otherwise
 */
int ami_close_cdev(ami_device *dev);

/*
 * ami_read_sysfs() - Read a sysfs attribute.
 * @dev: Device handle.
 * @attr: Attribute name.
 * @buf: Buffer to store attribute value. Must be at least AMI_SYSFS_STR_MAX bytes.
 *
 * Return: AMI_STATUS_ERROR or AMI_STATUS_OK
 */
int ami_read_sysfs(ami_device *dev, const char *attr, char *buf);

/**
 * ami_dev_register() - Register the current process with the driver.
 * @dev: Device handle.
 *
 * Return: AMI_STATUS_ERROR or AMI_STATUS_OK
 */
int ami_dev_register(ami_device *dev);

/**
 * ami_dev_deregister() - Deregister the current process from the driver.
 * @dev: Device handle.
 *
 * Return: AMI_STATUS_ERROR or AMI_STATUS_OK
 */
int ami_dev_deregister(ami_device *dev);

/**
 * ami_dev_open_bdf() - Open a VFIO device by numeric BDF.
 * @dev: Pointer to store new device handle (must be NULL on entry).
 * @bdf: Numeric BDF.
 * @wait_for_amc: If true, block up to AMC_READY_MAX_WAIT_SEC for VMR0 magic.
 *
 * Return: AMI_STATUS_OK or AMI_STATUS_ERROR.
 */
int ami_dev_open_bdf(ami_device **dev, uint16_t bdf, bool wait_for_amc);

#endif  /* AMI_DEVICE_INTERNAL_H */
