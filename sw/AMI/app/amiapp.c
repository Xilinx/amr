// SPDX-License-Identifier: GPL-2.0-only
/*
 * amiapp.c - This file contains the command line application for the AMI API
 *
 * Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
 */

/* Standard includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* API includes */
#include "ami.h"
#include "ami_version.h"
#include "ami_device.h"

/* App includes */
#include "amiapp.h"
#include "apputils.h"
#include "commands.h"

/*****************************************************************************/
/* Defines                                                                   */
/*****************************************************************************/

/* First element in cmd map is for empty commands. */
#define EMPTY_CMD_HANDLER	(0)
#define CMD_INDEX			(1)

/*****************************************************************************/
/* Local function declarations                                               */
/*****************************************************************************/

/**
 * do_cmd_none() - Global command callback when no CLI command is specified.
 * @options:  Ordered list of options passed in at the command line
 * @num_args:  Number of non-option arguments (excluding command)
 * @args:  List of non-option arguments (excluding command)
 *
 * `args` may be an invalid pointer. It is the function's responsibility
 * to validate the `num_args` parameter.
 *
 * Return: EXIT_SUCCESS or EXIT_FAILURE
 */
static int do_cmd_none(struct app_option *options, int num_args, char **args);

/**
 * init_profile_from_device() - Helper function to get the V80|Rave profile based on PCIE device id
 *
 * Return: PROFILE_V80, PROFILE_RAVE or PROFILE_DEFAULT
 */
static uint16_t init_profile_from_device(void);

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/

/*
 * The program version information string.
 */
static const char api_version_str[] = \
	"API Version     |  %d.%d.%d%c (%d)\r\n"
	"API Branch      |  " GIT_BRANCH "\r\n"
	"API Hash        |  " GIT_HASH   "\r\n"
	"API Hash Date   |  " GIT_DATE   "\r\n"
;

static const char driver_version_str[] = \
	"Driver Version  |  %d.%d.%d%c (%d)\r\n"
;

/*
 * List of short options supported when no command is specified.
 * Short options may correspond to long options but this is not always the case.
 *
 * :  means that the option takes a required argument
 * :: means that the option takes an optional argument
 *
 * h: Show help screen.
 */
static const char short_options[] = "h";

/*
 * List of long options supported when no command is specified.
 * These may or may not correspond to a short counterpart.
 */
static const struct option long_options[] = {
	{ "version", no_argument,       NULL, 'V' },  /* program version */
	{ "help",    no_argument,       NULL, 'h' },  /* help screen */
	{ },
};

static struct app_cmd cmd_none = {
	.callback      = &do_cmd_none,
	.short_options = short_options,
	.long_options  = long_options,
	.root_required = false,
	.help_msg      = NULL
};

/*
 * List of supported commands.
 */
static const struct app_cmd_map commands[] =  {
	{ "",                &cmd_none,
		PROFILE_DEFAULT,"\t\r\n"},
	{ "sensors",         &cmd_sensors,
		PROFILE_DEFAULT,"\tShow sensors details\r\n"},
	{ "cfgmem_program",  &cmd_cfgmem_program,
		PROFILE_DEFAULT,"\tProgram a device\r\n" },
	{ "cfgmem_copy",     &cmd_cfgmem_copy,
		PROFILE_DEFAULT,"\tCopy one partition to another\r\n"  },
	{ "cfgmem_info",     &cmd_cfgmem_info,
		PROFILE_DEFAULT,"\tShow partition details \r\n" },
	{ "pdi_program",     &cmd_pdi_program,
		PROFILE_DEFAULT,"\tProgram pdi\r\n"  },
	{ "partition_flag_rd", &cmd_partition_flag_rd,
		PROFILE_DEFAULT,"\tRead partition flags\r\n" },
	{ "partition_flag_wr", &cmd_partition_flag_wr,
		PROFILE_DEFAULT,"\tWrite partition flags\r\n" },
	{ "bar_rd",          &cmd_bar_rd,
		PROFILE_V80,	"\tRead from PCI BAR memory\r\n" },
	{ "bar_wr",          &cmd_bar_wr,
		PROFILE_V80,	"\tWrite to PCI BAR memory\r\n" },
	{ "overview",        &cmd_overview,
		PROFILE_DEFAULT,"\tShow basic AMI/device details\r\n" },
	{ "pcieinfo",        &cmd_pcieinfo,
		PROFILE_DEFAULT,"\tView PCI-related details\r\n" },
	{ "reload",          &cmd_reload,
		PROFILE_DEFAULT,"\tReload a device/devices\r\n" },
	{ "device_boot",     &cmd_device_boot,
		PROFILE_DEFAULT,"\tSet boot partition\r\n" },
	{ "mfg_info",        &cmd_mfg_info,
		PROFILE_DEFAULT,"\tView manufacturing details\r\n" },
	{ "eeprom_rd",       &cmd_eeprom_rd,
		PROFILE_DEFAULT,"\tRead data from the device EEPROM\r\n" },
	{ "eeprom_wr",       &cmd_eeprom_wr,
		PROFILE_V80,	"\tWrite data to the device EEPROM\r\n" },
	{ "cfgmem_fpt",      &cmd_cfgmem_fpt,
		PROFILE_DEFAULT,"\tProgram a device and update the FPT\r\n" },
	{ "module_byte_rd",  &cmd_module_byte_rd,
		PROFILE_V80,	"\tRead data from a QSFP module\r\n" },
	{ "module_byte_wr",  &cmd_module_byte_wr,
		PROFILE_V80,	"\tWrite data to a QSFP module\r\n" },
	{ "debug_verbosity", &cmd_debug_verbosity,
		PROFILE_DEFAULT,"\tSet the AMC debug level\r\n" },
};

