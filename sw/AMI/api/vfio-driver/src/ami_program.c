// SPDX-License-Identifier: MIT
/*
 * ami_program.c - This file contains the implementation of device programming logic.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
 */

/* Standard includes */
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>
#include <endian.h>

/* Public API includes */
#include "ami_program.h"

/* Private API includes */
#include "ami_internal.h"
#include "ami_device_internal.h"
#include "ami_sdr.h"
#include "ami_gcq_common.h"
#include "gcq_userspace.h"
#include "md5.h"
#include "ami_mfg_info.h"
#include "ami_ioctl.h"

/*****************************************************************************/
/* Private functions                                                         */
/*****************************************************************************/

/**
 * read_file() - Read an entire file into a byte buffer.
 * @fname: Full path to file.
 * @buf: Pointer to byte buffer.
 * @size: Pointer to variable which will hold buffer size.
 *
 * Note that the caller is responsible for freeing the memory allocated
 * by this function.
 *
 * Return: AMI_STATUS_OK or AMI_STATUS_ERROR.
 */
static int read_file(const char *fname, uint8_t **buf, uint32_t *size)
{
	FILE *fp = NULL;
	long offset = 0;
	size_t len = 0;
	int ret = AMI_STATUS_ERROR;
	uint8_t *buffer = NULL;

	if (!fname || !buf || !size)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	fp = fopen(fname, "rb");

	if (fp == NULL)
		return AMI_API_ERROR(AMI_ERROR_EBADF);

	/* Go to end of file. */
	if (fseek(fp, 0L, SEEK_END) != AMI_LINUX_STATUS_OK) {
		ret = AMI_API_ERROR(AMI_ERROR_EIO);
		goto close;
	}

	offset = ftell(fp);
	if (offset == AMI_LINUX_STATUS_ERROR) {
		ret = AMI_API_ERROR(AMI_ERROR_EIO);
		goto close;
	}

	buffer = (uint8_t*)malloc(sizeof(uint8_t) * (offset + 1));

	if (!buffer) {
		ret = AMI_API_ERROR(AMI_ERROR_ENOMEM);
		goto close;
	}

	/* Go back to the start of the file. */
	if (fseek(fp, 0L, SEEK_SET) != AMI_LINUX_STATUS_OK) {
		ret = AMI_API_ERROR(AMI_ERROR_EIO);
		goto del_buf;
	}

	len = fread(buffer, sizeof(uint8_t), offset, fp);

	if (ferror(fp) == AMI_LINUX_STATUS_OK) {
		*size = (uint32_t)len;
		*buf = buffer;
		ret = AMI_STATUS_OK;
	} else {
		ret = AMI_API_ERROR(AMI_ERROR_EIO);
		free(buffer);
	}
	fclose(fp);
	return ret;

del_buf:
	free(buffer);

close:
	fclose(fp);
	return ret;
}

/**
 * do_image_download_vfio() - Perform image download via VFIO/GCQ (chunked transfer)
 * @dev: Device handle.
 * @path: Path to image file.
 * @boot_device: Target boot device.
 * @partition: Partition number to program.
 * @progress_handler: Progress handler callback (optional).
 *
 * This function implements boot tag invalidation for multi-chunk flash programming
 * to prevent device bricking if download fails mid-way. For multi-chunk downloads:
 *   1. Write chunk 0 with PDI boot tag invalidated (at byte offset 0x14 in the image)
 *   2. Write chunks 1, 2, 3, ... N
 *   3. Rewrite chunk 0 with valid data (restores boot tag)
 *
 * Return: AMI_STATUS_OK or AMI_STATUS_ERROR
 */
