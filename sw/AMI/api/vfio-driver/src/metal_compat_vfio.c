/**
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * metal_compat_vfio.c - Linux VFIO backend for metal_io_region
 *
 * Implements metal_vfio_device_open() and metal_vfio_device_close() using
 * Linux VFIO ioctls.  This file is only compiled when USE_LIBMETAL is NOT
 * defined; when the real libmetal is in use its own VFIO backend is used
 * instead and these symbols are not needed.
 *
 * Porting notes
 * -------------
 * The only Linux-specific parts of this file are the VFIO ioctl sequence
 * and the /sys/bus/pci sysfs reads.  On baremetal,
 * FreeRTOS, Zephyr replace this file with a platform-specific
 * implementation that populates metal_io_region.virt / .size and returns 0,
 * using whatever mechanism is appropriate (e.g. direct physical address
 * mapping on baremetal).  All callers above this layer are agnostic to the
 * mechanism.
 *
 * @file metal_compat_vfio.c
 */

#ifndef USE_LIBMETAL   /* whole file is a no-op when real libmetal is present */

#include "metal_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/vfio.h>

/* --------------------------------------------------------------------------
 * Defines
 * -------------------------------------------------------------------------- */

#define SYSFS_PCI_DEVICES       "/sys/bus/pci/devices"
#define SYSFS_VFIO_PCI_NEW_ID   "/sys/bus/pci/drivers/vfio-pci/new_id"
#define DEV_VFIO                "/dev/vfio/vfio"
#define DEV_VFIO_GROUP          "/dev/vfio"

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

/**
 * vfio_find_group() - Resolve the VFIO IOMMU group number for a PCI BDF.
 * @bdf: PCI BDF string, e.g. "0000:e2:00.0".
 *
 * Reads the symlink:
 *   /sys/bus/pci/devices/<bdf>/iommu_group
 *   → "../../../../kernel/iommu_groups/<N>"
 * and extracts the trailing integer <N>.
 *
 * Return: Group number (>= 0) on success, -1 on error.
 */
