#!/bin/bash
set -e

# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

MOD_NAME="$1"
MOD_VER_STR="$2"

if [ "${MOD_NAME}" = "ami" ]; then
    # Unload existing module if loaded
    if lsmod | grep -wq "${MOD_NAME}"; then
        echo "Unloading old ${MOD_NAME} driver"
        rmmod "${MOD_NAME}"
    else
        echo "Module ${MOD_NAME} not loaded. Skipping 'rmmod'."
    fi

    # Remove old Dynamic Kernel Module Support(DKMS) installed versions
    # DKMS status output differs with different versions, so parse accordingly
    dkms_major=$(dkms --version | tr -d " "[a-z-:] | awk -F. '{print $1}')
    if [ "${dkms_major}" -ge 3 ]; then
        MOD_VER_STR_OLD=$(dkms status -m "${MOD_NAME}" | awk -F, '{print $1}' | awk -F/ '{print $2}' | head -n1)
    else
        MOD_VER_STR_OLD=$(dkms status -m "${MOD_NAME}" | awk -F, '{print $2}' | head -n1)
    fi

    if [ -n "${MOD_VER_STR_OLD}" ]; then
        echo "Unregistering old ${MOD_NAME} Linux kernel module sources ${MOD_VER_STR_OLD} from dkms"
        dkms remove -m "${MOD_NAME}" -v "${MOD_VER_STR_OLD}" --all || true
    fi

    # Remove old kernel module files (all compression formats)
    echo "Removing old ${MOD_NAME} kernel module files"
    find /lib/modules -type f -name "${MOD_NAME}.ko*" -delete

    depmod -A

    # Add and install new DKMS module
    echo "Adding ${MOD_NAME} ${MOD_VER_STR} to DKMS"
    if ! dkms add -m "${MOD_NAME}" -v "${MOD_VER_STR}"; then
        echo "ERROR: dkms add failed. Installation of ${MOD_NAME} failed"
        exit 1
    fi

    echo "Installing ${MOD_NAME} ${MOD_VER_STR} with DKMS"
    if ! dkms install -m "${MOD_NAME}" -v "${MOD_VER_STR}"; then
        echo "ERROR: dkms install failed. Installation of ${MOD_NAME} failed"
        exit 1
    fi

    # Load the new module
    echo "INFO: Loading new ${MOD_NAME} Linux kernel module"
    modprobe "${MOD_NAME}"

    # Verify installation
    if ! dkms status -m "${MOD_NAME}" -v "${MOD_VER_STR}" | grep -q installed; then
        echo "ERROR: failed to install ${MOD_NAME} drivers"
        exit 1
    fi

    echo "SUCCESS: ${MOD_NAME} driver ${MOD_VER_STR} installed and loaded"

elif [ "${MOD_NAME}" = "libami" ]; then
    # Remove old library files
    echo "Removing old ${MOD_NAME} library files"
    find /usr/local/lib -type f -name "${MOD_NAME}.so*" -delete

    # Build and install new library
    echo "Building ${MOD_NAME} ${MOD_VER_STR}"
    cd "/usr/src/${MOD_NAME}-${MOD_VER_STR}/api" || exit 1
    echo "Current directory: $(pwd)"
    make clean
    make
    mkdir -p /usr/local/lib/${MOD_NAME}
    install -m 755 build/libami.so /usr/local/lib/${MOD_NAME}
    ldconfig

    echo "SUCCESS: ${MOD_NAME} ${MOD_VER_STR} installed"

else
    # amitool installation
    # Remove old binary
    echo "Removing old ${MOD_NAME} binary"
    find /usr/local/bin -type f -name "${MOD_NAME}" -delete

    # Build and install new binary
    echo "Building ${MOD_NAME} ${MOD_VER_STR}"
    cd "/usr/src/${MOD_NAME}-${MOD_VER_STR}/app" || exit 1
    make clean
    make
    install -m 755 build/ami_tool /usr/local/bin/

    echo "SUCCESS: ${MOD_NAME} ${MOD_VER_STR} installed"
fi

echo ""
echo "Done! If you are not running in bash you may need to refresh your environment before using AMI in the current shell."
echo "If using csh, run 'rehash'. Otherwise, refer to the manual for your shell or start a new session."