static int do_image_download_gcq(ami_device *dev, const char *path, uint8_t boot_device,
	uint32_t partition, ami_event_handler progress_handler)
{
	uint8_t *img_data = NULL;
	uint32_t img_size = 0;
	int ret = AMI_STATUS_ERROR;
	uint8_t pdi_md5[16];
	struct ami_pdi_progress progress = { 0 };

	/*
	 * Chunk parameters for VFIO userspace driver:
	 *
	 * Kernel driver uses 6MB for everything (PDI_CHUNK_SIZE * PDI_CHUNK_MULTIPLIER = 6144 * 1024)
	 * but VFIO userspace driver has different constraints:
	 *
	 * - LIVELOAD (PDI programming): 1MB works reliably, 6MB caused crashes (tested)
	 * - FLASH (FPT updates): Trying 1MB (increased from 256KB which crashed)
	 *
	 * Theory: PDI programming uses firmware DDR buffer which may be smaller than 6MB.
	 * FPT flash writes should tolerate 1MB chunks if live-load does.
	 */
	const uint32_t FLASH_CHUNK_SIZE = 1024 * 1024;     /* 1 MB - same as live-load */
	const uint32_t LIVELOAD_CHUNK_SIZE = 1024 * 1024;  /* 1 MB - tested working */
	uint32_t chunk_size;
	uint32_t num_chunks;
	uint32_t chunk_num;
	uint32_t bytes_sent = 0;
	bool rewrite_boot_tag = false;

	/* Boot tag invalidation (matches APC HAL_APC_PDI_BIT_MASK in firmware) */
	#define INVALID_BOOT_TAG       0xFFFFFFFF
	#define AMI_PDI_BOOT_TAG_OFFSET 0x14
	#define BOOT_TAG_CHUNK          0

	if (!dev || !path)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (!dev->gcq_ctx)
		return AMI_STATUS_ERROR;

	/* Pause heartbeats — 4-slot CQ must not see HB completions mid-chunk */
	gcq_userspace_pause_heartbeat(dev->gcq_ctx, true);

	/* Brief settle after GCQ open (IDENTIFY optional; rcode 3 is non-fatal on some builds) */
	usleep(200000);
	ami_gcq_reset_session(dev->gcq_ctx);
	ami_gcq_purge_cq_completions(dev->gcq_ctx);

	/* Read entire file */
	if (read_file(path, &img_data, &img_size) != AMI_STATUS_OK) {
		ret = AMI_STATUS_ERROR;
		goto cleanup;
	}

	/* Calculate MD5 for entire file */
	calculate_md5(img_data, img_size, pdi_md5);

	if (progress_handler)
		progress.bytes_to_write = img_size;

	bool is_live_load = (partition == AMI_IOC_PDI_PROGRAM_MAGIC ||
	                     partition == AMI_IOC_PDI_APU_PROGRAM_MAGIC ||
	                     partition == AMI_IOC_PDI_RPU_PROGRAM_MAGIC);
	bool is_fpt_update = (partition == AMI_IOC_FPT_UPDATE_MAGIC);

	chunk_size = is_live_load ? LIVELOAD_CHUNK_SIZE : FLASH_CHUNK_SIZE;
	num_chunks = (img_size + chunk_size - 1) / chunk_size;

	/* Determine if we need boot tag invalidation (matches kernel driver logic):
	 * - Only for multi-chunk downloads (num_chunks > 1)
	 * - Only for flash programming (not live-load)
	 * - Not for FPT updates
	 */
	rewrite_boot_tag = (!is_live_load && !is_fpt_update && (num_chunks > 1));

	fprintf(stderr,
		"[%s] image=%u bytes, %u chunks (%u KiB stride), boot_tag_rewrite=%d\n",
		is_live_load ? "PDI_PROGRAM" : "PDI_DOWNLOAD",
		img_size, num_chunks, chunk_size / 1024, rewrite_boot_tag ? 1 : 0);

	/* STEP 1: Write first chunk with boot tag invalidated at offset 0x14.
	 * APC verifies the full transfer; a 4-byte-only write at flash offset 0 fails
	 * verification and puts the AMI proxy into ERROR (repeated 4-byte attempts).
	 */
	if (rewrite_boot_tag) {
		uint32_t chunk0_len = (chunk_size > img_size) ? img_size : chunk_size;
		uint8_t *chunk0_inv = (uint8_t *)malloc(chunk0_len);

		if (!chunk0_inv) {
			ret = AMI_API_ERROR(AMI_ERROR_ENOMEM);
			goto cleanup;
		}

		memcpy(chunk0_inv, img_data, chunk0_len);
		if (chunk0_len > AMI_PDI_BOOT_TAG_OFFSET + sizeof(uint32_t))
			*(uint32_t *)(chunk0_inv + AMI_PDI_BOOT_TAG_OFFSET) = INVALID_BOOT_TAG;

		ret = ami_gcq_pdi_download_chunk(
			dev->gcq_ctx,
			chunk0_inv,
			chunk0_len,
			BOOT_TAG_CHUNK,
			chunk_size / 1024,
			false,
			boot_device,
			partition,
			pdi_md5,
			img_size,
			progress_handler,
			progress_handler ? &progress : NULL
		);

		free(chunk0_inv);

		if (ret != 0) {
			ret = AMI_API_ERROR(AMI_ERROR_EIO);
			goto cleanup;
		}
	}

	/* STEP 2: Write chunks (skip chunk 0 if we need to rewrite it later)
	 * For single-chunk or live-load, write chunk 0 normally.
	 * For multi-chunk flash, write chunks 1, 2, 3, ... N.
	 */
	for (chunk_num = (rewrite_boot_tag ? 1 : 0); chunk_num < num_chunks; chunk_num++) {
		uint32_t chunk_offset;
		uint32_t chunk_len;
		bool is_last;

		chunk_offset = chunk_num * chunk_size;
		chunk_len = (chunk_offset + chunk_size > img_size) ?
		            (img_size - chunk_offset) : chunk_size;
		is_last = (chunk_num == num_chunks - 1);

		/*
		 * Live-load and flash both use AMI_CMD_OPCODE_PDI_DOWNLOAD (0xA).
		 * program_* flags in the GCQ payload select iAPC_PdiProgram() vs flash.
		 */
		ret = is_live_load ?
			ami_gcq_pdi_program_chunk(
				dev->gcq_ctx,
				&img_data[chunk_offset],
				chunk_len,
				chunk_num,
				chunk_size / 1024,
				is_last,
				boot_device,
				partition,
				pdi_md5,
				img_size,
				progress_handler,
				progress_handler ? &progress : NULL
			) :
			ami_gcq_pdi_download_chunk(
				dev->gcq_ctx,
				&img_data[chunk_offset],
				chunk_len,
				chunk_num,
				chunk_size / 1024,
				rewrite_boot_tag ? false : is_last,
				boot_device,
				partition,
				pdi_md5,
				img_size,
				progress_handler,
				progress_handler ? &progress : NULL
			);

		if (ret != 0) {
			ret = AMI_API_ERROR_M(AMI_ERROR_EIO,
				"Failed to %s chunk %u/%u",
				is_live_load ? "program" : "download",
				chunk_num + 1, num_chunks);
			goto cleanup;
		}

		bytes_sent += chunk_len;
		ret = AMI_STATUS_OK;
	}

	/* STEP 3: Rewrite chunk 0 with valid boot tag and data
	 * This makes the PDI bootable only after all chunks are successfully written.
	 */
	if (rewrite_boot_tag) {
		uint32_t chunk_len = (chunk_size > img_size) ? img_size : chunk_size;

		ret = ami_gcq_pdi_download_chunk(
			dev->gcq_ctx,
			img_data,             /* First chunk of original data */
			chunk_len,
			BOOT_TAG_CHUNK,       /* Chunk 0 */
			chunk_size / 1024,
			true,                 /* This is the LAST chunk */
			boot_device,
			partition,
			pdi_md5,
			img_size,
			progress_handler,
			progress_handler ? &progress : NULL
		);

		if (ret != 0) {
			ret = AMI_API_ERROR_M(AMI_ERROR_EIO,
				"Failed to rewrite boot tag chunk");
			goto cleanup;
		}

		bytes_sent += chunk_len;
		ret = AMI_STATUS_OK;
	}

	if (ret == AMI_STATUS_OK)
		ami_gcq_flush_pending_responses(dev->gcq_ctx);

cleanup:
	if (dev->gcq_ctx) {
		gcq_userspace_pause_heartbeat(dev->gcq_ctx, false);

		/* Aggressive cleanup after flash to ensure clean state for subsequent BOOT_SEL */
		ami_gcq_flush_pending_responses(dev->gcq_ctx);
		gcq_userspace_cq_drain(dev->gcq_ctx, NULL);
		ami_msleep(100);  /* Let AMC finish flash write operations */
		gcq_userspace_cq_drain(dev->gcq_ctx, NULL);  /* Drain again */

		fprintf(stderr, "[FLASH] Cleanup complete - GCQ state reset\n");
	}
	free(img_data);
	return ret;
}

