// SPDX-License-Identifier: GPL-2.0-only
/*
 * ami_program.c - This file contains the implementation of device programming logic.
 *
 * Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
 */

/* Standard includes */
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* Public API includes */
#include "ami_program.h"

/* Private API includes */
#include "ami_ioctl.h"
#include "ami_internal.h"
#include "ami_device_internal.h"
#include "md5.h"

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
	uint8_t *img_data = NULL;
	uint32_t img_size = 0;
	int ret = AMI_STATUS_ERROR;
	struct ami_ioc_data_payload payload = { 0 };

	/* For progress tracking */
	struct ami_event_data evt_data = { 0 };
	struct ami_pdi_progress progress = { 0 };

	if (!dev || !path)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR;  /* last error is set by ami_open_cdev */

	if (read_file(path, &img_data, &img_size) == AMI_STATUS_OK) {
		calculate_md5(img_data, img_size, payload.pdi_md5);
		payload.size = img_size;
		payload.addr = (unsigned long)(&img_data[0]);
		payload.pdi_size = img_size;
		payload.cap_override = dev->cap_override;
		payload.boot_device = boot_device;
		payload.partition = partition;
		payload.efd = AMI_INVALID_FD;

		if (progress_handler) {
			progress.bytes_to_write = img_size;

			if (ami_watch_driver_events(&evt_data, progress_handler, (void*)&progress) == AMI_STATUS_OK)
				payload.efd = evt_data.efd;
		}

		errno = 0;
		if (ioctl(dev->cdev, AMI_IOC_DOWNLOAD_PDI, &payload) == AMI_LINUX_STATUS_ERROR)
			ret = AMI_API_ERROR_M(
				AMI_ERROR_EIO,
				"errno %d (%s)",
				errno,
				strerror(errno)
			);
		else
			ret = AMI_STATUS_OK;

		free(img_data);  /* allocated by `read_file` */

		if (progress_handler && (evt_data.efd != AMI_INVALID_FD))
			ami_stop_watching_events(&evt_data);
	}

	return ret;
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
 *   6. Re-acquire the device handle
 */
#define POR_RESCAN_DELAY_MS	(15000)
#define POR_POST_RESCAN_MS	(5000)

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

