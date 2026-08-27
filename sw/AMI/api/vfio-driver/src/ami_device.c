// SPDX-License-Identifier: MIT
/*
 * ami_device.c - This file contains the implementation of device related logic
 *
 * Copyright (c) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.
 */

/* Standard includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>

/* Private API includes */
#include "ami_internal.h"
#include "ami_device_internal.h"
#include "ami_sensor_internal.h"
#include "ami_api_version.h"
#include "ami_mem_access.h"
#include "ami_amc_shared_mem.h"
#include "ami_gcq_common.h"
#include "ami_mfg_info.h"
#include "ami_sdr.h"

/* For VFIO mmap support */
#include <sys/mman.h>

/*****************************************************************************/
/* Defines                                                                   */
/*****************************************************************************/

/* Device sysfs attributes */
#define SYSFS_LOGIC_UUID		"logic_uuid"
#define SYSFS_PCI_VENDOR		"vendor"
#define SYSFS_PCI_DEVICE		"device"
#define SYSFS_PCI_NUMA_NODE		"numa_node"
#define SYSFS_PCI_CPULIST		"local_cpulist"
#define SYSFS_DEV_STATE			"dev_state"
#define SYSFS_DEV_NAME			"dev_name"
#define SYSFS_AMC_VERSION		"amc_version"

#define AMC_VERSION_ATTR_FMT	"%hhd.%hhd.%hhd +%hd *%hhd"
#define AMC_VERSION_ATTR_FIELDS	(5)
#define AMC_VERSION_ATTR_SIZE	(16)

/*
 * The AMI driver creates link speed/width attributes because
 * the pre-existing attributes created by the PCIe core return invalid
 * values on some systems.
 */
#define SYSFS_PCI_LINK_SPEED_C	"link_speed_current"
#define SYSFS_PCI_LINK_SPEED_M	"link_speed_max"
#define SYSFS_PCI_LINK_WIDTH_C	"link_width_current"
#define SYSFS_PCI_LINK_WIDTH_M	"link_width_max"

/* For PCI reloading */

/*
 * NOTE: The following delays may need tweaking.
 * It is possible that a device may not show up after a rescan
 * if improper delays are used!
 */
#define HOT_RESET_SBR_SET_DELAY_MS	(2)
#define HOT_RESET_RESCAN_DELAY_MS	(5000)

#define PCI_DEV_DIR			"/sys/bus/pci/devices/0000:%02x:%02x.%1x"
#define PCI_BRIDGE_CONTROL		(0x3e)
#define PCI_BRIDGE_CTL_BUS_RESET	(0x40)

#define SYSFS_ENABLE			"1"
#define SYSFS_PCI_CONFIG		"/sys/bus/pci/devices/%s/config"
#define SYSFS_PCI_REMOVE		PCI_DEV_DIR "/remove"
#define SYSFS_PCI_RESCAN		"/sys/bus/pci/rescan"
#define SYSFS_PCI_DEVICES_DIR		"/sys/bus/pci/devices"

/*****************************************************************************/
/* Local function declarations                                               */
/*****************************************************************************/

static uint64_t ami_monotonic_ms(void);
static bool vfio_amc_magic_valid(uint32_t magic);
static void ami_format_logic_uuid(const uint8_t uuid[16], char out[33]);
static void vfio_try_cache_info_from_bar(ami_device *dev);

/**
 * open_sysfs() - Open a sysfs node for either reading or writing (or both).
 * @dev: Device handle.
 * @attr: Attribute name.
 * @mode: File mode (O_RDONLY, O_WRONLY, O_RDWR).
 *
 * Return: The file descriptor (-1 on error).
 */
static int open_sysfs(ami_device *dev, const char *attr, int mode);

/**
 * get_new_device_handle() - Get a fresh handle for a specific device.
 * @new_dev: Variable to store new handle.
 * @bdf: BDF of old handle.
 * @with_sensors: Did the old handle have sensor data?
 *
 * This function is essentially a wrapper for `ami_dev_find_next` which performs
 * some additional setup to ensure that the new device handle is in the same
 * state as the old handle (i.e., if the old handle had sensor data, an attempt
 * will be made to fetch sensor data for the new handle also).
 *
 * Return: AMI_STATUS_OK or AMI_STATUS_ERROR
 */
static int get_new_device_handle(ami_device **new_dev, uint16_t bdf, bool with_sensors);

/**
 * pci_remove() - Remove a device from the PCI tree.
 * @bdf: Numeric BDF of the device to remove.
 *
 * Note that the device to be removed need not be a device attached to the AMI
 * driver - it can be any generic PCI device.
 *
 * Sudo/root permissions are required.
 *
 * If the provided device is attached to the AMI driver, it must be deleted
 * before calling this function, otherwise a SIGBUS will be sent to the calling
 * application. If this is not handled, the process will be terminated.
 *
 * Return: AMI_STATUS_OK or AMI_STATUS_ERROR.
 */
static int pci_remove(uint16_t bdf);

/**
 * pci_remove_slot() - Remove all PCI functions for a given bus/device.
 * @bdf: Numeric BDF; function bits are ignored.
 *
 * This helper removes all present functions (.0 - .7) on the same slot.
 * Missing functions are ignored.
 *
 * Return: AMI_STATUS_OK or AMI_STATUS_ERROR.
 */
static int pci_remove_slot(uint16_t bdf);

/**
 * pci_rescan() - Rescan the PCI tree.
 *
 * Sudo/root permissions are required. This function will not return
 * until any new devices have been fully initialized/probed.
 *
 * Return: AMI_STATUS_OK or AMI_STATUS_ERROR.
 */
static int pci_rescan(void);

/*****************************************************************************/
/* Local function definitions                                                */
/*****************************************************************************/

static bool vfio_amc_magic_valid(uint32_t magic)
{
	return (magic == AMC_MAGIC_NUMBER);
}

/*
 * ami_format_logic_uuid() - Format 16-byte AMC UUID for display.
 *
 * AMC shared memory stores the Versal parentUniqueId as a little-endian
 * uint32 in the first four bytes (rest zero).  logic_uuid sysfs and PDI
 * parent UUID use a 32-digit hex string with the ID right-aligned, e.g.
 * 00000000000000000000000059b68233 — not 3382b659000000000000000000000000.
 */
static void ami_format_logic_uuid(const uint8_t uuid[16], char out[33])
{
	int i;
	bool tail_zero = true;

	for (i = 4; i < 16; i++) {
		if (uuid[i] != 0) {
			tail_zero = false;
			break;
		}
	}

	if (tail_zero && (uuid[0] || uuid[1] || uuid[2] || uuid[3])) {
		uint32_t id = (uint32_t)uuid[0] |
			      ((uint32_t)uuid[1] << 8) |
			      ((uint32_t)uuid[2] << 16) |
			      ((uint32_t)uuid[3] << 24);

		snprintf(out, 33, "%032x", id);
	} else {
		for (i = 0; i < 16; i++)
			snprintf(out + (i * 2), 3, "%02x", uuid[i]);
	}
	out[32] = '\0';
}

/*
 * If AMC shared memory is valid, refresh UUID/state/name cache without opening GCQ.
 */