/**
 * do_image_download() - Perform an image download operation.
 * @dev: Device handle.
 * @path: Path to image file.
 * @boot_device: Target boot device.
 * @partition: Partition number to program.
 * @progress_handler: Progress handler callback (optional).
 *
 * Return: AMI_STATUS_OK or AMI_STATUS_ERROR
 */
static int do_image_download(ami_device *dev, const char *path, uint8_t boot_device, uint32_t partition,
	ami_event_handler progress_handler)
{
	if (!dev || !path)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	/* VFIO: always reopen GCQ so a prior partial program or FPT probe cannot leave a bad session.
	 * The reopened device gets a fresh gcq_ctx (per-context CID/pending state), so there is no
	 * global session to reset here. */
	ami_close_cdev(dev);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR;  /* last error is set by ami_open_cdev */

	return do_image_download_gcq(dev, path, boot_device, partition, progress_handler);
}

/*****************************************************************************/
/* Public API function definitions                                           */
/*****************************************************************************/

/*
 * Program a pdi bitstream onto a device.
 */
int ami_prog_download_pdi(ami_device *dev, const char *path, uint8_t boot_device,
	uint32_t partition, ami_event_handler progress_handler)
{
	if (!dev || !path || (partition == AMI_IOC_FPT_UPDATE_MAGIC))
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	return do_image_download(
		dev,
		path,
		boot_device,
		partition,
		progress_handler
	);
}

/*
 * Update the device FPT.
 */
int ami_prog_update_fpt(ami_device *dev, const char *path, uint8_t boot_device,
	ami_event_handler progress_handler)
{
	if (!dev || !path)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	return do_image_download(
		dev,
		path,
		boot_device,
		AMI_IOC_FPT_UPDATE_MAGIC,
		progress_handler
	);
}

/*
 * Set the device boot partition.
 *
 * FW triggers a full system POR via XilPM after selecting the partition.
 * The POR brings down the PCIe link, so every PF on the slot must be
 * removed from the PCI tree before the link goes down.  Sequence:
 *   1. Issue the ioctl (tells FW to select partition + trigger POR)
 *   2. Release our device handle (closes cdev fd)
 *   3. Remove all sibling PFs from the PCI tree via sysfs
 *   4. Wait for POR to complete
 *   5. Rescan the PCI bus
 * VFIO userspace sequence (ami_vfio — kernel re-probes on rescan):
 *   BOOT_SEL → close → pci_remove → wait → rescan → find /dev/uio0
 *
 * VFIO userspace (ZeBu — recover_vfio_device.sh model, no pci_remove):
 *   BOOT_SEL → VFIO_DEVICE_RESET → close → unbind vfio-pci → wait link/POR →
 *   rescan → bind group → find + GCQ open
 */
#define POR_AMC_DELAY_MS		(1200)   /* Wait after BOOT_SEL before recovery */
#define POR_RESCAN_DELAY_MS		(15000)  /* Wait for OSPI boot after PCI remove (cold path) */
#define POR_POST_RESCAN_MS		(5000)   /* Wait after PCI rescan */
#define POR_VFIO_OSPI_WAIT_MS		(30000)  /* Wait for OSPI boot and firmware init - increased for slower cards */
#define POR_VFIO_LINK_POLL_MS		(5000)   /* Wait for link down - warm reboot may not drop link */
#define POR_MAX_BOOT_RETRIES		(12)
#define POR_VFIO_MAX_BOOT_RETRIES	(24)
#define POR_RETRY_DELAY_MS		(15000)  /* Wait between retries - give firmware time to fully initialize */
#define POR_MAX_IOMMU_GROUP_DEVS	(16)

static void pci_bdf_sysfs_str(uint16_t bdf, char *buf, size_t buflen)
{
	snprintf(buf, buflen, "0000:%02x:%02x.%x",
		AMI_PCI_BUS(bdf), AMI_PCI_DEV(bdf), AMI_PCI_FUNC(bdf));
}

static bool pci_sysfs_present(uint8_t bus, uint8_t dev, uint8_t func)
{
	char path[64];

	snprintf(path, sizeof(path),
		"/sys/bus/pci/devices/0000:%02x:%02x.%x", bus, dev, func);
	return access(path, F_OK) == 0;
}

static bool pci_sysfs_present_bdf(uint16_t bdf)
{
	return pci_sysfs_present(AMI_PCI_BUS(bdf), AMI_PCI_DEV(bdf),
		AMI_PCI_FUNC(bdf));
}

/*
 * Config-space vendor 0x0000 / 0xffff while the sysfs node still exists
 * means the link is down (ZeBu may keep the node visible).
 */
static bool pci_vendor_valid(uint16_t bdf)
{
	char path[128];
	char bdf_str[32];
	FILE *fp;
	unsigned int ven = 0;

	pci_bdf_sysfs_str(bdf, bdf_str, sizeof(bdf_str));
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/vendor", bdf_str);
	if (access(path, F_OK) != 0)
		return false;

	fp = fopen(path, "r");
	if (!fp)
		return false;
	if (fscanf(fp, "0x%x", &ven) != 1) {
		fclose(fp);
		return false;
	}
	fclose(fp);

	return ven != 0 && ven != 0xffff;
}

