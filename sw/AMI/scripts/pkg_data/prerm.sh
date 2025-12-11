#!/bin/bash
set -e

# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.

MOD_NAME="$1"
MOD_VER_STR="$2"

if [ "${MOD_NAME}" == "ami" ]; then
    if lsmod | grep -wq "${MOD_NAME}"; then
        echo "Unloading old ${MOD_NAME} driver"
        rmmod "${MOD_NAME}"
    else
        echo "Module ${MOD_NAME} not loaded. Skipping 'rmmod'."
    fi

    echo "Unregistering ${MOD_NAME} Linux kernel module sources ${MOD_VER_STR} from dkms"
    dkms remove -m "${MOD_NAME}" -v "${MOD_VER_STR}" --all

    if [ $? -ne 0 ]; then
        echo "ERROR: dkms remove failed. Removal of ${MOD_NAME} failed"
        exit 1
    fi

    # Remove kernel module files
    find /lib/modules -type f -name "${MOD_NAME}.ko*" -delete

    # Remove source files from /usr/src
    echo "Removing source files from /usr/src/${MOD_NAME}-${MOD_VER_STR}"
    rm -rf "/usr/src/${MOD_NAME}-${MOD_VER_STR}"

    depmod -A

elif [ "${MOD_NAME}" == "libami" ]; then
    # Remove library files
    find /usr/local/lib -type f -name "${MOD_NAME}.so*" -delete

    # Remove source files
    echo "Removing source files from /usr/src/${MOD_NAME}-${MOD_VER_STR}"
    rm -rf "/usr/src/${MOD_NAME}-${MOD_VER_STR}"

    # Remove include files
    rm -rf "/usr/include/${MOD_NAME}"

else
    # Remove binary
    find /usr/local/bin -type f -name "${MOD_NAME}" -delete

    # Remove source files
    echo "Removing source files from /usr/src/${MOD_NAME}-${MOD_VER_STR}"
    rm -rf "/usr/src/${MOD_NAME}-${MOD_VER_STR}"
fi

