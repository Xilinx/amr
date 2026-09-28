#!/usr/bin/env bash

# Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Build script for AMI driver packages
#
# Usage:
# ./build.sh                    # Default: kernel driver (legacy)
# ./build.sh -driver ami        # Kernel driver (legacy /dev/ami)
# ./build.sh -driver vfio       # Userspace VFIO driver
#

set -Eeuo pipefail

# Determine script location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SCRIPTS_DIR="$ROOT_DIR/scripts"

# Build output directory created where script is invoked from
CWD="$(pwd)"
BUILD_DIR="$CWD/build_ami"
BUILD_VERSION="3.1.0"

# Default driver profile: ami (kernel)
DRIVER_PROFILE="ami"

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -driver)
            shift
            if [[ -z "${1:-}" ]]; then
                echo "ERROR: -driver requires an argument (ami or vfio)" >&2
                exit 1
            fi
            DRIVER_PROFILE="$1"
            ;;
        -h|-help|--help)
            cat <<-EOF
=================================== AMI Build Script ====================================

  -driver <ami|vfio>    Driver profile to build for (default: ami)
                        ami  = Legacy kernel driver (/dev/ami)
                        vfio = Userspace VFIO driver
  -h, -help, --help     Show this help message

Examples:
  ./build.sh                    # Build for kernel driver (default)
  ./build.sh -driver ami        # Build for kernel driver (explicit)
  ./build.sh -driver vfio       # Build for userspace VFIO driver

=========================================================================================
EOF
            exit 0
            ;;
        *)
            echo "ERROR: Unknown option '$1'" >&2
            echo "Run './build.sh -help' for usage information" >&2
            exit 1
            ;;
    esac
    shift
done

# Validate driver profile
if [[ "$DRIVER_PROFILE" != "ami" && "$DRIVER_PROFILE" != "vfio" ]]; then
    echo "ERROR: Invalid driver profile '$DRIVER_PROFILE'. Must be 'ami' or 'vfio'" >&2
    exit 1
fi

# Convert ami -> kernel, vfio -> userspace for internal use
if [[ "$DRIVER_PROFILE" == "ami" ]]; then
    MAKEFILE_PROFILE="kernel"
    DRIVER_DESC="Legacy Kernel Driver (/dev/ami)"
else
    MAKEFILE_PROFILE="userspace"
    DRIVER_DESC="Userspace VFIO Driver"
fi

# Export for Makefile and packaging scripts to use
export AMI_DRIVER_PROFILE="$DRIVER_PROFILE"
export AMI_MAKEFILE_PROFILE="$MAKEFILE_PROFILE"

echo "=========================================="
echo "AMI Driver Package Build"
echo "=========================================="
echo ""
echo "Driver profile:  $DRIVER_PROFILE ($DRIVER_DESC)"
echo "Build directory: $BUILD_DIR"
echo "Build version:   $BUILD_VERSION"
echo ""

# Both deb and rpm packages are required; verify both toolchains are present.
for tool in dpkg-deb rpmbuild; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: required tool '$tool' not found. Both deb and rpm generation is required." >&2
        exit 1
    fi
done

## Build packages
echo "Cleaning previous build directory..."
rm -rf ${BUILD_DIR}

# Build kernel driver package only for ami profile
if [[ "$DRIVER_PROFILE" == "ami" ]]; then
    for fmt in deb rpm; do
        echo ""
        echo "Building kernel driver package (${fmt})..."
        $SCRIPTS_DIR/gen_pkg_driver.py -f --format ${fmt} -o ${BUILD_DIR}/driver-${fmt}
    done
fi

# Build both deb and rpm packages
for fmt in deb rpm; do
    echo ""
    echo "Building libami package (${fmt}) for ${DRIVER_PROFILE} driver..."
    $SCRIPTS_DIR/gen_pkg_libami.py -f --format ${fmt} -o ${BUILD_DIR}/libami-${fmt} --driver ${DRIVER_PROFILE}

    echo ""
    echo "Building amitool package (${fmt}) for ${DRIVER_PROFILE} driver..."
    $SCRIPTS_DIR/gen_pkg_ami_tool.py -f --format ${fmt} -o ${BUILD_DIR}/ami-${fmt} --driver ${DRIVER_PROFILE}
done

echo ""
echo "Copying packages to build directory..."
cp -rf ${BUILD_DIR}/driver-*/*.deb ${BUILD_DIR}/driver-*/*.rpm ${BUILD_DIR} 2>/dev/null || true
cp -rf ${BUILD_DIR}/libami-*/*.deb ${BUILD_DIR}/libami-*/*.rpm ${BUILD_DIR} 2>/dev/null || true
cp -rf ${BUILD_DIR}/ami-*/*.deb ${BUILD_DIR}/ami-*/*.rpm ${BUILD_DIR} 2>/dev/null || true

echo ""
echo "=========================================="
echo "Build Complete!"
echo "=========================================="
echo ""
echo "Created AMI $DRIVER_DESC packages in: $BUILD_DIR"
echo ""
echo "Packages:"
ls -lh ${BUILD_DIR}/*.deb ${BUILD_DIR}/*.rpm 2>/dev/null || echo "  No packages found"
echo ""

if [[ "$DRIVER_PROFILE" == "vfio" ]]; then
    echo "Note: This is a VFIO-based userspace driver."
    echo "Make sure IOMMU is enabled and vfio-pci driver is available."
else
    echo "Note: This is a legacy kernel driver (/dev/ami)."
    echo "Make sure the ami.ko kernel module is loaded."
fi
echo ""