static bool pci_read_config16(uint16_t bdf, off_t offset, uint16_t *out)
{
	char bdf_str[32];
	char path[128];
	int fd;
	uint16_t val;

	if (!out)
		return false;

	pci_bdf_sysfs_str(bdf, bdf_str, sizeof(bdf_str));
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/config", bdf_str);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return false;

	if (pread(fd, &val, sizeof(val), offset) != (ssize_t)sizeof(val)) {
		close(fd);
		return false;
	}
	close(fd);

	*out = le16toh(val);
	return true;
}

/*
 * Vendor ID alone is not enough on ZeBu: the endpoint can reappear in sysfs
 * with a valid vendor while PCI COMMAND is still 0xffff and AMC BAR is dead.
 */
static bool __attribute__((unused)) pci_endpoint_config_ready(uint16_t bdf)
{
	char path[128];
	char bdf_str[32];
	FILE *fp;
	unsigned int dev_id = 0;
	uint16_t cmd = 0;

	if (!pci_vendor_valid(bdf))
		return false;

	pci_bdf_sysfs_str(bdf, bdf_str, sizeof(bdf_str));
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/device", bdf_str);
	fp = fopen(path, "r");
	if (!fp)
		return false;
	if (fscanf(fp, "0x%x", &dev_id) != 1) {
		fclose(fp);
		return false;
	}
	fclose(fp);

	if (dev_id == 0 || dev_id == 0xffff)
		return false;

	if (!pci_read_config16(bdf, 4, &cmd))
		return false;

	return cmd != 0 && cmd != 0xffff;
}

static int __attribute__((unused)) pci_collect_iommu_group(uint16_t bdf, char group_devs[][16],
	int max_devs)
{
	char bdf_str[32];
	char link_path[256];
	char group_link[256];
	char group_dir[128];
	char devices_dir[256];
	ssize_t link_len;
	DIR *dir = NULL;
	struct dirent *entry = NULL;
	int ndevs = 0;
	const char *group_id = NULL;

	if (max_devs <= 0)
		return 0;

	pci_bdf_sysfs_str(bdf, bdf_str, sizeof(bdf_str));
	snprintf(link_path, sizeof(link_path),
		"/sys/bus/pci/devices/%s/iommu_group", bdf_str);
	link_len = readlink(link_path, group_link, sizeof(group_link) - 1);
	if (link_len <= 0)
		return 0;

	group_link[link_len] = '\0';
	group_id = strrchr(group_link, '/');
	if (!group_id)
		return 0;
	group_id++;

	snprintf(group_dir, sizeof(group_dir), "/sys/kernel/iommu_groups/%s",
		group_id);
	snprintf(devices_dir, sizeof(devices_dir), "%s/devices", group_dir);

	dir = opendir(devices_dir);
	if (!dir)
		return 0;

	while ((entry = readdir(dir)) != NULL && ndevs < max_devs) {
		if (entry->d_name[0] == '.')
			continue;
		if (strlen(entry->d_name) >= 16)
			continue;
		memcpy(group_devs[ndevs], entry->d_name, strlen(entry->d_name) + 1);
		ndevs++;
	}

	closedir(dir);
	return ndevs;
}

static void pci_root_rescan(void)
{
	int fd = open("/sys/bus/pci/rescan", O_WRONLY);

	if (fd >= 0) {
		write(fd, "1", 1);
		close(fd);
	}
}

static bool pci_cache_parent_rescan_path(uint16_t bdf, char *out, size_t outlen)
{
	char bdf_str[32];
	char dev_path[PATH_MAX];
	char real_path[PATH_MAX];
	char parent_buf[PATH_MAX];
	size_t plen;

	if (!out || outlen < 8)
		return false;

	out[0] = '\0';
	pci_bdf_sysfs_str(bdf, bdf_str, sizeof(bdf_str));
	snprintf(dev_path, sizeof(dev_path),
		"/sys/bus/pci/devices/%s", bdf_str);
	if (!realpath(dev_path, real_path))
		return false;

	snprintf(parent_buf, sizeof(parent_buf), "%s", real_path);
	if (!dirname(parent_buf))
		return false;

	plen = strlen(parent_buf);
	if (plen + 8 >= outlen)
		return false;

	memcpy(out, parent_buf, plen);
	memcpy(out + plen, "/rescan", 8);
	return true;
}

static void pci_trigger_rescan_path(const char *rescan_path)
{
	int fd;

	if (!rescan_path || !rescan_path[0])
		return;

	fd = open(rescan_path, O_WRONLY);
	if (fd >= 0) {
		write(fd, "1", 1);
		close(fd);
		fprintf(stderr, "[DEVICE_BOOT] Bridge rescan: %s\n", rescan_path);
	}
}