/*****************************************************************************/
/* Public function definitions                                               */
/*****************************************************************************/

/*
 * Print the help msg
 */
static const char *get_profile_help_msg(void)
{
	static char buffer[4096];  // Make sure this is big enough
	size_t offset = 0;
	uint16_t current_profile;

	current_profile = init_profile_from_device();

	offset += snprintf(buffer + offset, sizeof(buffer) - offset,
			APP_NAME " - command line tool for the AMI driver API\r\n"
			"Usage:\r\n"
			"\t" APP_NAME " {command} [arguments]\r\n"
			"\t" APP_NAME " {command} -h | --help\r\n"
			"\t" APP_NAME " -h | --help\r\n"
			"\t" APP_NAME " --version\r\n"
			"Options:\r\n"
			"\t-h --help          Show this screen\r\n"
			"\t--version          Show version\r\n"
			"Commands:\r\n" );

	for (size_t i = 0; i < ARRAY_SIZE(commands); i++) {
		if (commands[i].valid_profiles_mask & current_profile) {
			offset += snprintf(buffer + offset, sizeof(buffer) - offset,
			"\t%-18s %s",
			commands[i].name,
			commands[i].msg ? commands[i].msg : "");
		}
	}
	return buffer;
}

/*
 * Find a CLI option.
 */
struct app_option* find_app_option(const int val, struct app_option *options)
{
	struct app_option *opt = options;

	while (opt) {
		if (opt->val == val) {
			return opt;
		}

		opt = opt->next;
	}

	return NULL;
}

/*
 * Find a command by its name.
 */
int find_app_command(const char *name)
{
	int cmd = 0;

	if (!name)
		return APP_INVALID_INDEX;

	for (cmd = 0; cmd < ARRAY_SIZE(commands); cmd++) {
		if (strcmp(commands[cmd].name, name) == 0) {
			return cmd;
		}
	}

	return APP_INVALID_INDEX;
}

/*****************************************************************************/
/* Local function definitions                                                */
/*****************************************************************************/

/*
 * Initialize device profile
 */
static uint16_t init_profile_from_device(void)
{
	uint16_t num_devs = 0;
	uint16_t pci_dev_id;
	uint16_t current_profile = PROFILE_DEFAULT;
	ami_device *dev = NULL;

	if (ami_dev_get_num_devices(&num_devs) != AMI_STATUS_OK) {
		APP_API_ERROR("Error getting number of devices");
		return current_profile;
	}
	/* Find device */
	if (ami_dev_find_next(&dev, AMI_ANY_DEV, AMI_ANY_DEV, AMI_ANY_DEV, NULL) != AMI_STATUS_OK) {
		APP_API_ERROR("could not find the requested device");
		return current_profile;
	}

	if (ami_dev_get_pci_device(dev, &pci_dev_id) != AMI_STATUS_OK) {
		APP_API_ERROR("could not get pci device id");
		return current_profile;
	}

	// use pci_dev_id to choose profile
	current_profile = (pci_dev_id == AMI_PCIE_DEVICE_ID_RAVE) ? PROFILE_RAVE : PROFILE_V80;
	return current_profile;
}


/*
 * Empty command callback.
 */
