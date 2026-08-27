#!/bin/bash
set -e

# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.

MOD_NAME="$1"
MOD_VER_STR="$2"
DRIVER_PROFILE="$3"

# Default to kernel driver if not specified (for backwards compatibility)
if [ -z "${DRIVER_PROFILE}" ]; then
    DRIVER_PROFILE="kernel"
fi

echo "Preparing to install ${MOD_NAME} ${MOD_VER_STR} (driver profile: ${DRIVER_PROFILE})"

# Check for conflicting XRT installation (deb-based systems only; RPM handles this via Conflicts:)
if command -v dpkg >/dev/null 2>&1 && dpkg -l | grep -qE "^ii[[:space:]]+xrt([[:space:]]|:)"; then
    echo "ERROR: XRT is installed. AMI conflicts with XRT."
    echo "Please remove XRT before installing AMI:"
    echo "  sudo apt-get remove xrt"
    exit 1
fi

# For userspace VFIO driver, unbind from old kernel drivers
if [ "${MOD_NAME}" = "amitool" ] && [ "${DRIVER_PROFILE}" = "userspace" ]; then
    echo "Preparing for VFIO userspace driver installation..."

    # Stop any running ami_tool processes
    if pgrep -x ami_tool >/dev/null 2>&1; then
        echo "Stopping running ami_tool processes"
        pkill -TERM -x ami_tool 2>/dev/null || true
        sleep 1
        pkill -KILL -x ami_tool 2>/dev/null || true
    fi

    # Unbind Alveo devices (Xilinx vendor 0x10ee, processing-accelerator class
    # 0x1200xx) from the old ami/ami_uio/xdma kernel drivers so vfio-pci can claim them.
    for dev in /sys/bus/pci/devices/*; do
        [ -r "$dev/vendor" ] && [ -r "$dev/class" ] || continue
        [ "$(cat "$dev/vendor")" = "0x10ee" ] || continue
        case "$(cat "$dev/class")" in
            0x1200*) ;;
            *) continue ;;
        esac
        [ -L "$dev/driver" ] || continue
        drv=$(basename "$(readlink -f "$dev/driver")")
        if [ "$drv" = "ami" ] || [ "$drv" = "ami_uio" ] || [ "$drv" = "xdma" ]; then
            bdf=$(basename "$dev")
            echo "Unbinding $bdf from $drv"
            echo "$bdf" > "/sys/bus/pci/drivers/$drv/unbind" 2>/dev/null || true
        fi
    done

    # Give time for unbind to complete before unloading modules
    sleep 1

    # Unload old kernel modules if loaded
    for mod in ami ami_uio xdma; do
        if lsmod | grep -q "^${mod} "; then
            echo "Unloading old kernel module: $mod"
            rmmod "$mod" 2>/dev/null || true
        fi
    done
fi

echo "Pre-installation checks passed"
