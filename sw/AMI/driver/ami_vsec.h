// SPDX-License-Identifier: GPL-2.0-only
/*
 * ami_vsec.h - This file contains definitions to parse PCI XILINX VSEC.
 *
 * Copyright (C) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef AMI_VSEC_H
#define AMI_VSEC_H

#include <linux/types.h>
#include <linux/pci.h>

#include "ami.h"
#include "ami_pcie.h"

/* PCIe BAR OFFSETS and Lengths */
#define XILINX_ENDPOINT_BAR_SGCQ_RAVE_OFFSET	0x800000  /* 8M */
#define XILINX_ENDPOINT_BAR_PL_RAVE_OFFSET		0x000000  /* 0M */
#define XILINX_ENDPOINT_BAR_SGCQ_V80_OFFSET		0x8000000 /* 128M */
#define XILINX_ENDPOINT_BAR_PL_V80_OFFSET		0x8000000 /* 128M */
#define XILINX_ENDPOINT_BAR_SGCQ_LEN			0x800000  /* 8M   */
#define XILINX_ENDPOINT_BAR_PL_LEN				0x800000  /* 8M   */

/* CG TODO: Get this from hw design metadata */
#define XILINX_ENDPOINT_NAME_SGCQ			"ep_gcq_mgmt_00"
#define XILINX_ENDPOINT_NAME_PL_PF0			"ep_pl_mgmt_00"

#define XILINX_LOGIC_UUID_SIZE_BYTES		16
#define XILINX_SGCQ_SIZE_BYTES				0x1000		/* sGCQ size */

typedef struct {
	endpoint_info_struct gcq;
	endpoint_info_struct pl;

	uint32_t logic_uuid[XILINX_LOGIC_UUID_SIZE_BYTES/sizeof(uint32_t)];
	char     logic_uuid_str[XILINX_LOGIC_UUID_SIZE_BYTES*2+1];
} endpoints_struct;

int read_vsec(struct pci_dev *dev, endpoints_struct **endpoints);

void release_endpoints(endpoints_struct **endpoints);
void release_vsec_mem(endpoints_struct **endpoints);

#endif /* AMI_VSEC_H */