static int do_cmd_none(struct app_option *options, int num_args, char **args)
{
	bool parse_options = true;
	int ret = EXIT_FAILURE;
	struct app_option *next_opt = options;

	/* There should be no positional arguments. */
	if (num_args == 0) {
		if (!options) {
			APP_USER_ERROR("not enough arguments", get_profile_help_msg());
		} else {
			while (next_opt && parse_options) {
				switch (next_opt->val)
				{
				/* help is handled by top level parser */

				case 'V':
				{
					struct ami_version driver_ver = { 0 };

					printf(
						api_version_str,
						GIT_TAG_VER_MAJOR,
						GIT_TAG_VER_MINOR,
						GIT_TAG_VER_PATCH,
						(GIT_STATUS == 0) ? (' ') : ('*'),
						GIT_TAG_VER_DEV_COMMITS
					);

					if (ami_get_driver_version(&driver_ver) == AMI_STATUS_OK) {
						printf(
							driver_version_str,
							driver_ver.major,
							driver_ver.minor,
							driver_ver.patch,
							(driver_ver.status == 0) ? (' ') : ('*'),
							driver_ver.dev_commits
						);

						ret = EXIT_SUCCESS;
					} else {
						APP_API_ERROR("unable to retrieve driver version");
					}

					parse_options = false;
					break;
				}

				default:
					APP_USER_ERROR("invalid options", get_profile_help_msg());
					parse_options = false;
					break;
				}

				next_opt = next_opt->next;
			}
		}
	} else {
		APP_USER_ERROR("too many arguments", get_profile_help_msg());
	}

	return ret;
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE;
	int long_ind = 0;
	int opt = AMI_LINUX_STATUS_ERROR;

	int n_args = 0;
	int cmd_ind = APP_INVALID_INDEX;
	struct app_cmd *cmd = NULL;

	struct app_option *options_head = NULL;
	struct app_option *options_tail = NULL;

	/*
	 * Check if user specified a command;
	 * `argc` must be enough to have an element at `CMD_INDEX`
	 * and the element at `CMD_INDEX` must not be an option (i.e., must not
	 * start with a '-' character). Positional arguments are not allowed when
	 * a command is not specified as the argument could be confused for a command.
	 */
	if ((argc < (CMD_INDEX + 1)) || (argv[CMD_INDEX][0] == '-')) {
		cmd_ind = EMPTY_CMD_HANDLER;
	} else {
		cmd_ind = find_app_command(argv[CMD_INDEX]);
	}

	if (cmd_ind == APP_INVALID_INDEX) {
		APP_USER_ERROR("unrecognised command", get_profile_help_msg());
		exit(EXIT_FAILURE);
	}

	cmd = commands[cmd_ind].command;

	/*
	 * Parse options for the identified command (or no command).
	 */
	while (AMI_LINUX_STATUS_ERROR != (opt = getopt_long(argc, argv, \
			cmd->short_options, cmd->long_options, &long_ind)))
	{
		switch (opt) {
			/* Error. */
			case '?':
			case ':':
				APP_USER_ERROR("invalid arguments", cmd->help_msg);
				exit(EXIT_FAILURE);
				break;

			/* All other options. */
			default:
			{
				struct app_option *option = \
					(struct app_option*)calloc(1, sizeof *option);

				option->long_ind = long_ind;
				option->val = opt;
				option->arg = optarg;
				option->next = NULL;
				option->handled = false;

				if (!options_head) {
					options_head = option;
					options_tail = option;
				} else {
					options_tail->next = option;
					options_tail = option;
				}
				break;
			}
		}
	}

	if (cmd_ind != EMPTY_CMD_HANDLER) {
		if ((optind + 1) < argc)  /* +1 to exclude command */
			n_args = argc - optind - 1;
	}

	/* Check if help was requested */
	if (NULL != find_app_option('h', options_head)) {
		if (cmd->help_msg)
			printf("%s",cmd->help_msg);
		else if (cmd_ind == EMPTY_CMD_HANDLER)
			printf("%s", get_profile_help_msg());
		else
			printf("Error. No help available for command '%s'\r\n", commands[cmd_ind].name);

		ret = EXIT_SUCCESS;
	} else {
		/* Check permissions. */
		if (cmd->root_required && (geteuid() != 0))
			APP_WARN("this command requires elevated permissions but you are not running as root!\r\n");

		/* Execute command callback. */
		ret = cmd->callback(
			options_head,
			n_args,
			((n_args != 0) ? (&argv[optind + 1]) : (NULL))
		);
	}

	/* Cleanup. */
	struct app_option *next_opt = NULL;
	options_tail = options_head;

	while (options_tail) {
		next_opt = options_tail->next;
		free(options_tail);
		options_tail = next_opt;
	}

	return ret;
}
