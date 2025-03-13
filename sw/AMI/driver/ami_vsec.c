// SPDX-License-Identifier: GPL-2.0-only
/*
 * ami_vsec.c - This file contains logic to parse PCI XILINX VSEC.
 *
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 */

#include "ami_vsec.h"
#include "ami_pci_dbg.h"
#include "ami_amc_control.h"


/* VSEC is only applicable for xilinx-vendor boards
 * No need to check the vendor ID is PCIE_VENDOR_ID_XILINX here prior to VSEC
 * discovery as only Xilinx card are used in MODULE_DEVICE_TABLE */

int read_logic_uuid(struct pci_dev *dev, endpoints_struct **endpoints)
{
	int ret = 0;
	int i = 0;
	void __iomem *virt_addr = NULL;
	struct amc_shared_mem shared_mem;

	if (!dev || !endpoints)
		return -EINVAL;

	if (!(*endpoints)->gcq.found) {
		DEV_ERR(dev, "Endpoint %s not found!!",
			XILINX_ENDPOINT_NAME_SGCQ);
		ret = -ENODEV;
		goto fail;
	}

	ret = pci_request_region(dev, (*endpoints)->gcq.bar_num,
			PCIE_BAR_NAME[(*endpoints)->gcq.bar_num]);
	if (ret) {
		DEV_ERR(dev, "Could not request %s region (%s)",
			PCIE_BAR_NAME[(*endpoints)->gcq.bar_num],
			(*endpoints)->gcq.name);
		ret = -EIO;
		goto fail;
	}

	virt_addr = pci_iomap_range(dev, (*endpoints)->gcq.bar_num,
			(*endpoints)->gcq.start_addr,
			(*endpoints)->gcq.bar_len);
	if (!virt_addr) {
		DEV_ERR(dev, "Could not map %s endpoint into virtual memory at start address 0x%llx",
			(*endpoints)->gcq.name, (*endpoints)->gcq.start_addr);
		ret = -EIO;
		goto release_bar;
	}
	memcpy_fromio(&shared_mem,
			virt_addr + XILINX_SGCQ_SIZE_BYTES,
			sizeof(struct amc_shared_mem));

	if ((shared_mem.uuid.amc_uuid_off + shared_mem.uuid.amc_uuid_len)
		> (*endpoints)->gcq.bar_len) {
		DEV_ERR(dev, "Could not map %s UUID offset 0x%08x out of range",
			(*endpoints)->gcq.name, shared_mem.uuid.amc_uuid_off);
		ret = -EIO;
		goto release_bar;
	}

	virt_addr += shared_mem.uuid.amc_uuid_off;
	(*endpoints)->logic_uuid_str[0] = '\0';
	for (i = ARRAY_SIZE((*endpoints)->logic_uuid) - 1; i >= 0; i--) {
		(*endpoints)->logic_uuid[i] = \
			ioread32(virt_addr + sizeof(uint32_t) * i);

		sprintf((*endpoints)->logic_uuid_str + \
			strlen((*endpoints)->logic_uuid_str),
			"%08x", (*endpoints)->logic_uuid[i]);
	}

	DEV_INFO(dev, "Logic uuid = %s", (*endpoints)->logic_uuid_str);
	pci_iounmap(dev, virt_addr);
	pci_release_region(dev, (*endpoints)->gcq.bar_num);

	return SUCCESS;

release_bar:
	pci_release_region(dev, (*endpoints)->gcq.bar_num);

fail:
	DEV_ERR(dev, "Failed to read logic UUID");
	return ret;
}

int read_vsec(struct pci_dev *dev, uint32_t vsec_base_addr,
	endpoints_struct **endpoints)
{
	int ret = 0;
	uint8_t pcie_function_num = 0;

	if (!dev || !endpoints)
		return -EINVAL;

	pcie_function_num = PCI_FUNC(dev->devfn);

	DEV_VDBG(dev, "Reading vendor specific information for PF %d",
		pcie_function_num);

	(*endpoints) = kzalloc(sizeof(endpoints_struct), GFP_KERNEL);
	if (!(*endpoints)) {
		DEV_ERR(dev, "Failed to allocate memory for endpoints");
		ret = -ENOMEM;
		goto fail;
	}

	/* sGCQ payload BAR 0: Offset 0 */
	(*endpoints)->gcq.found = true;
	(*endpoints)->gcq.bar_num = PCIE_BAR0;
	(*endpoints)->gcq.start_addr = 0;
	(*endpoints)->gcq.bar_len = XILINX_ENDPOINT_BAR_LEN_SGCQ;
	(*endpoints)->gcq.end_addr = (*endpoints)->gcq.start_addr +
						(*endpoints)->gcq.bar_len - 1;

	strcpy((*endpoints)->gcq.name, XILINX_ENDPOINT_NAME_SGCQ);
	print_endpoint_info(dev, (*endpoints)->gcq);

	/* PL BAR 1: offset 0 */
	(*endpoints)->pl.found = true;
	(*endpoints)->pl.bar_num = PCIE_BAR1;
	(*endpoints)->pl.start_addr = 0;
	(*endpoints)->pl.bar_len = XILINX_ENDPOINT_BAR_LEN_PL;
	(*endpoints)->pl.end_addr = (*endpoints)->pl.start_addr +
					(*endpoints)->pl.bar_len - 1;

	strcpy((*endpoints)->pl.name, XILINX_ENDPOINT_NAME_PL_PF0);
	print_endpoint_info(dev, (*endpoints)->pl);

	ret = read_logic_uuid(dev, endpoints);
	if (ret)
		goto fail;

	DEV_VDBG(dev, "Successfully read Vendor Specific Region (VSEC)");
	return SUCCESS;

fail:
	release_vsec_mem(endpoints);
	DEV_ERR(dev, "Failed to read Vendor Specific Region (VSEC)");
	return ret;
}

void release_endpoints(endpoints_struct **endpoints)
{
	if (endpoints && *endpoints) {
		kfree(*endpoints);
		*endpoints = NULL;
	}
}

void release_vsec_mem(endpoints_struct **endpoints)
{
	if (!endpoints)
		release_endpoints(endpoints);
}