static int vfio_find_group(const char *bdf)
{
	char sysfs_path[256];
	char link_target[256];
	ssize_t len;
	char *slash;
	int group;

	snprintf(sysfs_path, sizeof(sysfs_path),
		 SYSFS_PCI_DEVICES "/%s/iommu_group", bdf);

	len = readlink(sysfs_path, link_target, sizeof(link_target) - 1);
	if (len < 0) {
		fprintf(stderr,
			"[VFIO] readlink(%s) failed: %s\n"
			"[VFIO] Is the device bound to vfio-pci? Try:\n"
			"[VFIO]   echo \"%s\" > " SYSFS_VFIO_PCI_NEW_ID "\n",
			sysfs_path, strerror(errno), bdf);
		return -1;
	}
	link_target[len] = '\0';

	/* link ends with "/<group_num>" */
	slash = strrchr(link_target, '/');
	if (!slash || sscanf(slash + 1, "%d", &group) != 1) {
		fprintf(stderr, "[VFIO] Cannot parse group number from: %s\n",
			link_target);
		return -1;
	}

	return group;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

int metal_vfio_device_open(const char *bdf,
			   struct metal_io_region *bar0_out,
			   int *device_fd_out,
			   int *group_fd_out,
			   int *container_fd_out)
{
	int container_fd = -1;
	int group_fd     = -1;
	int device_fd    = -1;
	int group_id;
	char group_path[64];
	void *bar0_virt;

	struct vfio_group_status  group_status  = {
		.argsz = sizeof(group_status)
	};
	struct vfio_device_info   device_info   = {
		.argsz = sizeof(device_info)
	};
	struct vfio_region_info   region_info   = {
		.argsz = sizeof(region_info)
	};

	if (!bdf || !bar0_out || !device_fd_out ||
	    !group_fd_out || !container_fd_out) {
		fprintf(stderr, "[VFIO] metal_vfio_device_open: NULL argument\n");
		return -1;
	}

	/* ------------------------------------------------------------------ */
	/* Step 1: Resolve IOMMU group                                        */
	/* ------------------------------------------------------------------ */
	group_id = vfio_find_group(bdf);
	if (group_id < 0)
		return -1;

	/* ------------------------------------------------------------------ */
	/* Step 2: Open VFIO container                                         */
	/* ------------------------------------------------------------------ */
	container_fd = open(DEV_VFIO, O_RDWR);
	if (container_fd < 0) {
		fprintf(stderr, "[VFIO] Cannot open " DEV_VFIO ": %s\n"
			"[VFIO] Ensure vfio and vfio-pci kernel modules are loaded:\n"
			"[VFIO]   modprobe vfio vfio-pci\n",
			strerror(errno));
		return -1;
	}

	/* ------------------------------------------------------------------ */
	/* Step 3: Verify VFIO API version                                     */
	/* ------------------------------------------------------------------ */
	if (ioctl(container_fd, VFIO_GET_API_VERSION) != VFIO_API_VERSION) {
		fprintf(stderr, "[VFIO] Unexpected VFIO API version\n");
		goto err_container;
	}

	if (!ioctl(container_fd, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU)) {
		fprintf(stderr, "[VFIO] TYPE1_IOMMU not supported\n");
		goto err_container;
	}

	/* ------------------------------------------------------------------ */
	/* Step 4: Open VFIO group                                             */
	/* ------------------------------------------------------------------ */
	snprintf(group_path, sizeof(group_path), DEV_VFIO_GROUP "/%d", group_id);
	group_fd = open(group_path, O_RDWR);
	if (group_fd < 0) {
		fprintf(stderr, "[VFIO] Cannot open %s: %s\n",
			group_path, strerror(errno));
		goto err_container;
	}

	/* ------------------------------------------------------------------ */
	/* Step 5: Verify the group is viable                                  */
	/* (all functions in the IOMMU group must be bound to vfio-pci)        */
	/* ------------------------------------------------------------------ */
	if (ioctl(group_fd, VFIO_GROUP_GET_STATUS, &group_status)) {
		fprintf(stderr, "[VFIO] VFIO_GROUP_GET_STATUS failed: %s\n",
			strerror(errno));
		goto err_group;
	}

	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		fprintf(stderr,
			"[VFIO] Group %d is not viable — bind all sibling "
			"functions to vfio-pci before opening this device.\n",
			group_id);
		goto err_group;
	}

	/* ------------------------------------------------------------------ */
	/* Step 6: Associate the group with the container                      */
	/* ------------------------------------------------------------------ */
	if (ioctl(group_fd, VFIO_GROUP_SET_CONTAINER, &container_fd)) {
		fprintf(stderr, "[VFIO] VFIO_GROUP_SET_CONTAINER failed: %s\n",
			strerror(errno));
		goto err_group;
	}

	/* ------------------------------------------------------------------ */
	/* Step 7: Set IOMMU type on the container                             */
	/* ------------------------------------------------------------------ */
	if (ioctl(container_fd, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU)) {
		fprintf(stderr, "[VFIO] VFIO_SET_IOMMU(TYPE1) failed: %s\n",
			strerror(errno));
		goto err_group;
	}

	/* ------------------------------------------------------------------ */
	/* Step 8: Get the device file descriptor                              */
	/* ------------------------------------------------------------------ */
	device_fd = ioctl(group_fd, VFIO_GROUP_GET_DEVICE_FD, bdf);
	if (device_fd < 0) {
		fprintf(stderr,
			"[VFIO] VFIO_GROUP_GET_DEVICE_FD(%s) failed: %s\n",
			bdf, strerror(errno));
		goto err_group;
	}

	/* ------------------------------------------------------------------ */
	/* Step 8b: Enable Memory Space + Bus Master via PCI COMMAND register  */
	/*                                                                     */
	/* Devices with non-standard PCI headers (e.g. "Unknown header type") */
	/* may not have Memory Space Enable set by vfio-pci.  Without it all  */
	/* BAR0 accesses return PCIe UR → SIGBUS.  Access config space via    */
	/* the VFIO config region (index 7) and set bits 1 and 2 of COMMAND.  */
	/* ------------------------------------------------------------------ */
	{
		struct vfio_region_info cfg_info = { .argsz = sizeof(cfg_info) };
		cfg_info.index = VFIO_PCI_CONFIG_REGION_INDEX;

		if (ioctl(device_fd, VFIO_DEVICE_GET_REGION_INFO, &cfg_info) == 0) {
			uint16_t cmd = 0;
			off_t    cmd_off = (off_t)cfg_info.offset + 0x04; /* PCI_COMMAND */

			if (pread(device_fd, &cmd, sizeof(cmd), cmd_off) == sizeof(cmd)) {
				if (cmd != 0 && cmd != 0xffff) {
					cmd |= 0x0006; /* MSE | BME */
					if (pwrite(device_fd, &cmd, sizeof(cmd), cmd_off)
					    != sizeof(cmd)) {
						/* Failed to enable MSE/BME, but continue */
					}
				} else {
					fprintf(stderr,
						"[VFIO] PCI COMMAND invalid (0x%04x) — "
						"endpoint not ready for BAR access\n",
						cmd);
					goto err_device;
				}
			}
		}
	}

	/* ------------------------------------------------------------------ */
	/* Step 9: Query BAR0 region info (PCI region index 0 = BAR0)         */
	/* ------------------------------------------------------------------ */
	region_info.index = VFIO_PCI_BAR0_REGION_INDEX;
	if (ioctl(device_fd, VFIO_DEVICE_GET_REGION_INFO, &region_info)) {
		fprintf(stderr,
			"[VFIO] VFIO_DEVICE_GET_REGION_INFO(BAR0) failed: %s\n",
			strerror(errno));
		goto err_device;
	}

	if (!(region_info.flags & VFIO_REGION_INFO_FLAG_MMAP)) {
		fprintf(stderr, "[VFIO] BAR0 is not mmap-able\n");
		goto err_device;
	}

	/* ------------------------------------------------------------------ */
	/* Step 10: Map BAR0 into the process address space                    */
	/* ------------------------------------------------------------------ */
	bar0_virt = mmap(NULL, (size_t)region_info.size,
			 PROT_READ | PROT_WRITE, MAP_SHARED,
			 device_fd, (off_t)region_info.offset);
	if (bar0_virt == MAP_FAILED) {
		fprintf(stderr, "[VFIO] mmap BAR0 failed: %s\n",
			strerror(errno));
		goto err_device;
	}

	/* ------------------------------------------------------------------ */
	/* Success: populate outputs                                           */
	/* ------------------------------------------------------------------ */
	bar0_out->virt  = bar0_virt;
	bar0_out->size  = (size_t)region_info.size;

	*device_fd_out    = device_fd;
	*group_fd_out     = group_fd;
	*container_fd_out = container_fd;

	return 0;

	/* ------------------------------------------------------------------ */
	/* Error unwind                                                        */
	/* ------------------------------------------------------------------ */
err_device:
	close(device_fd);
err_group:
	close(group_fd);
err_container:
	close(container_fd);
	return -1;
}

void metal_vfio_device_close(struct metal_io_region *bar0,
			     int device_fd,
			     int group_fd,
			     int container_fd)
{
	if (bar0 && bar0->virt && bar0->virt != MAP_FAILED) {
		munmap(bar0->virt, bar0->size);
		bar0->virt = NULL;
		bar0->size = 0;
	}

	if (device_fd >= 0)
		close(device_fd);

	if (group_fd >= 0)
		close(group_fd);

	if (container_fd >= 0)
		close(container_fd);
}

#endif  /* !USE_LIBMETAL */
