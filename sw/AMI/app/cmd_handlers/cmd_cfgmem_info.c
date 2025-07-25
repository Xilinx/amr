// SPDX-License-Identifier: GPL-2.0-only
/*
 * cmd_cfgmem_info.c - This file contains the implementation for the command "cfgmem_info"
 *
 * Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
 */

/* Standard includes */
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>

/* API includes */
#include "ami.h"
#include "ami_program.h"

/* App includes */
#include "meta.h"
#include "commands.h"

/*****************************************************************************/
/* Function declarations                                                     */
/*****************************************************************************/

/**
 * do_cmd_cfgmem_info() - "cfgmem_info" command callback.
 * @options:  Ordered list of options passed in at the command line
 * @num_args:  Number of non-option arguments (excluding command)
 * @args:  List of non-option arguments (excluding command)
 *
 * `args` may be an invalid pointer. It is the function's responsibility
 * to validate the `num_args` parameter.
 *
 * Return: EXIT_SUCCESS or EXIT_FAILURE
 */
static int do_cmd_cfgmem_info(struct app_option *options, int num_args, char **args);

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/

/*
 * h: Help
 * d: Device
 * f: Output format
 * o: Output file
 */
static const char short_options[] = "hd:t:f:o:";

static const struct option long_options[] = {
	{ "help", no_argument, NULL, 'h' },  /* help screen */
	{ },
};

static const char help_msg[] = \
	"cfgmem_info - get fpt information\r\n"
	"\r\nUsage:\r\n"
	"\t" APP_NAME " cfgmem_info -d <bdf> -t <type> [options...]\r\n"
	"\r\nOptions:\r\n"
	"\t-h --help            Show this screen\r\n"
	"\t-d <b>:[d].[f]       Specify the device BDF\r\n"
	"\t-t <type>            Specify the boot device type (primary or secondary)\r\n"
	"\t-f <table|json>      Set the output format\r\n"
	"\t-o <file>            Specify output file\r\n"
;

struct app_cmd cmd_cfgmem_info = {
	.callback      = &do_cmd_cfgmem_info,
	.short_options = short_options,
	.long_options  = long_options,
	.root_required = false,
	.help_msg      = help_msg
};

/*****************************************************************************/
/* Function implementations                                                  */
/*****************************************************************************/

/**
 * "cfgmem_info" command callback.
 * @options:  Ordered list of options passed in at the command line
 * @num_args:  Number of non-option arguments (excluding command)
 * @args:  List of non-option arguments (excluding command)
 *
 * `args` may be an invalid pointer. It is the function's responsibility
 * to validate the `num_args` parameter.
 *
 * Return: EXIT_SUCCESS or EXIT_FAILURE
 */
static int do_cmd_cfgmem_info(struct app_option *options, int num_args, char **args)
{
	int ret = EXIT_FAILURE;
	/* Required options */
	struct app_option *device = NULL;
	struct app_option *boot_device_type = NULL;

	/* Required data */
	ami_device *dev = NULL;
	int selected_boot_device = 0;

	/* Must have device at least. */
	if (!options) {
		APP_USER_ERROR("not enough options", help_msg);
		return EXIT_FAILURE;
	}

	/* Device is required. */
	device = find_app_option('d', options);
	boot_device_type = find_app_option('t', options);

	if (!device || !boot_device_type) {
		APP_USER_ERROR("not enough arguments", help_msg);
		return AMI_STATUS_ERROR;
	}

	if (strcmp(boot_device_type->arg, "primary") == 0) {
		selected_boot_device = AMI_BOOT_DEVICES_PRIMARY;
	} else if (strcmp(boot_device_type->arg, "secondary") == 0) {
		selected_boot_device = AMI_BOOT_DEVICES_SECONDARY;
	} else{
		APP_USER_ERROR("provided boot device does not exist", help_msg);
		return AMI_STATUS_ERROR;
	}

	/* Find device */
	if (ami_dev_find(device->arg, &dev) != AMI_STATUS_OK) {
		APP_API_ERROR("could not find the requested device");
		return AMI_STATUS_ERROR;
	}

	ret = print_fpt_info(dev, selected_boot_device, options);

	ami_dev_delete(&dev);
	return ret;
}
