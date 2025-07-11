// SPDX-License-Identifier: GPL-2.0-only
/*
 * meta.h - This file contains utilities for printing AMI info/metadata
 *
 * Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef AMI_APP_META_H
#define AMI_APP_META_H

/* API includes */
#include "ami_device.h"

/* App includes */
#include "amiapp.h"

/*****************************************************************************/
/* Function declarations                                                     */
/*****************************************************************************/

/**
 * print_overview() - Utility function to print AMI overview.
 * @options: List of command line options.
 *
 * Return: EXIT_SUCCESS or EXIT_FAILURE.
 */
int print_overview(struct app_option *options);

/**
 * print_pcieinfo() - Print PCI-related info.
 * @dev: Device handle.
 * @options: List of command line options.
 *
 * Return: EXIT_SUCCESS or EXIT_FAILURE.
 */
int print_pcieinfo(ami_device *dev, struct app_option *options);

/**
 * print_fpt_info() - Print FPT-related info.
 * @dev: Device handle.
 * @boot_device: Target boot device.
 * @options: List of command line options.
 *
 * Return: EXIT_SUCCESS or EXIT_FAILURE.
 */
int print_fpt_info(ami_device *dev, uint8_t boot_device, struct app_option *options);

/**
 * print_mfg_info() - Print Manufacturing Information.
 * @dev: Device handle.
 * @options: List of command line options.
 *
 * Return: EXIT_SUCCESS or EXIT_FAILURE.
 */
int print_mfg_info(ami_device *dev, struct app_option *options);

#endif /* AMI_APP_META_H */
