#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.

import os
import sys
import json
import getopt
import shutil
import subprocess
import datetime
import logging
from io import StringIO
from os import path
from os.path import join
from os.path import abspath


# Get the root logger
logger = logging.getLogger('')
logger.setLevel(logging.INFO)

# stream
log_stream = StringIO()
string_handler = logging.StreamHandler(stream=log_stream)
string_handler.setLevel(logging.INFO)

# stream
console_handler = logging.StreamHandler(stream=sys.stdout)
console_handler.setLevel(logging.INFO)

# formatter
formatter = logging.Formatter('%(levelname)s: %(message)s')

# add
string_handler.setFormatter(formatter)
console_handler.setFormatter(formatter)

logger.addHandler(string_handler)
logger.addHandler(console_handler)

def setup_verbose():
    logger.setLevel(logging.DEBUG)
    string_handler.setLevel(logging.DEBUG)
    console_handler.setLevel(logging.DEBUG)

def setup_logfile(log_file):
    with open(log_file, 'w') as fd:
        log_stream.seek(0)
        shutil.copyfileobj(log_stream, fd)

    file_handler = logging.FileHandler(log_file, 'a')
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(formatter)
    logger.addHandler(file_handler)
    logger.removeHandler(string_handler)

def exit_msg(level, id, msg):
    log_msg(level, id, msg)
    if level == logging.ERROR:
        sys.exit(1)
    else:
        sys.exit(0)

def exit_info(id, msg):
    exit_msg(logging.INFO, id, msg)

def exit_error(id, msg):
    exit_msg(logging.ERROR, id, msg)

def time_str(time_in):
    return time_in.strftime('%Y-%m-%d, %H:%M:%S')

def log_debug(id, msg):
    logger.debug(     '[' + id + '] ' + msg)
def log_info(id, msg):
    logger.info(      '[' + id + '] ' + msg)
def log_warning(id, msg):
    logger.warning(   '[' + id + '] ' + msg)
def log_error(id, msg):
    logger.error(     '[' + id + '] ' + msg)
def log_msg(level, id, msg):
    logger.log(level, '[' + id + '] ' + msg)
def log_newline():
	string_handler.setFormatter(logging.Formatter(fmt=''))
	console_handler.setFormatter(logging.Formatter(fmt=''))
	logger.info( '\n')
	console_handler.setFormatter(formatter)
	string_handler.setFormatter(formatter)

CWD = os.getcwd(); # get current working directory

DIST_ID_CENTOS  = 'CentOS'
DIST_ID_UBUNTU  = 'Ubuntu'
DIST_ID_REDHAT  = 'RedHatEnterprise'
DIST_ID_REDHAT2 = 'RedHatEnterpriseWorkstation'
DIST_ID_SLES    = 'SUSE'
SUPPORTED_DIST_ID = [
    DIST_ID_CENTOS,
    DIST_ID_UBUNTU,
    DIST_ID_REDHAT,
    DIST_ID_REDHAT2,
    DIST_ID_SLES,
]
DIST_RPM = [DIST_ID_CENTOS, DIST_ID_REDHAT, DIST_ID_REDHAT2, DIST_ID_SLES]
DIST_DEB = [DIST_ID_UBUNTU]

ARCH_X86_64     = 'x86_64'
ARCH_AMD64      = 'amd64'
ARCH_PPC64LE    = 'ppc64le'
ARCH_PPC64      = 'ppc64'
SUPPORTED_ARCH = [
    ARCH_X86_64,
    ARCH_AMD64,
    ARCH_PPC64LE,
    ARCH_PPC64,
]

def start(id, file):
    start_time = datetime.datetime.now().replace(microsecond=0)
    log_info(id, '------------------------------------------------------------------------')
    log_info(id, '[' + time_str(start_time) + '] Starting ' + file)
    log_info(id, '-------------------------------------------------------------------------')
    return start_time

def tear_down(id, file, start_time):
    end_time = datetime.datetime.now().replace(microsecond=0)
    elapsed_time = end_time - start_time
    log_info(id, '------------------------------------------------------------------------')
    log_info(id, '[' + time_str(end_time) + '] ' + file + ' END. Total Elapsed Time: ' + str(elapsed_time))
    log_info(id, '------------------------------------------------------------------------')
    sys.exit(0)

