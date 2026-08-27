#!/bin/bash
set -e

# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.

MOD_NAME="$1"
MOD_VER_STR="$2"

echo "Removing ${MOD_NAME} ${MOD_VER_STR}"

if [ "${MOD_NAME}" = "ami" ]; then
    # Remove vfio-pci udev rules when removing ami kernel driver
    if [ -f /etc/udev/rules.d/90-ami-vfio.rules ]; then
        echo "Removing vfio-pci udev rules"
        rm -f /etc/udev/rules.d/90-ami-vfio.rules
        if command -v udevadm >/dev/null 2>&1; then
            udevadm control --reload-rules 2>/dev/null || true
        fi
    fi
elif [ "${MOD_NAME}" = "libami" ]; then
    echo "Removing source files from /usr/src/${MOD_NAME}-${MOD_VER_STR}"
    rm -rf "/usr/src/${MOD_NAME}-${MOD_VER_STR}"
elif [ "${MOD_NAME}" = "amitool" ]; then
    echo "Removing source files from /usr/src/${MOD_NAME}-${MOD_VER_STR}"
    rm -rf "/usr/src/${MOD_NAME}-${MOD_VER_STR}"

    # Remove vfio-pci udev rules and systemd service when removing amitool with userspace driver
    # Only remove if this was a VFIO installation (check for recovery tool)
    if [ -f /usr/local/sbin/ami-vfio-recover ]; then
        echo "Removing vfio-pci udev rules and services"
        rm -f /etc/udev/rules.d/90-ami-vfio.rules
        rm -f /etc/systemd/system/ami-vfio-recover.service
        rm -f /usr/local/sbin/ami-vfio-recover
        if command -v systemctl >/dev/null 2>&1; then
            systemctl daemon-reload 2>/dev/null || true
        fi
        if command -v udevadm >/dev/null 2>&1; then
            udevadm control --reload-rules 2>/dev/null || true
        fi
    fi
fi

echo "Pre-removal complete"