int ami_prog_device_boot(struct ami_device **dev, uint32_t partition)
{
	int ret = AMI_STATUS_ERROR;
	struct ami_ioc_data_payload payload = { 0 };
	uint16_t saved_bdf = 0;
	int cdev_fd = AMI_INVALID_FD;
	int rescan_fd = AMI_INVALID_FD;

	if (!dev || !(*dev))
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(*dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR;

	saved_bdf = (*dev)->bdf;
	cdev_fd = (*dev)->cdev;
	payload.partition = partition;

	errno = 0;
	if (ioctl(cdev_fd, AMI_IOC_DEVICE_BOOT, &payload) == AMI_LINUX_STATUS_ERROR) {
		return AMI_API_ERROR_M(
			AMI_ERROR_EIO,
			"errno %d (%s)",
			errno,
			strerror(errno)
		);
	} else {
		/*
		 * Ioctl succeeded — POR is imminent.
		 * Release our device handle, then remove every PF on the slot
		 * so no driver is still bound when the PCIe link drops.
		 */
		ami_dev_delete(dev);
		pci_remove_all_pf(AMI_PCI_BUS(saved_bdf), AMI_PCI_DEV(saved_bdf));

		ami_msleep(POR_RESCAN_DELAY_MS);

		/* Rescan the PCI bus so the host re-enumerates the device. */
		rescan_fd = open("/sys/bus/pci/rescan", O_WRONLY);
		if (rescan_fd >= 0) {
			write(rescan_fd, "1", 1);
			close(rescan_fd);
			ami_msleep(POR_POST_RESCAN_MS);
		}

		/* Re-acquire the device handle by BDF. */
		ret = ami_dev_find_next(dev, AMI_PCI_BUS(saved_bdf),
			AMI_PCI_DEV(saved_bdf), AMI_PCI_FUNC(saved_bdf), NULL);
	}
	return ret;
}

/*
 * Copy a device partition.
 */
int ami_prog_copy_partition(ami_device *dev, uint32_t src_device, uint32_t src_part,
	uint32_t dest_device, uint32_t dest_part, ami_event_handler progress_handler)
{
	int ret = AMI_STATUS_ERROR;
	struct ami_ioc_data_payload payload = { 0 };

	/* For progress tracking. */
	struct ami_event_data evt_data = { 0 };

	if (!dev)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR; /* last error is set by ami_open_cdev */

	payload.src_device = src_device;
	payload.src_part = src_part;
	payload.dest_device = dest_device;
	payload.dest_part = dest_part;

	/* NOTE: Progress tracking is currently not implemented driver side. */
	if (progress_handler)
		ami_watch_driver_events(
			&evt_data,
			progress_handler,
			NULL
		);

	if (ioctl(dev->cdev, AMI_IOC_COPY_PARTITION, &payload) == AMI_LINUX_STATUS_ERROR)
		ret = AMI_API_ERROR_M(
			AMI_ERROR_EIO,
			"errno %d (%s)",
			errno,
			strerror(errno)
		);
	else
		ret = AMI_STATUS_OK;

	if (progress_handler && (evt_data.efd != AMI_INVALID_FD))
		ami_stop_watching_events(&evt_data);

	return ret;
}

/*
 * Get the FPT header.
 */
int ami_prog_get_fpt_header(ami_device *dev, uint8_t boot_device,
	struct ami_fpt_header *header)
{
	int ret = AMI_STATUS_ERROR;
	struct ami_ioc_fpt_hdr_value data = { 0 };

	if (!dev || !header)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR; /* last error is set by ami_open_cdev */

	data.boot_device = boot_device;

	errno = 0;
	if (ioctl(dev->cdev, AMI_IOC_GET_FPT_HDR, &data) == AMI_LINUX_STATUS_ERROR) {
		ret = AMI_API_ERROR_M(
			AMI_ERROR_EIO,
			"errno %d (%s)",
			errno,
			strerror(errno)
		);
	} else {
		ret = AMI_STATUS_OK;
		header->version = data.version;
		header->hdr_size = data.hdr_size;
		header->entry_size = data.entry_size;
		header->num_entries = data.num_entries;
	}

	return ret;
}

/*
 * Set FPT partition information.
 */
int ami_prog_set_fpt_partition(ami_device *dev, uint8_t boot_device,
	uint32_t num, struct ami_fpt_partition *partition)
{
	int ret = AMI_STATUS_ERROR;
	struct ami_ioc_fpt_partition_value data = { 0 };

	if (!dev || !partition)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR; /* last error is set by ami_open_cdev */

	data.boot_device = boot_device;
	data.partition = num;
	data.type = partition->type;
	data.base_addr = partition->base_addr;
	data.size = partition->size;
	/* Make a copy of the response before removing from the list */
	memcpy(&data.pdi_md5, &partition->pdi_md5,
		sizeof(data.pdi_md5));
	data.pdi_size = partition->pdi_size;
	data.flags = partition->flags;
	errno = 0;
	if (ioctl(dev->cdev, AMI_IOC_SET_FPT_PARTITION, &data) == AMI_LINUX_STATUS_ERROR) {
		ret = AMI_API_ERROR_M(
			AMI_ERROR_EIO,
			"errno %d (%s)",
			errno,
			strerror(errno)
		);
	} else {
		ret = AMI_STATUS_OK;
	}

	return ret;
}

/*
 * Get FPT partition information.
 */
int ami_prog_get_fpt_partition(ami_device *dev, uint8_t boot_device,
	uint32_t num, struct ami_fpt_partition *partition)
{
	int ret = AMI_STATUS_ERROR;
	struct ami_ioc_fpt_partition_value data = { 0 };

	if (!dev || !partition)
		return AMI_API_ERROR(AMI_ERROR_EINVAL);

	if (ami_open_cdev(dev) != AMI_STATUS_OK)
		return AMI_STATUS_ERROR; /* last error is set by ami_open_cdev */

	data.boot_device = boot_device;
	data.partition = num;

	errno = 0;
	if (ioctl(dev->cdev, AMI_IOC_GET_FPT_PARTITION, &data) == AMI_LINUX_STATUS_ERROR) {
		ret = AMI_API_ERROR_M(
			AMI_ERROR_EIO,
			"errno %d (%s)",
			errno,
			strerror(errno)
		);
	} else {
		ret = AMI_STATUS_OK;
		partition->type = (enum ami_fpt_type)data.type;
		partition->base_addr = data.base_addr;
		partition->size = data.size;
		/* Make a copy of the response before removing from the list */
		memcpy(&partition->pdi_md5, &data.pdi_md5,
			sizeof(data.pdi_md5));
		partition->pdi_size = data.pdi_size;
		partition->flags = data.flags;
	}

	return ret;
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