def copy_source_dir(id, src_dir, dest_dir):
    log_debug(id, 'Source copied locally to: ' + dest_dir)
    if not path.isdir(dest_dir):
        os.makedirs(dest_dir)
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            shutil.copy(abspath(join(src_dir, file)), dest_dir)
            os.chmod(abspath(join(dest_dir, file)), 511); # octal 777
        for dir in dirs:
            dst = abspath(join(dest_dir, dir))
            shutil.copytree(abspath(join(src_dir, dir)), dst)
            os.chmod(dst, 511); # octal 777
        break   #prevent descending into sub-folders

def copy_source_file(id, src_file, dest_dir):
    if not path.isfile(src_file):
        exit_error(id, 'Source file does not exist: ' + src_file)
    if not path.isdir(dest_dir):
        os.makedirs(dest_dir)
    log_debug(id, 'Source copied locally to: ' + abspath(join(dest_dir, path.basename(src_file))))
    shutil.copy(src_file, dest_dir)

def check_log_error(id, step, log_file_name):
    # check for error in log
    log_file = open(log_file_name, mode='r')
    step_error = False
    for line in log_file:
        txt = 'ERROR: ['
        if txt == line[0:len(txt)-1]:
            step_error = True
            print(line)
    log_file.close()
    if step_error:
        exit_error(id, 'Messages containing pattern "ERROR: [" found in step: ' + step + '. Please check: ' + log_file_name)

def start_step(id, step):
    start_time = datetime.datetime.now().replace(microsecond=0)
    log_info(id, '********** [' + time_str(start_time) + '] Starting step: ' + step)
    return start_time

def end_step(id, start_time):
    elapsed_time = datetime.datetime.now().replace(microsecond=0) - start_time
    log_info(id, '********** End of step. Elapsed time: ' + str(elapsed_time) + '\n\n')

def exec_step_cmd(id, step, cmd, log_file_name = None, use_console = False, shell = False,
                  ignore_error = False, env = None, expect_fail = False, cwd = None):
    cmd_str = cmd if shell else ' '.join(cmd)
    log_info(id, 'Executing: $ ' + cmd_str)

    proc = subprocess.Popen(cmd, shell=shell, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, env=env, cwd=cwd)

    if log_file_name is not None:
        log_info(id, 'Log file: ' + log_file_name)
        log_file = open(log_file_name, mode='w')

    for line in proc.stdout:
        if use_console:
            sys.stdout.write(line)
            sys.stdout.flush()

        if log_file_name is not None:
            log_file.write(line)
            log_file.flush()

    proc.wait()

    if not ignore_error:
        err_log_msg = ''
        if log_file_name is not None:
            log_file.close()
            check_log_error(id, step, log_file_name)
            err_log_msg = '. Check log for more details: ' + log_file_name

        if not expect_fail and proc.returncode != 0:
            exit_error(id, 'Step ' + step + ' failed: Unexpected non-zero return code (' + str(proc.returncode) + ') for command: ' + cmd_str + err_log_msg)
        elif expect_fail and proc.returncode == 0:
            exit_error(id, 'Step ' + step + ' failed: Unexpected zero return code for command: ' + cmd_str + err_log_msg)
    return proc.returncode

def json_load(id, name, file):
    json_data = {}
    if not path.isfile(file):
        exit_error(id, 'Failed to load ' + name + '. File does not exist: ' + file)
    with open(file) as infile:
        try:
            json_data = json.load(infile)
        except ValueError as e:
            exit_error(id, 'Failed to load ' + name + '. File contains invalid JSON content: ' + file + '. JSON parser error: ' + str(e))
    return json_data

def check_file_exists(id, outfile):
    if not path.isfile(outfile):
        exit_error(id, 'Output file does not exist: ' + outfile)
    log_info(id, 'Successfully generated: ' + outfile)

def check_dir_exists(id, outfile):
    if not path.isdir(outfile):
        exit_error(id, 'Output directory does not exist: ' + outfile)
    log_info(id, 'Successfully generated: ' + outfile)

def force_remove_dir (id, output_dir):
    try:
       shutil.rmtree(output_dir)
    except OSError as e:
       exit_error(id, 'Failed to remove output directory: ' + output_dir + '. Exception caught: ' + str(e))

    if path.isdir(output_dir):
       exit_error(id, 'Failed to remove output directory: ' + output_dir + '. Directory still exists')

def get_date_long():
    # Thu Jul 15 09:35:39 BST 2021
    time_0 = datetime.datetime.now().replace(microsecond=0)
    return time_0.strftime('%a %b %d %H:%M:%S %Z %Y')

def get_date_short():
    # Thu Jul 15 2021
    time_0 = datetime.datetime.now().replace(microsecond=0)
    return time_0.strftime('%a %b %d %Y')
