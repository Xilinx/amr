#!/usr/bin/env bash

# Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
#
# E.g. Build:
# ./build_ami.sh
#     Default build directory is ./build_ami
# This will build AMR x86 debian packages
#

set -Eeuo pipefail

# Determine script location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SCRIPTS_DIR="$ROOT_DIR/scripts"

# Build output directory created where script is invoked from
CWD="$(pwd)"
BUILD_DIR="$CWD/build_ami"
BUILD_VERSION="2.0.0"


## Build packages
rm -rf ${BUILD_DIR}
$SCRIPTS_DIR/gen_pkg_driver.py   -f -o ${BUILD_DIR}/driver
$SCRIPTS_DIR/gen_pkg_libami.py   -f -o ${BUILD_DIR}/libami
$SCRIPTS_DIR/gen_pkg_ami_tool.py -f -o ${BUILD_DIR}/ami

cp -rf ${BUILD_DIR}/driver/*.deb ${BUILD_DIR}/driver/*.rpm ${BUILD_DIR} 2>/dev/null || true
cp -rf ${BUILD_DIR}/libami/*.deb ${BUILD_DIR}/libami/*.rpm ${BUILD_DIR} 2>/dev/null || true
cp -rf ${BUILD_DIR}/ami/*.deb    ${BUILD_DIR}/ami/*.rpm    ${BUILD_DIR} 2>/dev/null || true

echo "Created AMR debian packages in: $BUILD_DIR"
