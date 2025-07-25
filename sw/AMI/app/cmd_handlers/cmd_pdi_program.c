// SPDX-License-Identifier: GPL-2.0-only
/*
 * cmd_pdi_program.c - This file contains the implementation for the command
 * "pdi_program"
 *
 * Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
 */

/* Standard includes */
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

/* API includes */
#include "ami.h"
#include "ami_program.h"

/* App includes */
#include "commands.h"
#include "apputils.h"

/*****************************************************************************/
/* Defines                                                                   */
/*****************************************************************************/
#define PROGRESS_BAR_WIDTH (100)

/*****************************************************************************/
/* Function declarations                                                     */
/*****************************************************************************/

/**
 * do_cmd_pdi_program() - "pdi_program" command callback.
 * @options:  Ordered list of options passed in at the command line
 * @num_args:  Number of non-option arguments (excluding command)
 * @args:  List of non-option arguments (excluding command)
 *
 * `args` may be an invalid pointer. It is the function's responsibility
 * to validate the `num_args` parameter.
 *
 * Return: EXIT_SUCCESS or EXIT_FAILURE
 */
static int do_cmd_pdi_program(struct app_option *options, int num_args, char **args);

/**
 * progress_handler() - Event handler for the PDI download operation.
 * @status: Event status.
 * @ctr: Event counter - equal to the number of bytes written.
 * @data: Pointer to PDI progress struct.
 *
 * Return: None.
 */
static void progress_handler(enum ami_event_status status, uint64_t ctr, void *data);

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/

/*
 * h: Help
 * d: Device
 * i: Image file
 * y: Skip user confirmation
 * q: Quit after programming
 */
static const char short_options[] = "hd:i:yqa";

static const struct option long_options[] = {
	{ "help", no_argument, NULL, 'h' },  /* help screen */
	{ },
};

static const char help_msg[] = \
	"mem_program - program a bitstream onto a device\r\n"
	"\r\nThis command requires root/sudo permissions.\r\n"
	"\r\nUsage:\r\n"
	"\t" APP_NAME " mem_program -d <bdf> -i <path> <-a>\r\n"
	"\r\nOptions:\r\n"
	"\t-h --help             Show this screen\r\n"
	"\t-d <b>:[d].[f]        Specify the device BDF\r\n"
	"\t-i <path>             Path to image file\r\n"
	"\t-a                    APU image\r\n"
	"\t-y                    Skip confirmation\r\n"
	"\t-q                    Quit after programming\r\n"
;

struct app_cmd cmd_pdi_program = {
	.callback		= &do_cmd_pdi_program,
	.short_options	= short_options,
	.long_options	= long_options,
	.root_required	= true,
	.help_msg		= help_msg
};

/*****************************************************************************/
/* Function implementations                                                  */
/*****************************************************************************/

/*
 * Event handler for PDI download.
 */
static void progress_handler(enum ami_event_status status, uint64_t ctr, void *data)
{
	struct ami_pdi_progress *prog = NULL;

	if (!data)
		return;

	prog = (struct ami_pdi_progress*)data;

	if (status == AMI_EVENT_STATUS_OK)
		prog->bytes_written += ctr;

	prog->reserved = print_progress_bar(
		prog->bytes_written,
		prog->bytes_to_write,
		PROGRESS_BAR_WIDTH,
		'[',
		']',
		'#',
		'.',
		prog->reserved
	);
}

/**
 * "program" command callback.
 * @options:  Ordered list of options passed in at the command line
 * @num_args:  Number of non-option arguments (excluding command)
 * @args:  List of non-option arguments (excluding command)
 *
 * `args` may be an invalid pointer. It is the function's responsibility
 * to validate the `num_args` parameter.
 *
 * Return: EXIT_SUCCESS or EXIT_FAILURE
 */
static int do_cmd_pdi_program(struct app_option *options, int num_args, char **args)
{
	int ret = EXIT_FAILURE;

	/* Required options */
	struct app_option *device = NULL;
	struct app_option *image = NULL;

	/* Required data */
	uint16_t bdf = 0;
	ami_device *dev = NULL;

	/* For UUID checks */
	int found_current_uuid = AMI_STATUS_ERROR;
	int found_parent_uuid = AMI_STATUS_ERROR;
	char current_uuid[AMI_LOGIC_UUID_SIZE] = { 0 };
	char parent_uuid[AMI_LOGIC_UUID_SIZE] = { 0 };

	/* Must have at least an image and device. */
	if (!options) {
		APP_USER_ERROR("not enough options", help_msg);
		return ret;
	}

	/* Device and image are required. TODO: Should these be positional args? */
	device = find_app_option('d', options);
	image  = find_app_option('i', options);

	if (!device || !image) {
		APP_USER_ERROR("not enough arguments", help_msg);
		return AMI_STATUS_ERROR;
	}

	/* Check that the provided PDI image exists. */
	if (access(image->arg, F_OK) != AMI_LINUX_STATUS_OK) {
		/* File does not exist */
		APP_ERROR("provided image does not exist");
		return AMI_STATUS_ERROR;
	}

	/* Find device */
	if (ami_dev_find(device->arg, &dev) != AMI_STATUS_OK) {
		APP_API_ERROR("could not find the requested device");
		return AMI_STATUS_ERROR;
	}

	/* Check compatibility mode */
	warn_compat_mode(dev);

	ami_dev_get_pci_bdf(dev, &bdf);

	found_current_uuid = ami_dev_read_uuid(dev, current_uuid);
	found_parent_uuid = find_parent_uuid(image->arg, parent_uuid);

	printf("\n"
		"----------------------------------------------\r\n"
		"Device      | %02x:%02x.%01x\r\n"
		"----------------------------------------------\r\n\n"
		"Current Configuration\r\n"
		"----------------------------------------------\r\n"
		"UUID        | %s\r\n"
		"----------------------------------------------\r\n\n"
		"Incoming Configuration\r\n"
		"----------------------------------------------\r\n"
		"Parent UUID | %s\r\n"
		"Path        | %s\r\n"
		"----------------------------------------------\r\n",
		AMI_PCI_BUS(bdf),
		AMI_PCI_DEV(bdf),
		AMI_PCI_FUNC(bdf),
		((found_current_uuid != AMI_STATUS_OK) ? ("N/A") : (current_uuid)),
		((found_parent_uuid  != AMI_STATUS_OK) ? ("N/A") : (parent_uuid)),
		image->arg
	);


	if ((NULL == find_app_option('a', options)) &&
		(strcasecmp(current_uuid, parent_uuid) != 0)) {
		printf("\r\nError: %s's parent ID doesn't match...\r\n",
			image->arg);
	}
	else if ((NULL != find_app_option('y', options)) ||
		confirm_action(APP_CONFIRM_PROMPT, 'Y', 3)) {
		printf("\r\nProgramming pdi image...\r\n");

		if (ami_prog_pdi(dev,
						image->arg,
						progress_handler) == AMI_STATUS_OK) {
			printf("\r\nPDI programming complete.\r\n");

			printf(
				"\r\nOK. PDI has been programmed successfully.\r\n"
				"****************************************************\r\n"
			);
			ret = EXIT_SUCCESS;
		} else {
			APP_API_ERROR("could not program PDI");
		}
	} else {
		printf("\r\nAborting...\r\n");
	}

	ami_dev_delete(&dev);

	return ret;
}