static int pci_bind_vfio_bdf_sysfs(const char *bdf)
{
	char dev_path[128];
	char driver_link[256];
	char driver_target[256];
	char vendor_path[128];
	char device_path[128];
	char vendor[16] = { 0 };
	char device_id[16] = { 0 };
	char ven_hex[8] = { 0 };
	char dev_hex[8] = { 0 };
	FILE *fp = NULL;
	ssize_t link_len;
	int fd;

	snprintf(dev_path, sizeof(dev_path), "/sys/bus/pci/devices/%s", bdf);
	if (access(dev_path, F_OK) != 0)
		return AMI_STATUS_ERROR;

	snprintf(driver_link, sizeof(driver_link),
		"/sys/bus/pci/devices/%s/driver", bdf);
	link_len = readlink(driver_link, driver_target,
		sizeof(driver_target) - 1);
	if (link_len > 0) {
		driver_target[link_len] = '\0';
		if (strstr(driver_target, "vfio-pci") != NULL)
			return AMI_STATUS_OK;
	}

	snprintf(vendor_path, sizeof(vendor_path),
		"/sys/bus/pci/devices/%s/vendor", bdf);
	snprintf(device_path, sizeof(device_path),
		"/sys/bus/pci/devices/%s/device", bdf);

	fp = fopen(vendor_path, "r");
	if (!fp)
		return AMI_STATUS_ERROR;
	if (!fgets(vendor, sizeof(vendor), fp)) {
		fclose(fp);
		return AMI_STATUS_ERROR;
	}
	fclose(fp);

	fp = fopen(device_path, "r");
	if (!fp)
		return AMI_STATUS_ERROR;
	if (!fgets(device_id, sizeof(device_id), fp)) {
		fclose(fp);
		return AMI_STATUS_ERROR;
	}
	fclose(fp);

	snprintf(ven_hex, sizeof(ven_hex), "%s", vendor + 2);
	snprintf(dev_hex, sizeof(dev_hex), "%s", device_id + 2);
	ven_hex[strcspn(ven_hex, "\n")] = '\0';
	dev_hex[strcspn(dev_hex, "\n")] = '\0';

	fd = open("/sys/bus/pci/drivers/vfio-pci/new_id", O_WRONLY);
	if (fd >= 0) {
		char new_id[32];

		snprintf(new_id, sizeof(new_id), "%s %s", ven_hex, dev_hex);
		write(fd, new_id, strlen(new_id));
		close(fd);
	}

	fd = open("/sys/bus/pci/drivers/vfio-pci/bind", O_WRONLY);
	if (fd < 0)
		return AMI_STATUS_ERROR;

	if (write(fd, bdf, strlen(bdf)) < 0) {
		close(fd);
		link_len = readlink(driver_link, driver_target,
			sizeof(driver_target) - 1);
		if (link_len > 0) {
			driver_target[link_len] = '\0';
			if (strstr(driver_target, "vfio-pci") != NULL)
				return AMI_STATUS_OK;
		}
		return AMI_STATUS_ERROR;
	}

	close(fd);
	fprintf(stderr, "[DEVICE_BOOT] Bound %s to vfio-pci\n", bdf);
	return AMI_STATUS_OK;
}

static int __attribute__((unused)) pci_bind_vfio_pf(uint8_t bus, uint8_t dev, uint8_t func)
{
	char bdf[32];

	snprintf(bdf, sizeof(bdf), "0000:%02x:%02x.%x", bus, dev, func);
	return pci_bind_vfio_bdf_sysfs(bdf);
}

static void __attribute__((unused)) pci_bind_vfio_group_list(char group_devs[][16], int ndevs)
{
	int i;

	for (i = 0; i < ndevs; i++) {
		if (pci_bind_vfio_bdf_sysfs(group_devs[i]) != AMI_STATUS_OK) {
			fprintf(stderr,
				"[DEVICE_BOOT] WARNING: could not bind %s to vfio-pci\n",
				group_devs[i]);
		}
	}
}

static void pci_unbind_bdf_sysfs(const char *bdf)
{
	char driver_link[256];
	char driver_target[256];
	char driver_name[64];
	char unbind_path[320];
	const char *drv;
	ssize_t link_len;
	size_t dlen;
	int fd;

	snprintf(driver_link, sizeof(driver_link),
		"/sys/bus/pci/devices/%s/driver", bdf);
	link_len = readlink(driver_link, driver_target,
		sizeof(driver_target) - 1);
	if (link_len <= 0)
		return;

	driver_target[link_len] = '\0';
	if (!strstr(driver_target, "vfio-pci"))
		return;

	drv = strrchr(driver_target, '/');
	drv = drv ? drv + 1 : driver_target;
	dlen = strlen(drv);
	if (dlen >= sizeof(driver_name))
		return;

	memcpy(driver_name, drv, dlen + 1);
	snprintf(unbind_path, sizeof(unbind_path),
		"/sys/bus/pci/drivers/%s/unbind", driver_name);

	fd = open(unbind_path, O_WRONLY);
	if (fd >= 0) {
		write(fd, bdf, strlen(bdf));
		close(fd);
		fprintf(stderr,
			"[DEVICE_BOOT] Unbound %s from vfio-pci\n", bdf);
	}
}

static void __attribute__((unused)) pci_unbind_vfio_group_list(char group_devs[][16], int ndevs)
{
	int i;

	for (i = 0; i < ndevs; i++)
		pci_unbind_bdf_sysfs(group_devs[i]);
}

static void pci_remove_all_pf(uint8_t bus, uint8_t dev_num)
{
	DIR *pci_dir = NULL;
	struct dirent *entry = NULL;
	unsigned int domain, bus_scan, dev_scan, fn_scan;
	char path[256];
	int fd;

	pci_dir = opendir("/sys/bus/pci/devices");
	if (!pci_dir)
		return;

	while ((entry = readdir(pci_dir)) != NULL) {
		if (sscanf(entry->d_name, "%x:%x:%x.%x",
				&domain, &bus_scan, &dev_scan, &fn_scan) != 4)
			continue;

		if (domain != 0 || bus_scan != bus || dev_scan != dev_num)
			continue;

		snprintf(path, sizeof(path),
			"/sys/bus/pci/devices/%04x:%02x:%02x.%x/remove",
			domain, bus_scan, dev_scan, fn_scan);

		fd = open(path, O_WRONLY);
		if (fd >= 0) {
			write(fd, "1", 1);
			close(fd);
		}
	}

	closedir(pci_dir);
}

/*
 * Release any existing handle, reset GCQ session state, optionally wait for
 * AMC to quiesce after flash, then open with a full AMC-ready wait (not
 * quick_open).  Matches the device_boot refresh path used before BOOT_SEL.
 */
