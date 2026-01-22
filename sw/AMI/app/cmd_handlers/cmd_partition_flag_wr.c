// SPDX-License-Identifier: GPL-2.0-only
/*
 * cmd_partition_flag_wr.c - This file contains the implementation for
                             the command "partition_flag_wr"
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
 */

/* Standard includes */
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

/* API include */
#include "ami_module_access.h"
#include "ami.h"
#include "ami_program.h"

/* App includes */
#include "commands.h"
#include "apputils.h"
#include "amiapp.h"
#include "printer.h"

/*****************************************************************************/
/* Function declarations                                                     */
/*****************************************************************************/

/**
 * do_cmd_partition_flag_wr() - "partition_flag_wr" command callback.
 * @options:  Ordered list of options passed in at the command line
 * @num_args:  Number of non-option arguments (excluding command)
 * @args:  List of non-option arguments (excluding command)
 *
 * `args` may be an invalid pointer. It is the function's responsibility
 * to validate the `num_args` parameter.
 *
 * Return: EXIT_SUCCESS or EXIT_FAILURE
 */
static int do_cmd_partition_flag_wr(struct app_option *options,
	int num_args, char **args);

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/

/*
 * h: Help
 * d: Device
 * t: Boot device type
 * p: Partition number
 * i: Flags value <on|off>
 */
static const char short_options[] = "hd:t:p:i:";

static const struct option long_options[] = {
	{ "help", no_argument, NULL, 'h' },  /* help screen */
	{ },
};

static const char help_msg[] = \
	"partition_flag_wr - Write partition flags\r\n"
	"\r\nUsage:\r\n"
	"\t" APP_NAME " partition_flag_wr -d <bdf> -t <type> -p <n> -i <flags>\r\n"
	"\r\nOptions:\r\n"
	"\t-h --help             Show this screen\r\n"
	"\t-d <b>:[d].[f]        Specify the device BDF\r\n"
	"\t-t <type>             Specify the boot device type (primary or secondary)\r\n"
	"\t-p <partition>        Partition number to write\r\n"
	"\t-i <flags>            Powerup autoload flag to write\r\n"
	"\t                      Possible values are:\r\n"
	"\t                        on=power on load user partition\r\n"
	"\t                        off=do not power on load user partition\r\n"
;

struct app_cmd cmd_partition_flag_wr = {
	.callback      = &do_cmd_partition_flag_wr,
	.short_options = short_options,
	.long_options  = long_options,
	.root_required = false,
	.help_msg      = help_msg
};

/*****************************************************************************/
/* Function implementations                                                  */
/*****************************************************************************/

/**
 * "partition_flag_wr" command callback.
 * @options:  Ordered list of options passed in at the command line
 * @num_args:  Number of non-option arguments (excluding command)
 * @args:  List of non-option arguments (excluding command)
 *
 * `args` may be an invalid pointer. It is the function's responsibility
 * to validate the `num_args` parameter.
 *
 * Return: EXIT_SUCCESS or EXIT_FAILURE
 */
static int do_cmd_partition_flag_wr(struct app_option *options, int num_args, char **args)
{
	int ret = AMI_STATUS_ERROR;

	/* Required options */
	struct app_option *device = NULL;
	struct app_option *boot_device_type = NULL;
	struct app_option *partition = NULL;
	struct app_option *partition_flags = NULL;

	/* Required data */
	uint16_t bdf = 0;
	ami_device *dev = NULL;
	int selected_boot_device = 0;
	uint32_t partition_number = 0;
	struct ami_fpt_partition fpt_partition = { 0 };
	uint32_t flags = 0;

	/* Must have at least a device, boot device type, and partition number. */
	if (!options) {
		APP_USER_ERROR("not enough options", help_msg);
		return AMI_STATUS_ERROR;
	}

	/* Device, boot device type, and partition number are required. */
	device = find_app_option('d', options);
	boot_device_type = find_app_option('t', options);
	partition = find_app_option('p', options);
	partition_flags = find_app_option('i', options);

	if (!device || !boot_device_type || !partition || !partition_flags) {
		APP_USER_ERROR("not enough arguments", help_msg);
		return AMI_STATUS_ERROR;
	}

	/* Boot device type */
	if (strcmp(boot_device_type->arg, "primary") == 0) {
		selected_boot_device = AMI_BOOT_DEVICES_PRIMARY;
	} else if (strcmp(boot_device_type->arg, "secondary") == 0) {
		selected_boot_device = AMI_BOOT_DEVICES_SECONDARY;
	} else {
		APP_USER_ERROR("provided boot device does not exist", help_msg);
		return AMI_STATUS_ERROR;
	}

	/* Parition Number */
	partition_number = (uint32_t)strtoul(partition->arg, NULL, 0);

	if (partition_number > 2) {
		APP_USER_ERROR("Invalid partition number", help_msg);
		return AMI_STATUS_ERROR;
	}

	/* Parition flags */
	if (strcmp(partition_flags->arg, "on") == 0) {
		flags = 1;
	} else if (strcmp(partition_flags->arg, "off") == 0) {
		flags = 0;
	} else {
		APP_USER_ERROR("Invalid partition flag value", help_msg);
		return AMI_STATUS_ERROR;
	}

	/* Find device */
	if (ami_dev_find(device->arg, &dev) != AMI_STATUS_OK) {
		APP_API_ERROR("could not find the requested device");
		return AMI_STATUS_ERROR;
	}

	ami_dev_get_pci_bdf(dev, &bdf);

	printf(
		"Writing partition flags to (device %02x:%02x.%01x, partition %u flags 0x%08X)\r\n",
		AMI_PCI_BUS(bdf), AMI_PCI_DEV(bdf), AMI_PCI_FUNC(bdf), partition_number, flags
	);

	if (ami_prog_get_fpt_partition(dev, selected_boot_device, partition_number, &fpt_partition) != AMI_STATUS_OK) {
		APP_API_ERROR("could not write partition flags");
		goto fail;
	}

	fpt_partition.flags = flags;

	if (ami_prog_set_fpt_partition(dev, selected_boot_device, partition_number, &fpt_partition) != AMI_STATUS_OK) {
		APP_API_ERROR("could not write partition flags");
	} else {
		ret = AMI_STATUS_OK;
		printf("OK - Partition flags written successfully\r\n");
	}

fail:
	ami_dev_delete(&dev);
	return ret;
}
