// SPDX-License-Identifier: GPL-2.0-only
/*
 * ami.h - This file contains generic AMI driver definitions.
 *
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef AMI_DRIVER_H
#define AMI_DRIVER_H

#include <linux/printk.h>

/* Meta Information */
#define MDL_VERSION	"1.0.0"
#define MDL_DESCRIPTION	"Adapative Management Interface (AMI) is used to manage AMR-based devices through PCIe"
#define MDL_AUTHOR	"AMD, Inc."
#define MDL_RELDATE	"2025"
#define MDL_LICENSE	"GPL"

/* Enables debug messages in dmesg */
extern bool ami_debug_enabled;

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define PR_ERR(fmt, arg ...)       pr_err("ERROR           : " fmt "\n", ## arg)
#define PR_INFO(fmt, arg ...)      pr_info("INFO            : " fmt "\n", ## arg)
#define PR_WARN(fmt, arg ...)      pr_warn("WARNING         : " fmt "\n", ## arg)
#define PR_CRIT_WARN(fmt, arg ...) pr_warn("CRITICAL WARNING: " fmt "\n", ## arg)
#define PR_DBG(fmt, arg ...)       do{ if (ami_debug_enabled) pr_debug("DEBUG           : " fmt "\n", ## arg); \
} while (0)

#define SUCCESS  0
#define FAILURE -1

#define BDF_STR_LEN 7

#define XILINX_ENDPOINT_NAME_SIZE 30

typedef struct {
	bool		found;
	uint8_t		bar_num;
	uint64_t	start_addr;
	uint64_t	end_addr;
	uint64_t	bar_len;
	char		name[XILINX_ENDPOINT_NAME_SIZE];
} endpoint_info_struct;

#endif /* AMI_DRIVER_H */
