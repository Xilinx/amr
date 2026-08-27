/**
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * metal_compat.h - Portable metal_io_region API
 *
 * Provides a source-compatible subset of the libmetal metal_io_region API.
 * The implementation comes from one of two sources:
 *
 *   USE_LIBMETAL defined   → include real libmetal headers; zero shim code
 *   USE_LIBMETAL not defined → minimal shim backed by Linux VFIO ioctls
 *
 * Callers use metal_io_read32 / metal_io_write32 / metal_io_virt throughout
 * and require no changes when switching between the two backends.
 *
 * For the VFIO backend, device lifecycle is managed by the helpers:
 *   metal_vfio_device_open()   - open, map BAR0, populate metal_io_region
 *   metal_vfio_device_close()  - unmap BAR0, close all VFIO fds
 *
 * @file metal_compat.h
 */

#ifndef _METAL_COMPAT_H_
#define _METAL_COMPAT_H_

#ifdef USE_LIBMETAL

/* -----------------------------------------------------------------------
 * Real libmetal path: include library headers and expose nothing extra.
 * ----------------------------------------------------------------------- */
#include <metal/device.h>
#include <metal/io.h>

/*
 * metal_vfio_device_open() / metal_vfio_device_close() are not needed when
 * using real libmetal — callers use metal_device_open() / metal_device_close()
 * from the library instead.
 */

#else  /* !USE_LIBMETAL */

/* -----------------------------------------------------------------------
 * Shim path: provide a minimal metal_io_region compatible with libmetal.
 * ----------------------------------------------------------------------- */

#include <stdint.h>
#include <stddef.h>

/**
 * struct metal_io_region - describes a mapped I/O region.
 *
 * Layout deliberately matches libmetal's metal_io_region so that a future
 * switch to real libmetal requires no changes to callers.
 */
struct metal_io_region {
	void   *virt;   /**< Virtual base address of the mapped region */
	size_t  size;   /**< Size of the region in bytes               */
};

/**
 * metal_io_read32() - Read a 32-bit value from an I/O region.
 * @io:     Region descriptor.
 * @offset: Byte offset from the region base.
 *
 * Issues a full compiler + CPU memory barrier after the read, matching
 * the behaviour of the libmetal implementation on Linux.
 *
 * Return: The 32-bit value at @offset.
 */
static inline uint32_t metal_io_read32(struct metal_io_region *io,
				       unsigned long offset)
{
	volatile uint32_t *ptr =
		(volatile uint32_t *)((char *)io->virt + offset);
	uint32_t val = *ptr;

	__sync_synchronize();
	return val;
}

/**
 * metal_io_write32() - Write a 32-bit value to an I/O region.
 * @io:     Region descriptor.
 * @offset: Byte offset from the region base.
 * @value:  Value to write.
 *
 * Issues a full barrier before the write so the store is visible to the
 * device immediately.
 */
static inline void metal_io_write32(struct metal_io_region *io,
				    unsigned long offset, uint32_t value)
{
	volatile uint32_t *ptr =
		(volatile uint32_t *)((char *)io->virt + offset);

	__sync_synchronize();
	*ptr = value;
}

/**
 * metal_io_virt() - Get a virtual pointer to an offset within a region.
 * @io:     Region descriptor.
 * @offset: Byte offset from the region base.
 *
 * Return: Pointer to the byte at @offset.
 */
static inline void *metal_io_virt(struct metal_io_region *io,
				  unsigned long offset)
{
	return (void *)((char *)io->virt + offset);
}

/**
 * metal_vfio_device_open() - Open a PCIe device via VFIO and map BAR0.
 * @bdf:              PCI BDF string, e.g. "0000:e2:00.0".
 * @bar0_out:         Populated with virtual address and size of BAR0.
 * @device_fd_out:    Receives VFIO device fd (caller stores for close).
 * @group_fd_out:     Receives VFIO group fd.
 * @container_fd_out: Receives VFIO container fd.
 *
 * Performs the full VFIO open sequence:
 *   1. Resolve IOMMU group from /sys/bus/pci/devices/<bdf>/iommu_group
 *   2. Open /dev/vfio/vfio container
 *   3. Open /dev/vfio/<group_id> group
 *   4. Associate group with container, set TYPE1 IOMMU
 *   5. Get device fd for @bdf
 *   6. Query BAR0 region info (VFIO_DEVICE_GET_REGION_INFO, index 0)
 *   7. mmap BAR0 via the device fd
 *
 * All three fds must be kept open while the BAR mapping is in use.
 * Pass them unchanged to metal_vfio_device_close() when done.
 *
 * Return: 0 on success, -1 on error (message printed to stderr).
 */
int metal_vfio_device_open(const char *bdf,
			   struct metal_io_region *bar0_out,
			   int *device_fd_out,
			   int *group_fd_out,
			   int *container_fd_out);

/**
 * metal_vfio_device_close() - Unmap BAR0 and close all VFIO file descriptors.
 * @bar0:         Region descriptor populated by metal_vfio_device_open().
 * @device_fd:    VFIO device fd from metal_vfio_device_open() (-1 = skip).
 * @group_fd:     VFIO group fd (-1 = skip).
 * @container_fd: VFIO container fd (-1 = skip).
 *
 * Safe to call with MAP_FAILED or NULL virt; fd values of -1 are skipped.
 */
void metal_vfio_device_close(struct metal_io_region *bar0,
			     int device_fd,
			     int group_fd,
			     int container_fd);

#endif  /* USE_LIBMETAL */

#endif  /* _METAL_COMPAT_H_ */