int ami_prog_prepare_device_boot(struct ami_device **dev, uint16_t bdf,
	unsigned int settle_ms)
{
	int ret;
	int retry;
	const int MAX_RETRIES = 3;

	if (!dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (*dev)
		ami_dev_delete(dev);

	/* No global GCQ session to reset — each ami_dev_open_bdf() below creates a
	 * fresh gcq_ctx with its own CID/pending state. */

	if (settle_ms > 0) {
		fprintf(stderr,
			"[DEVICE_BOOT] Waiting %u ms for AMC to settle after flash...\n",
			settle_ms);
		ami_msleep((long)settle_ms);
	}

	/* Retry device open with validation - after flash, AMC may need extra time */
	for (retry = 0; retry < MAX_RETRIES; retry++) {
		ret = ami_dev_open_bdf(dev, bdf, true);

		if (ret != AMI_STATUS_OK) {
			if (retry < MAX_RETRIES - 1) {
				fprintf(stderr,
					"[DEVICE_BOOT] GCQ open failed (attempt %d/%d), retrying...\n",
					retry + 1, MAX_RETRIES);
				ami_msleep(2000);
				continue;
			}
			return ret;
		}

		/* Validate GCQ is truly ready by draining any stale completions */
		if (*dev && (*dev)->gcq_ctx) {
			ami_gcq_purge_cq_completions((*dev)->gcq_ctx);
			ami_msleep(200);  /* Brief settle after purge */

			/* Verify GCQ is responsive with a lightweight health check */
			uint8_t major = 0, minor = 0, patch = 0;

			if (ami_gcq_send_identify((*dev)->gcq_ctx, &major, &minor, &patch) == 0) {
				fprintf(stderr,
					"[DEVICE_BOOT] GCQ validated - AMC v%u.%u.%u ready\n",
					major, minor, patch);
				return AMI_STATUS_OK;
			}

			/* IDENTIFY failed - AMC may still be processing flash */
			fprintf(stderr,
				"[DEVICE_BOOT] AMC not responsive (attempt %d/%d)\n",
				retry + 1, MAX_RETRIES);
			ami_dev_delete(dev);

			if (retry < MAX_RETRIES - 1) {
				ami_msleep(3000);  /* Longer wait between retries */
			}
		} else {
			/* Device opened but no GCQ context - shouldn't happen */
			return AMI_STATUS_OK;
		}
	}

	return AMI_API_ERROR(AMI_ERROR_EIO);
}

int ami_prog_device_boot(struct ami_device **dev, uint32_t partition)
{
	int ret = AMI_STATUS_ERROR;
	uint16_t saved_bdf = 0;
	char parent_rescan[PATH_MAX];
	bool vfio_backend;

	if (!dev || !(*dev))
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	saved_bdf = (*dev)->bdf;

	/*
	 * BOOT_SEL must run on a full GCQ session (not quick_open enumeration).
	 * Callers should use ami_prog_prepare_device_boot() first; this guards
	 * legacy paths that still pass a quick_open handle.
	 */
	if ((*dev)->use_userspace_gcq &&
	    (!(*dev)->gcq_ctx || (*dev)->quick_open)) {
		ami_dev_delete(dev);
		if (ami_dev_open_bdf(dev, saved_bdf, true) != AMI_STATUS_OK)
			return AMI_STATUS_ERROR;
	} else if (ami_open_cdev(*dev) != AMI_STATUS_OK) {
		return AMI_STATUS_ERROR;
	}

	vfio_backend = (access("/dev/vfio/vfio", F_OK) == 0);

	parent_rescan[0] = '\0';
	(void)pci_cache_parent_rescan_path(saved_bdf, parent_rescan,
		sizeof(parent_rescan));

	/* Drop any orphaned responses from the PDI download before BOOT_SEL */
	ami_gcq_flush_pending_responses((*dev)->gcq_ctx);
	gcq_userspace_cq_drain((*dev)->gcq_ctx, NULL);
	ami_msleep(100);  /* Final settle before BOOT_SEL */

	ret = ami_gcq_device_boot((*dev)->gcq_ctx, partition);
	if (ret != 0) {
		return AMI_API_ERROR(AMI_ERROR_EIO);
	}

	/*
	 * Close GCQ immediately to stop background threads before PCIe link drops.
	 * TODO: This causes "[AMC] Boot logs sent OK" spam - root cause still unknown.
	 */
	if ((*dev)->gcq_ctx) {
		ami_gcq_reset_session((*dev)->gcq_ctx);
		gcq_userspace_close((*dev)->gcq_ctx);
		gcq_userspace_destroy_context((*dev)->gcq_ctx);
		(*dev)->gcq_ctx = NULL;
	}
	gcq_userspace_deinit();

	/* Wait for firmware reboot */
	ami_msleep(POR_AMC_DELAY_MS);

	/* Free device struct */
	free(*dev);
	*dev = NULL;

	if (vfio_backend) {
		unsigned int elapsed;
		unsigned int i;

		/* Remove device from PCI tree to reset PCIe endpoint state */
		pci_remove_all_pf(AMI_PCI_BUS(saved_bdf), AMI_PCI_DEV(saved_bdf));

		elapsed = 0;
		/* Wait for link down (may not happen on warm reboot) */
		while (elapsed < POR_VFIO_LINK_POLL_MS) {
			if (!pci_sysfs_present_bdf(saved_bdf) ||
			    !pci_vendor_valid(saved_bdf)) {
				break;
			}
			ami_msleep(500);
			elapsed += 500;
		}

		/* Wait for OSPI boot and firmware initialization */
		fprintf(stderr, "Waiting for firmware to boot");
		fflush(stderr);
		for (i = 0; i < POR_VFIO_OSPI_WAIT_MS / 5000; i++) {
			ami_msleep(5000);
			fprintf(stderr, ".");
			fflush(stderr);
		}
		fprintf(stderr, "\n");
	} else {
		pci_remove_all_pf(AMI_PCI_BUS(saved_bdf), AMI_PCI_DEV(saved_bdf));
		fprintf(stderr, "Waiting for firmware to boot");
		fflush(stderr);
		for (unsigned int i = 0; i < POR_RESCAN_DELAY_MS / 5000; i++) {
			ami_msleep(5000);
			fprintf(stderr, ".");
			fflush(stderr);
		}
		fprintf(stderr, "\n");
	}

	if (vfio_backend) {
		(void)system("modprobe vfio-pci 2>/dev/null");
		(void)system("modprobe vfio_iommu_type1 2>/dev/null");
		pci_trigger_rescan_path(parent_rescan);
	}
	pci_root_rescan();
	ami_msleep(POR_POST_RESCAN_MS);

	/*
	 * Device boot completed. Manually rebind to vfio-pci in case udev rules
	 * didn't run in time and ami kernel driver claimed the device.
	 */
	if (vfio_backend) {
		char bdf_str[32];
		pci_bdf_sysfs_str(saved_bdf, bdf_str, sizeof(bdf_str));
		pci_unbind_bdf_sysfs(bdf_str);
		ami_msleep(500);  /* Give device time to unbind */
		if (pci_bind_vfio_bdf_sysfs(bdf_str) != AMI_STATUS_OK) {
			fprintf(stderr,
				"[DEVICE_BOOT] WARNING: Failed to bind %s to vfio-pci after rescan. "
				"Device may be bound to ami kernel driver. Manually rebind with:\n"
				"  echo %s | sudo tee /sys/bus/pci/drivers/ami/unbind\n"
				"  echo %s | sudo tee /sys/bus/pci/drivers/vfio-pci/bind\n",
				bdf_str, bdf_str, bdf_str);
		}
	}
	fprintf(stderr, "\nDevice boot command completed.\n");
	fprintf(stderr, "Device successfully booted into partition %u.\n", partition);
	fprintf(stderr, "Verify device health with: ami_tool overview\n");

	return AMI_STATUS_OK;
}

/*
 * Copy a device partition.
 */
int ami_prog_copy_partition(ami_device *dev, uint32_t src_device, uint32_t src_part,
	uint32_t dest_device, uint32_t dest_part, ami_event_handler progress_handler)
{
	int ret = AMI_STATUS_ERROR;
	struct ami_fpt_partition src_partition_info = {0};

	if (!dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR; /* last error is set by ami_open_cdev */

	/* Get source partition size from FPT */
	ret = ami_prog_get_fpt_partition(dev, src_device, src_part, &src_partition_info);
	if (ret != AMI_STATUS_OK) {
		return AMI_API_ERROR(AMI_ERROR_EIO);
	}

	ret = ami_gcq_partition_copy(dev->gcq_ctx, src_device, src_part, dest_device, dest_part,
	                             src_partition_info.size);
	return (ret == 0) ? AMI_STATUS_OK : AMI_API_ERROR(AMI_ERROR_EIO);
}

/*
 * Get the FPT header.
 */
int ami_prog_get_fpt_header(ami_device *dev, uint8_t boot_device,
	struct ami_fpt_header *header)
{
	if (!dev || !header)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR; /* last error is set by ami_open_cdev */

	struct sdr_repo fpt_repo = {0};
	int ret_sdr;

	/* Fetch FPT SDR from firmware */
	ret_sdr = ami_gcq_get_sdr(dev->gcq_ctx, SDR_TYPE_FPT, &fpt_repo);
	if (ret_sdr != 0) {
		return AMI_API_ERROR(AMI_ERROR_EIO);
	}

	/* Copy the appropriate FPT header */
	struct fpt_header *src_hdr = (boot_device == AMI_BOOT_DEVICES_PRIMARY) ?
	                              &fpt_repo.fpt.hdr_primary :
	                              &fpt_repo.fpt.hdr_secondary;

	header->version = src_hdr->version;
	header->hdr_size = src_hdr->header_size;
	header->entry_size = src_hdr->entry_size;
	header->num_entries = src_hdr->num_entries;

	ami_free_sdr_repo(&fpt_repo);
	return AMI_STATUS_OK;
}

/*
 * Set FPT partition information.
 */
int ami_prog_set_fpt_partition(ami_device *dev, uint8_t boot_device,
	uint32_t num, struct ami_fpt_partition *partition)
{
	if (!dev || !partition)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR; /* last error is set by ami_open_cdev */

	struct amc_proxy_cmd_request req = {0};
	struct com_queue_entry resp;
	uint16_t cid = ami_gcq_allocate_cid(dev->gcq_ctx);
	uint32_t result;

	/* Build SET_FPT_PARTITION request */
	req.hdr.opcode = AMC_PROXY_CMD_OPCODE_FPT_FLAGS;  /* 0x6 */
	req.hdr.state = 1;
	req.hdr.cid = cid;
	req.hdr.count = sizeof(req.fpt_partition_payload);

	/* Fill FPT partition payload */
	req.fpt_partition_payload.req_type = 1;  /* Write */
	req.fpt_partition_payload.boot_device = boot_device;
	req.fpt_partition_payload.partition = num;
	req.fpt_partition_payload.type = partition->type;
	req.fpt_partition_payload.base_addr = partition->base_addr;
	req.fpt_partition_payload.size = partition->size;
	memcpy(req.fpt_partition_payload.pdi_md5, partition->pdi_md5, MD5_SIZE);
	req.fpt_partition_payload.pdi_size = partition->pdi_size;
	req.fpt_partition_payload.flags = partition->flags;

	/* Send command */
	result = gcq_userspace_write(dev->gcq_ctx, (const uint8_t *)&req, 512, 5000);
	if (result != GCQ_ERRORS_NONE) {
		ami_gcq_free_cid(cid);
		return AMI_API_ERROR(AMI_ERROR_EIO);
	}

	/* Wait for response */
	if (ami_gcq_wait_response(dev->gcq_ctx, cid, &resp, 10000) != 0) {
		ami_gcq_free_cid(cid);
		return AMI_API_ERROR(AMI_ERROR_EIO);
	}

	ami_gcq_free_cid(cid);

	/* Note: FPT flags operation returns rcode=1 for success (not 0) */
	if (resp.rcode == 0 || resp.rcode == 1) {
		return AMI_STATUS_OK;
	} else {
		fprintf(stderr, "[FPT_FLAGS_WR] ERROR: Firmware returned rcode=%u\n", resp.rcode);
		return AMI_API_ERROR(AMI_ERROR_EIO);
	}
}

/*
 * Set FPT partition flags only.
 */
int ami_prog_set_fpt_flags(ami_device *dev, uint8_t boot_device,
	uint32_t num, uint32_t flags)
{
	if (!dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR; /* last error is set by ami_open_cdev */

	struct amc_proxy_cmd_request req = {0};
	struct com_queue_entry resp;
	uint16_t cid = ami_gcq_allocate_cid(dev->gcq_ctx);
	uint32_t result;

	/* Build FPT FLAGS update request - minimal payload approach
	 * Only populate the essential fields needed for a flags-only update.
	 * Leave type, base_addr, size, MD5, and pdi_size as zero to avoid
	 * validation conflicts with existing partition data in firmware.
	 */
	req.hdr.opcode = AMC_PROXY_CMD_OPCODE_FPT_FLAGS;  /* 0x6 */
	req.hdr.state = 1;
	req.hdr.cid = cid;
	req.hdr.count = sizeof(req.fpt_partition_payload);

	/* Fill minimal FPT partition payload - flags update only */
	req.fpt_partition_payload.req_type = 1;  /* Write */
	req.fpt_partition_payload.boot_device = boot_device;
	req.fpt_partition_payload.partition = num;
	req.fpt_partition_payload.type = 0;       /* Zero - not updating type */
	req.fpt_partition_payload.base_addr = 0;  /* Zero - not updating address */
	req.fpt_partition_payload.size = 0;       /* Zero - not updating size */
	memset(req.fpt_partition_payload.pdi_md5, 0, MD5_SIZE); /* Zero - not updating MD5 */
	req.fpt_partition_payload.pdi_size = 0;   /* Zero - not updating PDI size */
	req.fpt_partition_payload.flags = flags;  /* Only this field matters */

	/* Send command */
	result = gcq_userspace_write(dev->gcq_ctx, (const uint8_t *)&req, 512, 5000);
	if (result != GCQ_ERRORS_NONE) {
		ami_gcq_free_cid(cid);
		return AMI_API_ERROR(AMI_ERROR_EIO);
	}

	/* Wait for response */
	if (ami_gcq_wait_response(dev->gcq_ctx, cid, &resp, 10000) != 0) {
		ami_gcq_free_cid(cid);
		return AMI_API_ERROR(AMI_ERROR_EIO);
	}

	ami_gcq_free_cid(cid);

	/* Check response code
	 * rcode=0: SUCCESS
	 * rcode=1: Alternative success code (some firmware versions)
	 */
	if (resp.rcode == 0 || resp.rcode == 1) {
		return AMI_STATUS_OK;
	} else {
		fprintf(stderr, "[FPT_FLAGS_WR] ERROR: Firmware returned rcode=%u\n", resp.rcode);
		return AMI_API_ERROR(AMI_ERROR_EIO);
	}
}

/*
 * Get FPT partition information.
 */
int ami_prog_get_fpt_partition(ami_device *dev, uint8_t boot_device,
	uint32_t num, struct ami_fpt_partition *partition)
{
	if (!dev || !partition)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR; /* last error is set by ami_open_cdev */

	struct sdr_repo fpt_repo = {0};
	int ret_sdr;

	/* Fetch FPT SDR from firmware */
	ret_sdr = ami_gcq_get_sdr(dev->gcq_ctx, (uint8_t)SDR_TYPE_FPT, &fpt_repo);
	if (ret_sdr != 0) {
		return AMI_API_ERROR(AMI_ERROR_EIO);
	}

	/* Get partition entries based on boot device */
	struct fpt_partition_entry *partitions = (boot_device == AMI_BOOT_DEVICES_PRIMARY) ?
	                                          fpt_repo.fpt.partitions_primary :
	                                          fpt_repo.fpt.partitions_secondary;
	uint8_t num_entries = (boot_device == AMI_BOOT_DEVICES_PRIMARY) ?
	                       fpt_repo.fpt.hdr_primary.num_entries :
	                       fpt_repo.fpt.hdr_secondary.num_entries;

	if (!partitions || num >= num_entries) {
		ami_free_sdr_repo(&fpt_repo);
		return AMI_API_ERROR(AMI_ERROR_EINVAL);
	}

	/* Copy partition data */
	partition->type = (enum ami_fpt_type)partitions[num].type;
	partition->base_addr = partitions[num].base_addr;
	partition->size = partitions[num].size;
	partition->pdi_size = partitions[num].pdi_size;
	partition->flags = partitions[num].flags;
	memcpy(&partition->pdi_md5, partitions[num].md5, sizeof(partition->pdi_md5));

	ami_free_sdr_repo(&fpt_repo);
	return AMI_STATUS_OK;
}

/*
 * Program a pdi bitstream onto a device.
 */
int ami_prog_pdi(ami_device *dev, const char *path,
	ami_event_handler progress_handler)
{
	if (!dev || !path)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	return do_image_download(
		dev,
		path,
		0/*boot_device*/,
		AMI_IOC_PDI_PROGRAM_MAGIC,
		progress_handler
	);
}

/*
 * Live-load an APU PDI image onto a device.
 */
int ami_prog_pdi_apu(ami_device *dev, const char *path,
	ami_event_handler progress_handler)
{
	if (!dev || !path)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	return do_image_download(
		dev,
		path,
		0/*boot_device*/,
		AMI_IOC_PDI_APU_PROGRAM_MAGIC,
		progress_handler
	);
}

/*
 * Live-load an RPU PDI image onto a device.
 */
int ami_prog_pdi_rpu(ami_device *dev, const char *path,
	ami_event_handler progress_handler)
{
	if (!dev || !path)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	return do_image_download(
		dev,
		path,
		0/*boot_device*/,
		AMI_IOC_PDI_RPU_PROGRAM_MAGIC,
		progress_handler
	);
}