static void vfio_try_cache_info_from_bar(ami_device *dev)
{
	int fd;
	void *bar0;
	volatile struct amc_shared_mem *shm;
	volatile uint32_t *status;
	uint32_t magic;

	if (!dev)
		return;

	fd = open("/dev/vfio/vfio", O_RDONLY);
	if (fd < 0)
		return;

	bar0 = mmap(NULL, XILINX_ENDPOINT_BAR_SGCQ_LEN, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if (bar0 == MAP_FAILED)
		return;

	__sync_synchronize();
	shm = (volatile struct amc_shared_mem *)((uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET);
	magic = shm->amc_magic_no;
	__sync_synchronize();

	if (!vfio_amc_magic_valid(magic)) {
		munmap(bar0, XILINX_ENDPOINT_BAR_SGCQ_LEN);
		return;
	}

	if (shm->uuid.amc_uuid_len == 16) {
		const uint8_t *uuid = (const uint8_t *)((uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET +
							shm->uuid.amc_uuid_off);

		ami_format_logic_uuid(uuid, dev->cached_uuid);
	}

	status = (volatile uint32_t *)((uint8_t *)bar0 +
		AMC_SHARED_MEM_OFFSET + shm->status.amc_status_off);

	if (*status == AMC_STATUS_READY)
		strncpy(dev->cached_state, "READY", sizeof(dev->cached_state) - 1);
	else
		snprintf(dev->cached_state, sizeof(dev->cached_state),
			 "0x%x", *status);

	snprintf(dev->cached_name, sizeof(dev->cached_name), "ALVEO V80");
	dev->info_cached = true;
	munmap(bar0, XILINX_ENDPOINT_BAR_SGCQ_LEN);
}

/*
 * Open a sysfs node.
 */
static int open_sysfs(ami_device *dev, const char *attr, int mode)
{
	int file = AMI_INVALID_FD;
	char path[AMI_SYSFS_PATH_MAX] = { 0 };

	if (!dev || !attr)
		return file;

	snprintf(
		path,
		AMI_SYSFS_PATH_MAX,
		AMI_DEV_SYSFS_NODE,
		AMI_PCI_BUS(dev->bdf),
		AMI_PCI_DEV(dev->bdf),
		AMI_PCI_FUNC(dev->bdf),
		attr
	);

	return open(path, mode);
}

/*
 * Get a fresh handle for a specific device.
 */
static int get_new_device_handle(ami_device **new_dev, uint16_t bdf, bool with_sensors)
{
	int ret = AMI_STATUS_ERROR;
	ami_device *dev = NULL;

	if (!new_dev || (*new_dev))
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	ret = ami_dev_find_next(
		&dev,
		AMI_PCI_BUS(bdf),
		AMI_PCI_DEV(bdf),
		AMI_PCI_FUNC(bdf),
		NULL
	);

	if (ret == AMI_STATUS_OK) {
		/* Check if we need to setup sensors. */
		if (!with_sensors || ((ret = ami_sensor_discover(dev)) == AMI_STATUS_OK)) {
			*new_dev = dev;
		} else {
			/* Sensor discovery failed - clean up the device handle to avoid leak */
			ami_dev_delete(&dev);
		}
	}

	return ret;
}

/*
 * Remove a device from the PCI tree.
 */
static int pci_remove(uint16_t bdf)
{
	int ret = AMI_STATUS_ERROR;
	int file = AMI_INVALID_FD;
	char path[AMI_SYSFS_PATH_MAX] = { 0 };

	snprintf(
		path,
		AMI_SYSFS_PATH_MAX,
		SYSFS_PCI_REMOVE,
		AMI_PCI_BUS(bdf),
		AMI_PCI_DEV(bdf),
		AMI_PCI_FUNC(bdf)
	);

	if ((file = open(path, O_WRONLY)) != AMI_INVALID_FD) {
		if (write(file, SYSFS_ENABLE, strlen(SYSFS_ENABLE)) != AMI_LINUX_STATUS_ERROR)
			ret = AMI_STATUS_OK;
		else
			ret = AMI_API_ERROR(AMI_ERROR_EIO);

		close(file);
	} else {
		ret = AMI_API_ERROR(AMI_ERROR_EBADF);
	}

	return ret;
}

/*
 * Remove all PCI functions on a slot from the PCI tree.
 */
static int pci_remove_slot(uint16_t bdf)
{
	int ret = AMI_STATUS_ERROR;
	int remove_ret = AMI_STATUS_ERROR;
	bool removed_any = false;
	DIR *pci_dir = NULL;
	struct dirent *entry = NULL;
	uint8_t functions[8] = { 0 };
	bool function_seen[8] = { false };
	int function_count = 0;
	int i = 0;
	int j = 0;
	uint8_t bus = AMI_PCI_BUS(bdf);
	uint8_t dev = AMI_PCI_DEV(bdf);
	unsigned int domain = 0;
	unsigned int bus_scan = 0;
	unsigned int dev_scan = 0;
	unsigned int fn_scan = 0;
	char remove_path[AMI_SYSFS_PATH_MAX] = { 0 };

	pci_dir = opendir(SYSFS_PCI_DEVICES_DIR);
	if (!pci_dir)
		return AMI_API_ERROR(AMI_ERROR_EBADF);

	while ((entry = readdir(pci_dir)) != NULL) {
		if (sscanf(entry->d_name, "%x:%x:%x.%x", &domain, &bus_scan, &dev_scan, &fn_scan) != 4)
			continue;

		if ((domain != 0) || (bus_scan != bus) || (dev_scan != dev) || (fn_scan > 7))
			continue;

		if (!function_seen[fn_scan]) {
			functions[function_count++] = (uint8_t)fn_scan;
			function_seen[fn_scan] = true;
		}
	}

	closedir(pci_dir);

	/* Sort discovered functions in descending order (highest -> lowest). */
	for (i = 0; i < (function_count - 1); i++) {
		for (j = (i + 1); j < function_count; j++) {
			if (functions[i] < functions[j]) {
				uint8_t tmp = functions[i];

				functions[i] = functions[j];
				functions[j] = tmp;
			}
		}
	}

	for (i = 0; i < function_count; i++) {
		uint8_t fn = functions[i];
		snprintf(
			remove_path,
			AMI_SYSFS_PATH_MAX,
			SYSFS_PCI_REMOVE,
			bus,
			dev,
			fn
		);

		/* The remove call for one function may drop sibling nodes too. */
		if (access(remove_path, F_OK) == AMI_LINUX_STATUS_ERROR)
			continue;

		remove_ret = pci_remove(AMI_MK_BDF(bus, dev, fn));
		if (remove_ret != AMI_STATUS_OK)
			return remove_ret;

		removed_any = true;
	}

	if (!removed_any) {
		return AMI_API_ERROR(AMI_ERROR_ENODEV);
	}

	ret = AMI_STATUS_OK;
	return ret;
}

/*
 * Rescan the PCI bus.
 */
static int pci_rescan(void)
{
	int ret = AMI_STATUS_ERROR;
	int file = AMI_INVALID_FD;

	if ((file = open(SYSFS_PCI_RESCAN, O_WRONLY)) != AMI_INVALID_FD) {
		if (write(file, SYSFS_ENABLE, strlen(SYSFS_ENABLE)) != AMI_LINUX_STATUS_ERROR)
			ret = AMI_STATUS_OK;
		else
			ret = AMI_API_ERROR(AMI_ERROR_EIO);

		close(file);
	} else {
		ret = AMI_API_ERROR(AMI_ERROR_EBADF);
	}

	return ret;
}


/*****************************************************************************/
/* Private API function definitions                                          */
/*****************************************************************************/

/*
 * Open a character device file.
*/
int ami_open_cdev(ami_device *dev)
{
	int ret = AMI_STATUS_ERROR;
	int fd = AMI_INVALID_FD;
	char path[AMI_DEV_NAME_MAX] = { 0 };
	FILE *log = NULL;
	GCQCfg gcq_cfg;
	void *bar0;
	char product_name_buf[AMI_MFG_INFO_MAX_STR] = {0};

	log = fopen("/tmp/ami_sensor_debug.log", "a");
	if (log) {
		fprintf(log, "[OPEN_CDEV] Entry: dev=%p cdev=%d\n", dev, dev ? dev->cdev : -999);
		fclose(log);
	}

	if (!dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (dev->use_userspace_gcq && dev->gcq_ctx) {
		log = fopen("/tmp/ami_sensor_debug.log", "a");
		if (log) {
			fprintf(log, "[OPEN_CDEV] Userspace GCQ already opened, ctx=%p\n",
				dev->gcq_ctx);
			fclose(log);
		}
		return AMI_STATUS_OK;
	}

	if (dev->cdev != AMI_INVALID_FD) {
		log = fopen("/tmp/ami_sensor_debug.log", "a");
		if (log) {
			fprintf(log, "[OPEN_CDEV] Already opened, cdev=%d\n", dev->cdev);
			fclose(log);
		}
		return AMI_STATUS_OK;  /* Device already opened */
	}

	uint8_t bus = (dev->bdf >> 8) & 0xFF;
	uint8_t device = (dev->bdf >> 3) & 0x1F;
	uint8_t func = dev->bdf & 0x7;

	/* Detect if device is bound to vfio-pci */
	{
		char bdf_str[32];
		char driver_symlink[256];
		char driver_target[256];
		ssize_t link_len;
		bool vfio_bound = false;

		snprintf(bdf_str, sizeof(bdf_str), "0000:%02x:%02x.%x", bus, device, func);
		snprintf(driver_symlink, sizeof(driver_symlink),
			 "/sys/bus/pci/devices/%s/driver", bdf_str);
		link_len = readlink(driver_symlink, driver_target,
				    sizeof(driver_target) - 1);
		if (link_len > 0) {
			driver_target[link_len] = '\0';
			vfio_bound = (strstr(driver_target, "vfio-pci") != NULL);
		}

		log = fopen("/tmp/ami_sensor_debug.log", "a");
		if (log) {
			fprintf(log, "[OPEN_CDEV] bdf=%s vfio_bound=%d driver=%s\n",
				bdf_str, vfio_bound,
				link_len > 0 ? driver_target : "(none)");
			fclose(log);
		}

		if (!vfio_bound) {
			fprintf(stderr,
				"[AMI] Device %s is not bound to vfio-pci.\n"
				"[AMI] Bind it first (IOMMU must be enabled):\n"
				"[AMI]   modprobe vfio vfio-pci\n"
				"[AMI]   echo \"%s\" > /sys/bus/pci/drivers/vfio-pci/bind\n",
				bdf_str, bdf_str);
		}

		if (vfio_bound) {
			/* Open debug log once for all VFIO operations */
			log = fopen("/tmp/ami_sensor_debug.log", "a");

			if (log)
				fprintf(log, "[OPEN_CDEV] VFIO path for %s\n", bdf_str);

			dev->use_userspace_gcq = true;

			/* Initialize GCQ userspace */
			ret = gcq_userspace_init();
			if (log)
				fprintf(log, "[OPEN_CDEV] gcq_userspace_init returned %d\n", ret);

			if (ret != GCQ_ERRORS_NONE && ret != GCQ_ERRORS_DRIVER_IN_USE) {
				if (log)
					fclose(log);
				return AMI_STATUS_ERROR;
			}

			/* Create GCQ context (use_metal=true → VFIO backend) */
			ret = gcq_userspace_create_context(&dev->gcq_ctx, 0, true);
			if (log)
				fprintf(log,
					"[OPEN_CDEV] gcq_userspace_create_context returned %d, ctx=%p\n",
					ret, dev->gcq_ctx);

			if (ret != GCQ_ERRORS_NONE) {
				if (log)
					fclose(log);
				return AMI_STATUS_ERROR;
			}

			/* Set the PCI BDF so the VFIO open can locate the device */
			gcq_userspace_set_bdf(dev->gcq_ctx, bdf_str);

			/* If caller requested no-wait (e.g. device enumeration),
			 * override the default 60s AMC magic wait to 0 so overview
			 * never blocks holding the VFIO group fd open. */
			if (dev->quick_open)
				gcq_userspace_set_amc_timeout(dev->gcq_ctx, 0);

			/* Open GCQ — hardware layout discovered automatically */
			memset(&gcq_cfg, 0, sizeof(gcq_cfg));
			ret = gcq_userspace_open(dev->gcq_ctx, &gcq_cfg);
			if (log)
				fprintf(log, "[OPEN_CDEV] gcq_userspace_open returned %d\n", ret);

			if (ret != GCQ_ERRORS_NONE) {
				static bool gcq_open_warned;

				if (log)
					fprintf(log,
						"[OPEN_CDEV] ERROR: gcq_userspace_open failed with %d\n",
						ret);
				/* Suppress verbose errors for quick_open (overview/enumeration) */
				if (!dev->quick_open && !gcq_open_warned) {
					gcq_open_warned = true;
					fprintf(stderr,
						"[AMI] GCQ open failed (error %u). "
						"cfgmem_program/sensors need AMC VMR0 at BAR0+AMC_SHARED_MEM_OFFSET. "
						"If magic is 0xffffffff, cold-reboot the card and wait ~60s.\n",
						ret);
				}
				gcq_userspace_destroy_context(dev->gcq_ctx);
				dev->gcq_ctx = NULL;
				gcq_userspace_deinit();
				if (log)
					fclose(log);
				/* For quick_open (enumeration/sensor discovery), return error silently */
				if (dev->quick_open)
					return AMI_STATUS_ERROR;
				return AMI_API_ERROR_M(AMI_ERROR_EIO,
					"GCQ open failed (code %u)", ret);
			}

			ami_gcq_reset_session(dev->gcq_ctx);

			/*
			 * Legacy setup_amc: heartbeat only after IDENTIFY + version
			 * match (skipped in compat mode and quick_open enumeration).
			 */
			if (!dev->quick_open) {
				uint8_t amc_maj = 0, amc_min = 0, amc_pat = 0;

				if (ami_gcq_send_identify(dev->gcq_ctx, &amc_maj,
							  &amc_min, &amc_pat) == 0) {
					/* Cache the result for future calls */
					dev->amc_version_cached = true;
					dev->amc_ver_major = amc_maj;
					dev->amc_ver_minor = amc_min;
					dev->amc_ver_patch = amc_pat;
					dev->amc_ver_cache_ms = ami_monotonic_ms();

					/* Major version defines the command interface;
					 * start the heartbeat for any compatible AMC. */
					if (amc_maj == GIT_TAG_VER_MAJOR) {
						(void)gcq_userspace_start_heartbeat(dev->gcq_ctx);
					}
				}
			}

			/* ----------------------------------------------------------
			 * Cache device info from BAR0 shared memory.
			 * Use the already-mapped BAR0 from gcq_userspace_get_bar0()
			 * instead of opening the device again.
			 * ---------------------------------------------------------- */
			bar0 = gcq_userspace_get_bar0(dev->gcq_ctx);
			if (bar0) {
				volatile struct amc_shared_mem *shm =
					(volatile struct amc_shared_mem *)
					((uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET);
				volatile uint32_t *status;

				if (shm->uuid.amc_uuid_len == 16) {
					uint8_t *uuid = (uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET +
							shm->uuid.amc_uuid_off;

					ami_format_logic_uuid(uuid, dev->cached_uuid);
				} else {
					strncpy(dev->cached_uuid,
						"0000000000000000",
						sizeof(dev->cached_uuid) - 1);
				}

				status = (volatile uint32_t *)
					 ((uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET +
					  shm->status.amc_status_off);
				if (*status == AMC_STATUS_READY)
					strncpy(dev->cached_state, "READY",
						sizeof(dev->cached_state) - 1);
				else
					snprintf(dev->cached_state,
						 sizeof(dev->cached_state),
						 "0x%x", *status);
			} else {
				strncpy(dev->cached_state, "UNKNOWN",
					sizeof(dev->cached_state) - 1);
				strncpy(dev->cached_uuid, "0000000000000000",
					sizeof(dev->cached_uuid) - 1);
			}

			dev->info_cached = true;

			/* Mark as opened (fake FD for legacy compat) */
			dev->cdev = 1;

			/*
			 * Read device name from EEPROM product_name.
			 * Must be done AFTER dev->cdev is set, as ami_mfg_get_info() checks it.
			 */
			if (ami_mfg_get_info(dev, AMI_MFG_PRODUCT_NAME, product_name_buf) == AMI_STATUS_OK) {
				strncpy(dev->cached_name, product_name_buf,
					sizeof(dev->cached_name) - 1);
			} else {
				/* If BDINFO read fails, set to UNKNOWN */
				strncpy(dev->cached_name, "UNKNOWN",
					sizeof(dev->cached_name) - 1);
			}

			/* Close debug log */
			if (log)
				fclose(log);

			return AMI_STATUS_OK;
		}
	}

	/* Fallback to old kernel driver (legacy IOCTL path) */
	dev->use_userspace_gcq = false;

	snprintf(dev->cdev_name, AMI_DEV_NAME_MAX, "ami-bdf-%02x:%02x.%x", bus, device, func);
	snprintf(path, AMI_DEV_NAME_MAX, "/dev/%.32s", dev->cdev_name);

	fd = open(path, O_RDWR | O_NONBLOCK);

	if (fd != AMI_INVALID_FD) {
		dev->cdev = fd;
		ret = AMI_STATUS_OK;
	} else {
		ret = AMI_API_ERROR(AMI_ERROR_EBADF);
	}

	return ret;
}

/*
 * Close a character device file.
 */
int ami_close_cdev(ami_device *dev)
{
	if (!dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* Handle VFIO userspace GCQ path even though cdev is unused. */
	if (dev->use_userspace_gcq) {
		if (dev->gcq_ctx) {
			ami_gcq_reset_session(dev->gcq_ctx);
			gcq_userspace_close(dev->gcq_ctx);
			gcq_userspace_destroy_context(dev->gcq_ctx);
			dev->gcq_ctx = NULL;
		}
		gcq_userspace_deinit();
		dev->cdev = AMI_INVALID_FD;
		return AMI_STATUS_OK;
	}

	if (dev->cdev == AMI_INVALID_FD)
		return AMI_STATUS_OK;  /* Device already closed */

	/* Handle legacy kernel driver path */
	if (close(dev->cdev) != AMI_LINUX_STATUS_ERROR)
		return AMI_STATUS_OK;

	return AMI_API_ERROR(AMI_ERROR_EBADF);
}

/*
 * Read a sysfs attribute.
 */
int ami_read_sysfs(ami_device *dev, const char *attr, char *buf)
{
	int file = AMI_INVALID_FD;
	int ret = AMI_STATUS_ERROR;

	if (!dev || !attr || !buf)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	file = open_sysfs(dev, attr, O_RDONLY);

	if (file != AMI_INVALID_FD) {
		if (read(file, buf, AMI_SYSFS_STR_MAX) != AMI_LINUX_STATUS_ERROR)
			ret = AMI_STATUS_OK;
		else
			ret = AMI_API_ERROR(AMI_ERROR_EIO);

		close(file);
	} else {
		ret = AMI_API_ERROR(AMI_ERROR_EBADF);
	}

	return ret;
}


/*****************************************************************************/
/* Public API function definitions                                           */
/*****************************************************************************/

/*
 * Register the current process with a driver device.
 * Note: This is a kernel driver feature. In VFIO userspace mode, this is a no-op.
 */
int ami_dev_register(ami_device *dev)
{
	if (!dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* VFIO userspace mode doesn't need process registration */
	return AMI_STATUS_OK;
}

/*
 * Deregister the current process from a driver device.
 * Note: This is a kernel driver feature. In VFIO userspace mode, this is a no-op.
 */
int ami_dev_deregister(ami_device *dev)
{
	if (!dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* VFIO userspace mode doesn't need process deregistration */
	return AMI_STATUS_OK;
}

/*
 * Find the next PCI device attached to the AMI driver.
 */
int ami_dev_find_next(ami_device **dev, int b, int d, int f, ami_device *prev)
{
	int ret = AMI_STATUS_ERROR;

	/* For reading the device file. */
	FILE *file = NULL;
	char *line = NULL;
	size_t len = 0;
	bool passed_prev = false;
	int previous_dev = 0;
	int current_line = 0;
	struct ami_version driver_ver = { 0 };
	/* Parsed values. */
	int map[AMI_BDF_MAP_MAX] = { 0, 0, 0, AMI_STATUS_ERROR, AMI_STATUS_ERROR };

	if (!dev || *dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* Check if any devices are bound to vfio-pci */
	if (access("/sys/bus/pci/drivers/vfio-pci", F_OK) == 0) {
		DIR *vfio_dir;
		struct dirent *entry;
		unsigned int domain, bus_num, dev_num, func_num;
		/*
		 * When prev is NULL this is the first call — return the first
		 * matching device.  When prev is set, skip devices up to and
		 * including prev->bdf, then return the next match.
		 */
		bool prev_found = (prev == NULL);

		vfio_dir = opendir("/sys/bus/pci/drivers/vfio-pci");
		if (!vfio_dir)
			return AMI_STATUS_ERROR;

		while ((entry = readdir(vfio_dir)) != NULL) {
			/* Directory entries for bound devices are BDF strings */
			if (sscanf(entry->d_name, "%x:%x:%x.%x",
				   &domain, &bus_num, &dev_num, &func_num) != 4)
				continue;

			/* Skip until we have passed the previously returned device */
			if (!prev_found) {
				uint16_t entry_bdf = (uint16_t)
					((bus_num << 8) | (dev_num << 3) | func_num);
				if (entry_bdf == prev->bdf)
					prev_found = true;
				continue;
			}

			/* Apply BDF filter; skip non-zero functions (secondary PCIe
			 * functions do not expose the GCQ/AMC interface). */
			if ((b == AMI_ANY_DEV || b == (int)bus_num) &&
			    (d == AMI_ANY_DEV || d == (int)dev_num) &&
			    (f == AMI_ANY_DEV || f == (int)func_num) &&
			    (f != AMI_ANY_DEV || func_num == 0)) {
				*dev = calloc(1, sizeof(ami_device));
				if (*dev) {
					(*dev)->bdf = (uint16_t)
						((bus_num << 8) | (dev_num << 3) | func_num);
					(*dev)->cdev = AMI_INVALID_FD;
					(*dev)->use_userspace_gcq = true;
					(*dev)->info_cached = false;
					strncpy((*dev)->cached_name, "UNKNOWN",
						sizeof((*dev)->cached_name) - 1);
					strncpy((*dev)->cached_state, "UNKNOWN",
						sizeof((*dev)->cached_state) - 1);
					(*dev)->cached_uuid[0] = '\0';
					(*dev)->quick_open = true;  /* never stall during enumeration */
					/* Open VFIO device to populate cached info (UUID,
					 * state, name).  Ignore failure — the device is
					 * still enumerated even if firmware is not ready. */
					ami_open_cdev(*dev);
					closedir(vfio_dir);
					return AMI_STATUS_OK;
				}
			}
		}

		closedir(vfio_dir);
		return AMI_STATUS_ERROR;
	}

	/* Legacy kernel driver path */
	/* Check driver version */
	if (ami_get_driver_version(&driver_ver) != AMI_STATUS_ERROR) {
		if ((GIT_TAG_VER_MAJOR != driver_ver.major) || (GIT_TAG_VER_MINOR != driver_ver.minor))
			return AMI_API_ERROR(AMI_ERROR_EVER);
	} else {
		return AMI_STATUS_ERROR;
	}

	file = fopen(AMI_DEVICES_MAP, "r");

	if (file) {
		int nread = 0;
		bool found = false;

		while ((nread = getline(&line, &len, file)) != AMI_LINUX_STATUS_ERROR) {
			/* First line is the number of devices. */
			if (0 == current_line++)
				continue;

			int iScan = sscanf(
				line,
				"%02x:%02x.%1x %d %d",
				&map[AMI_BDF_MAP_BUS],
				&map[AMI_BDF_MAP_DEV],
				&map[AMI_BDF_MAP_FUNC],
				&map[AMI_BDF_MAP_DEVN],
				&map[AMI_BDF_MAP_HWMON]
			);

			if (iScan == AMI_BDF_MAP_MAX) {
				if (!passed_prev && (!prev || (previous_dev == prev->cdev_num))) {
					passed_prev = true;
				}

				if ((passed_prev) &&
					((b == AMI_ANY_DEV) || (map[AMI_BDF_MAP_BUS] == b)) &&
					((d == AMI_ANY_DEV) || (map[AMI_BDF_MAP_DEV] == d)) &&
					((f == AMI_ANY_DEV) || (map[AMI_BDF_MAP_FUNC] == f))) {
					/* Initialise device attributes. */
					found = true;
					*dev = (ami_device*)calloc(1, sizeof(ami_device));

					if (*dev) {
						(*dev)->bdf = AMI_MK_BDF(
							map[AMI_BDF_MAP_BUS],
							map[AMI_BDF_MAP_DEV],
							map[AMI_BDF_MAP_FUNC]
						);
						(*dev)->cdev = AMI_INVALID_FD;
						(*dev)->cdev_num = map[AMI_BDF_MAP_DEVN];
						(*dev)->hwmon_num = map[AMI_BDF_MAP_HWMON];
						ret = ami_dev_register(*dev);
					} else {
						ret = AMI_API_ERROR(AMI_ERROR_ENOMEM);
					}
					break;
				}

				previous_dev = map[AMI_BDF_MAP_DEVN];
			} else {
				ret = AMI_API_ERROR(AMI_ERROR_EFMT);
				break;
			}
		}

		fclose(file);

		if (line)
			free(line);

		/* Check if device was found. */
		if ((nread == AMI_LINUX_STATUS_ERROR) && !found)
			ret = AMI_API_ERROR(AMI_ERROR_ENODEV);
	} else {
		ret = AMI_API_ERROR(AMI_ERROR_EBADF);
	}

	return ret;
}

/*
 * Open a VFIO device by numeric BDF with optional AMC-ready wait.
 */
int ami_dev_open_bdf(ami_device **dev, uint16_t bdf, bool wait_for_amc)
{
	if (!dev || *dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	*dev = calloc(1, sizeof(ami_device));
	if (!*dev)
		return AMI_API_ERROR(AMI_ERROR_ENOMEM);

	(*dev)->bdf = bdf;
	(*dev)->cdev = AMI_INVALID_FD;
	(*dev)->use_userspace_gcq = true;
	(*dev)->info_cached = false;
	(*dev)->quick_open = !wait_for_amc;
	strncpy((*dev)->cached_name, "UNKNOWN", sizeof((*dev)->cached_name) - 1);
	strncpy((*dev)->cached_state, "UNKNOWN", sizeof((*dev)->cached_state) - 1);

	if (ami_open_cdev(*dev) != AMI_STATUS_OK) {
		free(*dev);
		*dev = NULL;
		return AMI_STATUS_ERROR;
	}

	return AMI_STATUS_OK;
}

/*
 * Find a PCIe device with a specific BDF.
 */
int ami_dev_find(const char *bdf, ami_device **dev)
{
	uint16_t bdf_num = 0;

	if (!dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	bdf_num = ami_parse_bdf(bdf);

	return ami_dev_find_next(
		dev,
		AMI_PCI_BUS(bdf_num),
		AMI_PCI_DEV(bdf_num),
		AMI_PCI_FUNC(bdf_num),
		NULL
	);
}

/*
 * Find a device and perform setup logic on it.
 */
int ami_dev_bringup(const char *bdf, ami_device **dev)
{
	int ret = AMI_STATUS_ERROR;

	if (!bdf || !dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_dev_find(bdf, dev) == AMI_STATUS_OK)
		ret = ami_sensor_discover(*dev);

	return ret;
}

/*
 * Free device memory.
 */
void ami_dev_delete(ami_device **dev)
{
	if (dev && *dev) {
		/* Free sensor data. */
		if ((*dev)->sensors) {
			struct ami_sensor *sensor = (*dev)->sensors;
			struct ami_sensor *next = NULL;

			while (sensor) {
				next = sensor->next;

				/* Free private data. */
				free(sensor->sensor_data->temp);
				free(sensor->sensor_data->power);
				free(sensor->sensor_data->current);
				free(sensor->sensor_data->voltage);
				free(sensor->sensor_data);

				/* Free sensor. */
				free(sensor);
				sensor = next;
			}

			(*dev)->num_sensors = 0;
			(*dev)->num_total_sensors = 0;
			(*dev)->sensors = NULL;
		}

		/* Free cached BDINFO SDR repo. */
		if ((*dev)->bdinfo_cache) {
			ami_free_sdr_repo((*dev)->bdinfo_cache);
			free((*dev)->bdinfo_cache);
			(*dev)->bdinfo_cache = NULL;
		}

		/* Cleanup device. */
		ami_dev_deregister(*dev);
		ami_close_cdev(*dev);
		free(*dev);
		*dev = NULL;
	}
}

/*
 * Request elevated device permissions.
 */
int ami_dev_request_access(ami_device *dev)
{
	if (!dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	dev->cap_override = true;
	return AMI_STATUS_OK;
}

/*
 * Remove a device from the PCI tree and rescan the bus.
 */
int ami_dev_pci_reload(ami_device **dev, const char *bdf)
{
	int ret = AMI_STATUS_ERROR;
	uint16_t bdf_num = 0;
	bool has_sensors = false;

	/* Only `bdf` or `dev` may be specified (not both); *dev must be non-NULL */
	if ((bdf && dev) || (!dev && !bdf) || (dev && (!(*dev))))
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (dev) {
		char bdf_str[32];

		bdf_num = (*dev)->bdf;
		has_sensors = ((*dev)->sensors != NULL);
		snprintf(bdf_str, sizeof(bdf_str), "0000:%02x:%02x.%x",
			 AMI_PCI_BUS(bdf_num), AMI_PCI_DEV(bdf_num),
			 AMI_PCI_FUNC(bdf_num));
		gcq_userspace_clear_log_index_for_bdf(bdf_str);
		ami_dev_delete(dev);
	} else {
		bdf_num = ami_parse_bdf(bdf);
		gcq_userspace_clear_log_index_for_bdf(bdf);
	}

	if (pci_remove_slot(bdf_num) == AMI_STATUS_OK) {
		if ((ret = pci_rescan()) == AMI_STATUS_OK) {
			/* Check if we need to update the device handle. */
			if (dev)
				ret = get_new_device_handle(
					dev,
					bdf_num,
					has_sensors
				);
		}
	}

	return ret;
}

/*
 * Perform a hot reset on a device.
 */
int ami_dev_hot_reset(ami_device **dev)
{
	int ret = AMI_STATUS_ERROR;
	char config_path[AMI_SYSFS_PATH_MAX] = { 0 };
	int config = AMI_INVALID_FD;
	char port[AMI_DEV_PCI_PORT_SIZE] = { 0 };
	uint16_t bridge_ctl = 0;

	/* Store data so we can restore the handle later. */
	uint16_t bdf = 0;
	bool has_sensors = false;

	if (!dev || !(*dev))
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/*
	 * For VFIO userspace driver: Pause logging thread to avoid invalid
	 * PCI reads during reset. Do NOT send BOOT_SEL - SBR performs a
	 * complete hardware reset that resets AMC, so BOOT_SEL is not needed
	 * and may interfere with the reset sequence.
	 */
	if ((*dev)->use_userspace_gcq && (*dev)->gcq_ctx) {
		gcq_userspace_pause_logging((*dev)->gcq_ctx);
	}

	/* Need the port so we can write to PCI config space. */
	ret = ami_dev_get_pci_port(*dev, port);

	if (ret)
		return ret;

	snprintf(
		config_path,
		AMI_SYSFS_PATH_MAX,
		SYSFS_PCI_CONFIG,
		port
	);

	config = open(config_path, O_RDWR | O_SYNC);

	if (config == AMI_INVALID_FD)
		return AMI_API_ERROR(AMI_ERROR_EBADF);

	/* Remove device */
	has_sensors = ((*dev)->sensors != NULL);
	bdf = (*dev)->bdf;
	{
		char bdf_str[32];

		snprintf(bdf_str, sizeof(bdf_str), "0000:%02x:%02x.%x",
			 AMI_PCI_BUS(bdf), AMI_PCI_DEV(bdf), AMI_PCI_FUNC(bdf));
		gcq_userspace_clear_log_index_for_bdf(bdf_str);
	}
	ami_dev_delete(dev);
	ret = pci_remove(bdf);
	if (ret)
		goto close_config;

	/*
	 * On some systems, the device that is being reset disappears from the host,
	 * forcing a system reboot - adding a delay before setting the SBR seems
	 * to mitigate this issue.
	 */
	ami_msleep(HOT_RESET_SBR_SET_DELAY_MS);

	/* Toggle SBR */
	/* Read current BRIDGE_CONTROL */
	errno = 0;
	if (lseek(config, PCI_BRIDGE_CONTROL, SEEK_SET) != AMI_LINUX_STATUS_ERROR) {
		errno = 0;
		if (read(config, &bridge_ctl, sizeof(uint16_t)) == AMI_LINUX_STATUS_ERROR) {
			ret = AMI_API_ERROR_M(
				AMI_ERROR_EIO,
				"errno %d (%s)",
				errno,
				strerror(errno)
			);
			goto close_config;
		}
	} else {
		ret = AMI_API_ERROR_M(
			AMI_ERROR_EIO,
			"errno %d (%s)",
			errno,
			strerror(errno)
		);
		goto close_config;
	}

	/* Set SBR - PDI will reload here */
	errno = 0;
	bridge_ctl |= PCI_BRIDGE_CTL_BUS_RESET;
	if (lseek(config, PCI_BRIDGE_CONTROL, SEEK_SET) != AMI_LINUX_STATUS_ERROR) {
		errno = 0;
		if (write(config, &bridge_ctl, sizeof(uint16_t)) == AMI_LINUX_STATUS_ERROR) {
			ret = AMI_API_ERROR_M(
				AMI_ERROR_EIO,
				"errno %d (%s)",
				errno,
				strerror(errno)
			);
			goto close_config;
		}
	} else {
		ret = AMI_API_ERROR_M(
			AMI_ERROR_EIO,
			"errno %d (%s)",
			errno,
			strerror(errno)
		);
		goto close_config;
	}

	/* Wait a short while before resetting the SBR */
	ami_msleep(HOT_RESET_SBR_SET_DELAY_MS);

	/* Reset SBR */
	errno = 0;
	bridge_ctl &= ~PCI_BRIDGE_CTL_BUS_RESET;
	if (lseek(config, PCI_BRIDGE_CONTROL, SEEK_SET) != AMI_LINUX_STATUS_ERROR) {
		errno = 0 ;
		if (write(config, &bridge_ctl, sizeof(uint16_t)) == AMI_LINUX_STATUS_ERROR) {
			ret = AMI_API_ERROR_M(
				AMI_ERROR_EIO,
				"errno %d (%s)",
				errno,
				strerror(errno)
			);
			goto close_config;
		}
	} else {
		ret = AMI_API_ERROR_M(
			AMI_ERROR_EIO,
			"errno %d (%s)",
			errno,
			strerror(errno)
		);
		goto close_config;
	}

	/* Give enough time for the PDI to initialize */
	ami_msleep(HOT_RESET_RESCAN_DELAY_MS);

	/* Finished with the config */
	close(config);

	/* Rescan bus */
	ret = pci_rescan();

	/* Update handle */
	return get_new_device_handle(dev, bdf, has_sensors);

close_config:
	close(config);
	return ret;
}

/*
 * Set the AMC debug verbosity.
 */
int ami_dev_set_amc_debug_level(ami_device *dev, enum ami_amc_debug_level level)
{
	struct amc_proxy_cmd_request req = {0};
	struct com_queue_entry resp;
	uint16_t cid;
	uint32_t result;

	if (!dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR;

	cid = ami_gcq_allocate_cid(dev->gcq_ctx);

	/* Build DEBUG_VERBOSITY request */
	req.hdr.opcode = 0x5;  /* AMC_CMD_OPCODE_DEBUG_VERBOSITY_REQ */
	req.hdr.state = 1;
	req.hdr.cid = cid;
	req.hdr.count = 1;  /* Single byte payload */

	/* Debug level as single byte */
	req.raw_payload[0] = (uint8_t)level;

	/* Send command */
	result = gcq_userspace_write(dev->gcq_ctx, (const uint8_t *)&req, 512, 5000);
	if (result != GCQ_ERRORS_NONE) {
		ami_gcq_free_cid(cid);
		return AMI_API_ERROR(AMI_ERROR_EIO);
	}

	/* Wait for response */
	if (ami_gcq_wait_response(dev->gcq_ctx, cid, &resp, 5000) != 0) {
		ami_gcq_free_cid(cid);
		return AMI_API_ERROR(AMI_ERROR_EIO);
	}

	ami_gcq_free_cid(cid);
	return (resp.rcode == 0) ? AMI_STATUS_OK : AMI_API_ERROR(AMI_ERROR_EIO);
}

/*
 * Read the logic uuid of a device.
 */
int ami_dev_read_uuid(ami_device *dev, char buf[AMI_LOGIC_UUID_SIZE])
{
	int ret = AMI_STATUS_ERROR;
	char raw_buf[AMI_SYSFS_STR_MAX] = { 0 };

	if (!dev || !buf)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* If using VFIO driver, return cached UUID (from BAR peek or GCQ open) */
	if (dev->use_userspace_gcq) {
		if (!dev->info_cached)
			vfio_try_cache_info_from_bar(dev);
		if (dev->info_cached) {
			strncpy(buf, dev->cached_uuid, AMI_LOGIC_UUID_SIZE - 1);
			return AMI_STATUS_OK;
		}
		return AMI_STATUS_ERROR;
	}

	/* Legacy sysfs path */
	if (ami_read_sysfs(dev, SYSFS_LOGIC_UUID, raw_buf) == AMI_STATUS_OK) {
		/*
		 * This will also strip the new line because we're specifying
		 * the maximum size of the logic UUID.
		 */
		strncpy(buf, raw_buf, AMI_LOGIC_UUID_SIZE - 1);
		ret = AMI_STATUS_OK;
	}

	return ret;
}

/*
 * Get the number of attached PCI devices.
 */
int ami_dev_get_num_devices(uint16_t *num)
{
	int ret = AMI_STATUS_ERROR;

	/* For reading the device file. */
	FILE *file = NULL;
	char *line = NULL;
	size_t len = 0;

	if (!num)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* Count devices bound to vfio-pci */
	if (access("/sys/bus/pci/drivers/vfio-pci", F_OK) == 0) {
		DIR *vfio_dir = opendir("/sys/bus/pci/drivers/vfio-pci");
		if (vfio_dir) {
			struct dirent *entry;
			int count = 0;
			unsigned int dom, b2, d2, f2;

			while ((entry = readdir(vfio_dir)) != NULL) {
				if (sscanf(entry->d_name, "%x:%x:%x.%x",
					   &dom, &b2, &d2, &f2) == 4 && f2 == 0)
					count++;
			}
			closedir(vfio_dir);
			*num = count;
			return AMI_STATUS_OK;
		}
		*num = 0;
		return AMI_STATUS_OK;
	}

	/* Legacy kernel driver - read from /proc/ami/devices */
	file = fopen(AMI_DEVICES_MAP, "r");

	if (file) {
		if (getline(&line, &len, file) != AMI_LINUX_STATUS_ERROR) {
			int num_devices = 0;

			if (sscanf(line, "%d", &num_devices) == 1) {
				*num = num_devices;
				ret = AMI_STATUS_OK;
			} else {
				ret = AMI_API_ERROR(AMI_ERROR_ERET);
			}
		}

		fclose(file);

		if (line)
			free(line);
	} else {
		ret = AMI_API_ERROR(AMI_ERROR_EBADF);
	}

	return ret;
}

/*
 * Get the PCI link speed.
 */
int ami_dev_get_pci_link_speed(ami_device *dev, uint8_t *current, uint8_t *max)
{
	int ret = AMI_STATUS_ERROR;
	char raw_buf_c[AMI_SYSFS_STR_MAX] = { 0 };
	char raw_buf_m[AMI_SYSFS_STR_MAX] = { 0 };
	char path[256];
	FILE *fp;

	if (!dev || !current || !max)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* For VFIO driver, read from standard PCI sysfs */
	if (dev->use_userspace_gcq) {
		uint8_t bus = (dev->bdf >> 8) & 0xFF;
		uint8_t device = (dev->bdf >> 3) & 0x1F;
		uint8_t func = dev->bdf & 0x7;

		/* Read current link speed from current_link_speed */
		snprintf(path, sizeof(path), "/sys/bus/pci/devices/0000:%02x:%02x.%x/current_link_speed", bus, device, func);
		fp = fopen(path, "r");
		if (fp) {
			char speed_str[32];
			if (fscanf(fp, "%s", speed_str) == 1) {
				/* Parse "8.0 GT/s PCIe" -> extract 8.0 and convert to Gen */
				float speed_gts;
				if (sscanf(speed_str, "%f", &speed_gts) == 1) {
					/* 2.5 GT/s = Gen1, 5.0 = Gen2, 8.0 = Gen3, 16.0 = Gen4, 32.0 = Gen5 */
					if (speed_gts >= 30.0) *current = 5;
					else if (speed_gts >= 15.0) *current = 4;
					else if (speed_gts >= 7.0) *current = 3;
					else if (speed_gts >= 4.0) *current = 2;
					else *current = 1;
				}
			}
			fclose(fp);
		}

		/* Read max link speed from max_link_speed */
		snprintf(path, sizeof(path), "/sys/bus/pci/devices/0000:%02x:%02x.%x/max_link_speed", bus, device, func);
		fp = fopen(path, "r");
		if (fp) {
			char speed_str[32];
			if (fscanf(fp, "%s", speed_str) == 1) {
				float speed_gts;
				if (sscanf(speed_str, "%f", &speed_gts) == 1) {
					if (speed_gts >= 30.0) *max = 5;
					else if (speed_gts >= 15.0) *max = 4;
					else if (speed_gts >= 7.0) *max = 3;
					else if (speed_gts >= 4.0) *max = 2;
					else *max = 1;
					ret = AMI_STATUS_OK;
				}
			}
			fclose(fp);
		}
		return ret;
	}

	/* Legacy kernel driver path */
	if (ami_read_sysfs(dev, SYSFS_PCI_LINK_SPEED_M, raw_buf_m) == AMI_STATUS_OK) {
		if (ami_read_sysfs(dev, SYSFS_PCI_LINK_SPEED_C, raw_buf_c) == AMI_STATUS_OK) {
			uint8_t c = 0, m = 0;

			if (sscanf(raw_buf_c, "%hhd", &c) != 1)
				return AMI_API_ERROR(AMI_ERROR_ERET);

			if (sscanf(raw_buf_m, "%hhd", &m) != 1)
				return AMI_API_ERROR(AMI_ERROR_ERET);

			*current = c;
			*max = m;
			ret = AMI_STATUS_OK;
		}
	}

	return ret;
}

/*
 * Get the PCI link width.
 */
int ami_dev_get_pci_link_width(ami_device *dev, uint8_t *current, uint8_t *max)
{
	int ret = AMI_STATUS_ERROR;
	char raw_buf_c[AMI_SYSFS_STR_MAX] = { 0 };
	char raw_buf_m[AMI_SYSFS_STR_MAX] = { 0 };
	char path[256];
	FILE *fp;

	if (!dev || !current || !max)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* For VFIO driver, read from standard PCI sysfs */
	if (dev->use_userspace_gcq) {
		uint8_t bus = (dev->bdf >> 8) & 0xFF;
		uint8_t device = (dev->bdf >> 3) & 0x1F;
		uint8_t func = dev->bdf & 0x7;

		/* Read current link width */
		snprintf(path, sizeof(path), "/sys/bus/pci/devices/0000:%02x:%02x.%x/current_link_width", bus, device, func);
		fp = fopen(path, "r");
		if (fp && fscanf(fp, "%hhd", current) == 1) {
			fclose(fp);
		} else {
			if (fp) fclose(fp);
			*current = 0;
		}

		/* Read max link width */
		snprintf(path, sizeof(path), "/sys/bus/pci/devices/0000:%02x:%02x.%x/max_link_width", bus, device, func);
		fp = fopen(path, "r");
		if (fp && fscanf(fp, "%hhd", max) == 1) {
			fclose(fp);
			ret = AMI_STATUS_OK;
		} else {
			if (fp) fclose(fp);
			*max = 0;
		}
		return ret;
	}

	/* Legacy kernel driver path */
	if (ami_read_sysfs(dev, SYSFS_PCI_LINK_WIDTH_M, raw_buf_m) == AMI_STATUS_OK) {
		if (ami_read_sysfs(dev, SYSFS_PCI_LINK_WIDTH_C, raw_buf_c) == AMI_STATUS_OK) {
			uint8_t c = 0, m = 0;

			if (sscanf(raw_buf_c, "%hhd", &c) != 1)
				return AMI_API_ERROR(AMI_ERROR_ERET);

			if (sscanf(raw_buf_m, "%hhd", &m) != 1)
				return AMI_API_ERROR(AMI_ERROR_ERET);

			*current = c;
			*max = m;
			ret = AMI_STATUS_OK;
		}
	}

	return ret;
}

/*
 * Get the PCI vendor.
 */
int ami_dev_get_pci_vendor(ami_device *dev, uint16_t *vendor)
{
	int ret = AMI_STATUS_ERROR;
	char raw_buf[AMI_SYSFS_STR_MAX] = { 0 };
	char path[256];
	FILE *fp;

	if (!dev || !vendor)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* For VFIO driver, read from PCI sysfs directly */
	if (dev->use_userspace_gcq) {
		uint8_t bus = (dev->bdf >> 8) & 0xFF;
		uint8_t device = (dev->bdf >> 3) & 0x1F;
		uint8_t func = dev->bdf & 0x7;

		snprintf(path, sizeof(path), "/sys/bus/pci/devices/0000:%02x:%02x.%x/vendor", bus, device, func);
		fp = fopen(path, "r");
		if (fp && fscanf(fp, "0x%hx", vendor) == 1) {
			ret = AMI_STATUS_OK;
		}
		if (fp) fclose(fp);
		return ret;
	}

	/* Legacy kernel driver path */
	if (ami_read_sysfs(dev, SYSFS_PCI_VENDOR, raw_buf) == AMI_STATUS_OK) {
		uint16_t v = 0;

		if (sscanf(raw_buf, "0x%hx", &v) != 1)
			return AMI_API_ERROR(AMI_ERROR_ERET);

		*vendor = v;
		ret = AMI_STATUS_OK;
	}

	return ret;
}

/*
 * Get the PCI device number.
 */
int ami_dev_get_pci_device(ami_device *dev, uint16_t *device)
{
	int ret = AMI_STATUS_ERROR;
	char raw_buf[AMI_SYSFS_STR_MAX] = { 0 };
	char path[256];
	FILE *fp;

	if (!dev || !device)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* For VFIO driver, read from PCI sysfs directly */
	if (dev->use_userspace_gcq) {
		uint8_t bus = (dev->bdf >> 8) & 0xFF;
		uint8_t dev_num = (dev->bdf >> 3) & 0x1F;
		uint8_t func = dev->bdf & 0x7;

		snprintf(path, sizeof(path), "/sys/bus/pci/devices/0000:%02x:%02x.%x/device", bus, dev_num, func);
		fp = fopen(path, "r");
		if (fp && fscanf(fp, "0x%hx", device) == 1) {
			ret = AMI_STATUS_OK;
		}
		if (fp) fclose(fp);
		return ret;
	}

	/* Legacy kernel driver path */
	if (ami_read_sysfs(dev, SYSFS_PCI_DEVICE, raw_buf) == AMI_STATUS_OK) {
		uint16_t d = 0;

		if (sscanf(raw_buf, "0x%hx", &d) != 1)
			return AMI_API_ERROR(AMI_ERROR_ERET);

		*device = d;
		ret = AMI_STATUS_OK;
	}

	return ret;
}

/*
 * Get the PCI NUMA node.
 */
int ami_dev_get_pci_numa_node(ami_device *dev, uint8_t *node)
{
	int ret = AMI_STATUS_ERROR;
	char raw_buf[AMI_SYSFS_STR_MAX] = { 0 };

	if (!dev || !node)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_read_sysfs(dev, SYSFS_PCI_NUMA_NODE, raw_buf) == AMI_STATUS_OK) {
		uint8_t n = 0;

		if (sscanf(raw_buf, "%hhd", &n) != 1)
			return AMI_API_ERROR(AMI_ERROR_ERET);

		*node = n;
		ret = AMI_STATUS_OK;
	}

	return ret;
}

/*
 * Get the PCI CPU affinity.
 */
int ami_dev_get_pci_cpulist(ami_device *dev, char buf[AMI_PCI_CPULIST_SIZE])
{
	int ret = AMI_STATUS_ERROR;
	char raw_buf[AMI_SYSFS_STR_MAX] = { 0 };

	if (!dev || !buf)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_read_sysfs(dev, SYSFS_PCI_CPULIST, raw_buf) == AMI_STATUS_OK) {
		/* Strip newline */
		raw_buf[strcspn(raw_buf, "\r\n")] = 0;
		memset(buf, 0x00, AMI_PCI_CPULIST_SIZE);
		strncpy(buf, raw_buf, AMI_PCI_CPULIST_SIZE - 1);
		ret = AMI_STATUS_OK;
	}

	return ret;
}

/*
 * Freshness window for the cached AMC IDENTIFY result. IDENTIFY costs ~1s on
 * some AMC firmware, and a single command (e.g. overview) reads the version
 * from several code paths microseconds apart. Caching within this window lets
 * those share one IDENTIFY, while a longer-lived caller re-probes afterwards
 * so a crashed or updated AMC is still detected.
 */
#define AMC_VERSION_CACHE_TTL_MS	(1000)

/*
 * Monotonic milliseconds (CLOCK_MONOTONIC) for cache-age comparisons.
 */
static uint64_t ami_monotonic_ms(void)
{
	struct timespec ts = { 0 };

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((uint64_t)ts.tv_sec * 1000) + ((uint64_t)ts.tv_nsec / 1000000);
}

/*
 * True if the cached AMC version is still within the freshness window.
 */
static bool ami_amc_version_cache_fresh(const ami_device *dev)
{
	if (!dev->amc_version_cached)
		return false;

	return (ami_monotonic_ms() - dev->amc_ver_cache_ms) < AMC_VERSION_CACHE_TTL_MS;
}

/*
 * True if a recent IDENTIFY probe failed within the freshness window. Overview
 * probes the AMC from several code paths; without this, a card that no longer
 * answers IDENTIFY would incur a full ~5s timeout on each path. The window is
 * bounded so a card that recovers is re-probed shortly after.
 */
static bool ami_amc_probe_failed_fresh(const ami_device *dev)
{
	if (!dev->amc_probe_failed)
		return false;

	return (ami_monotonic_ms() - dev->amc_probe_fail_ms) < AMC_VERSION_CACHE_TTL_MS;
}

/*
 * Check if AMC is responsive via GCQ IDENTIFY command.
 */
bool ami_dev_is_amc_responsive(ami_device *dev)
{
	uint8_t maj = 0, min = 0, pat = 0;

	if (!dev || !dev->use_userspace_gcq || !dev->gcq_ctx)
		return false;

	/* Use cached version if fresh to avoid the slow IDENTIFY call */
	if (ami_amc_version_cache_fresh(dev)) {
		return true;
	}

	/* A recent probe already failed — don't pay another IDENTIFY timeout */
	if (ami_amc_probe_failed_fresh(dev)) {
		return false;
	}

	/* Try IDENTIFY command with 5 second timeout (same as ami_dev_get_amc_version) */
	if (ami_gcq_send_identify(dev->gcq_ctx, &maj, &min, &pat) == 0) {
		/* Cache the result for future calls */
		dev->amc_version_cached = true;
		dev->amc_ver_major = maj;
		dev->amc_ver_minor = min;
		dev->amc_ver_patch = pat;
		dev->amc_ver_cache_ms = ami_monotonic_ms();
		dev->amc_probe_failed = false;
		return true;
	}

	dev->amc_probe_failed = true;
	dev->amc_probe_fail_ms = ami_monotonic_ms();
	return false;
}

/*
 * Get the device state.
 */
int ami_dev_get_state(ami_device *dev, char buf[AMI_DEV_STATE_SIZE])
{
	int ret = AMI_STATUS_ERROR;
	char raw_buf[AMI_DEV_STATE_SIZE] = { 0 };

	if (!dev || !buf)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/*
	 * Userspace/VFIO path: report one of three states.
	 *   NO_AMC - AMC does not answer the IDENTIFY probe (no communication)
	 *   COMPAT - AMC answered but its version does not match this AMI build
	 *   READY  - AMC answered and version is compatible
	 *
	 * Freshness is bounded by AMC_VERSION_CACHE_TTL_MS rather than forcing a
	 * fresh IDENTIFY on every call: a crashed AMC is still detected once the
	 * cache window lapses, without paying the ~1s IDENTIFY twice per command.
	 */
	if (dev->use_userspace_gcq) {
		struct amc_version ver = { 0 };
		char serial[AMI_MFG_INFO_MAX_STR] = { 0 };

		memset(buf, 0x00, AMI_DEV_STATE_SIZE);

		/*
		 * Reuse a recently probed AMC version (see AMC_VERSION_CACHE_TTL_MS):
		 * within a single command the responsiveness probe here and the AMC
		 * version column share one IDENTIFY instead of issuing the ~1s call
		 * twice. Once the window expires ami_dev_is_amc_responsive() re-probes,
		 * so a crashed AMC is still reported as NO_AMC.
		 */
		bool responsive = ami_dev_is_amc_responsive(dev);
		int get_ver_ret = ami_dev_get_amc_version(dev, &ver);

		if (!responsive || get_ver_ret != AMI_STATUS_OK) {
			strncpy(buf, "NO_AMC", AMI_DEV_STATE_SIZE - 1);
			return AMI_STATUS_OK;
		}

		/*
		 * Verify basic device functionality by checking if manufacturing
		 * info (serial number) can be read. If this fails, the device is
		 * not fully functional (e.g., broken firmware/FPT) and should show
		 * NO_AMC rather than READY.
		 */
		int serial_ret = ami_mfg_get_info(dev, AMI_MFG_BOARD_SERIAL, serial);
		bool has_serial = (serial_ret == AMI_STATUS_OK && serial[0] != '\0');

		if (!has_serial) {
			strncpy(buf, "NO_AMC", AMI_DEV_STATE_SIZE - 1);
			return AMI_STATUS_OK;
		}

		if (getenv("AMI_DEBUG_STATE")) {
			fprintf(stderr, "[DEBUG] BDF=%02x:%02x.%01x responsive=%d get_ver=%d ver=%u.%u.%u serial_ret=%d has_serial=%d\n",
				(dev->bdf >> 8) & 0xFF, (dev->bdf >> 3) & 0x1F, dev->bdf & 0x7,
				responsive, get_ver_ret,
				ver.major, ver.minor, ver.patch,
				serial_ret, has_serial);
		}

		/*
		 * Determine state based on version compatibility.
		 * The command structure is tied to the major version only;
		 * minor/patch bumps don't change the AMC command interface, so
		 * any AMC sharing this AMI's major version is READY (e.g. AMC
		 * 3.0.0 against AMI 3.1.0).
		 */
		if (ver.major != GIT_TAG_VER_MAJOR) {
			strncpy(buf, "COMPAT", AMI_DEV_STATE_SIZE - 1);
		} else {
			strncpy(buf, "READY", AMI_DEV_STATE_SIZE - 1);
		}

		return AMI_STATUS_OK;
	}

	/* Legacy sysfs path */
	if (ami_read_sysfs(dev, SYSFS_DEV_STATE, raw_buf) == AMI_STATUS_OK) {
		/* Strip newline */
		raw_buf[strcspn(raw_buf, "\r\n")] = 0;
		memset(buf, 0x00, AMI_DEV_STATE_SIZE);
		strncpy(buf, raw_buf, AMI_DEV_STATE_SIZE - 1);
		ret = AMI_STATUS_OK;
	}

	return ret;
}

/*
 * Get the device name.
 */
int ami_dev_get_name(ami_device *dev, char buf[AMI_DEV_NAME_SIZE])
{
	int ret = AMI_STATUS_ERROR;
	char raw_buf[AMI_DEV_NAME_SIZE] = { 0 };

	if (!dev || !buf)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* If using VFIO, return cached name (read at device open time) */
	if (dev->use_userspace_gcq) {
		if (dev->info_cached) {
			strncpy(buf, dev->cached_name, AMI_DEV_NAME_SIZE - 1);
			return AMI_STATUS_OK;
		}
		return AMI_STATUS_ERROR;
	}

	/* Legacy sysfs path */
	if (ami_read_sysfs(dev, SYSFS_DEV_NAME, raw_buf) == AMI_STATUS_OK) {
		/* Strip newline */
		raw_buf[strcspn(raw_buf, "\r\n")] = 0;
		memset(buf, 0x00, AMI_DEV_NAME_SIZE);
		strncpy(buf, raw_buf, AMI_DEV_NAME_SIZE - 1);
		ret = AMI_STATUS_OK;
	}

	return ret;
}

/*
 * Get AMC version.
 */
int ami_dev_get_amc_version(ami_device *dev, struct amc_version *amc_version)
{
	int ret = AMI_STATUS_ERROR;
	char raw_buf[AMC_VERSION_ATTR_SIZE] = { 0 };

	if (!dev || !amc_version)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* If using VFIO/userspace GCQ path, query firmware via IDENTIFY */
	if (dev->use_userspace_gcq && dev->gcq_ctx) {
		uint8_t maj = 0, min = 0, pat = 0;

		/* Use cached version if fresh to avoid the slow IDENTIFY call */
		if (ami_amc_version_cache_fresh(dev)) {
			amc_version->major = dev->amc_ver_major;
			amc_version->minor = dev->amc_ver_minor;
			amc_version->patch = dev->amc_ver_patch;
			amc_version->dev_commits = 0;
			amc_version->local_changes = 0;
			return AMI_STATUS_OK;
		}

		/* A recent probe already failed — skip the IDENTIFY timeout and
		 * report zeroes immediately. */
		if (ami_amc_probe_failed_fresh(dev)) {
			amc_version->major = 0;
			amc_version->minor = 0;
			amc_version->patch = 0;
			amc_version->dev_commits = 0;
			amc_version->local_changes = 0;
			return AMI_STATUS_OK;
		}

		/* Query firmware version using IDENTIFY GCQ command.
		 * This is the reliable, authoritative source for the firmware version. */
		if (ami_gcq_send_identify(dev->gcq_ctx, &maj, &min, &pat) == 0) {
			/* Cache the result for future calls */
			dev->amc_version_cached = true;
			dev->amc_ver_major = maj;
			dev->amc_ver_minor = min;
			dev->amc_ver_patch = pat;
			dev->amc_ver_cache_ms = ami_monotonic_ms();
			dev->amc_probe_failed = false;

			amc_version->major = maj;
			amc_version->minor = min;
			amc_version->patch = pat;
			amc_version->dev_commits = 0;
			amc_version->local_changes = 0;
			return AMI_STATUS_OK;
		}

		dev->amc_probe_failed = true;
		dev->amc_probe_fail_ms = ami_monotonic_ms();

		/* IDENTIFY command failed — return zeroes rather than error */
		amc_version->major = 0;
		amc_version->minor = 0;
		amc_version->patch = 0;
		amc_version->dev_commits = 0;
		amc_version->local_changes = 0;
		return AMI_STATUS_OK;
	}

	/* Legacy sysfs path */
	if (ami_read_sysfs(dev, SYSFS_AMC_VERSION, raw_buf) == AMI_STATUS_OK) {
		int scan = sscanf(
			raw_buf,
			AMC_VERSION_ATTR_FMT,
			&amc_version->major,
			&amc_version->minor,
			&amc_version->patch,
			&amc_version->dev_commits,
			&amc_version->local_changes
		);

		if (scan == AMC_VERSION_ATTR_FIELDS)
			ret = AMI_STATUS_OK;
		else
			ret = AMI_API_ERROR(AMI_ERROR_EFMT);
	}

	return ret;
}

/*
 * Get the PCI port of a device.
 */
int ami_dev_get_pci_port(ami_device *dev, char buf[AMI_DEV_PCI_PORT_SIZE])
{
	int ret = AMI_STATUS_ERROR;
	char dev_path[AMI_SYSFS_PATH_MAX] = { 0 };
	char link_path[AMI_SYSFS_PATH_MAX] = { 0 };

	if (!dev || !buf)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	snprintf(
		dev_path,
		AMI_SYSFS_PATH_MAX,
		PCI_DEV_DIR,
		AMI_PCI_BUS(dev->bdf),
		AMI_PCI_DEV(dev->bdf),
		AMI_PCI_FUNC(dev->bdf)
	);

	errno = 0;
	if (readlink(dev_path, link_path, AMI_SYSFS_PATH_MAX) == AMI_LINUX_STATUS_ERROR) {
		ret = AMI_API_ERROR_M(
				AMI_ERROR_ERET,
				"errno %d (%s)",
				errno,
				strerror(errno)
			);
	} else {
		strncpy(
			buf,
			basename(dirname(link_path)),
			AMI_DEV_PCI_PORT_SIZE
		);
		buf[AMI_DEV_PCI_PORT_SIZE - 1] = '\0';

		ret = AMI_STATUS_OK;
	}

	return ret;
}

/****************************** Device Getters ********************************/

/*
 * Get device BDF.
 */
int ami_dev_get_pci_bdf(ami_device *dev, uint16_t *bdf)
{
	if (!dev || !bdf)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	*bdf = dev->bdf;
	return AMI_STATUS_OK;
}

/*
 * Get cdev number.
 */
int ami_dev_get_cdev_num(ami_device *dev, int *num)
{
	if (!dev || !num)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	*num = dev->cdev_num;
	return AMI_STATUS_OK;
}

/*
 * Get hwmon number.
 */
int ami_dev_get_hwmon_num(ami_device *dev, int *num)
{
	if (!dev || !num)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	*num = dev->hwmon_num;
	return AMI_STATUS_OK;
}
