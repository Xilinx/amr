#!/usr/bin/env bash

# Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
#
# E.g. Build:
# ./build_all.sh -xsa <xsa>  <product>
#
# This will build bsp (amc_bsp), amc.elf and OSPI images
#
set -Eeuo pipefail
# Build
# ./build.sh <xsa> <product>

# Init
XSA=${1:-"ve2302_pcie_qdma_base.xsa"}
PRODUCT=${2:-"rave"}
FW_DIR=$(realpath ./fw/AMC)
SW_DIR=$(realpath ./sw/AMI)

# Step HW
if [ "$PRODUCT" != "rave" ]; then
    echo "${DESIGN}"
    echo "${PRODUCT}"
    HW_DIR=$(realpath ./hw/amd_rave_gen3x4_25.1)
    mkdir -p ${HW_DIR}/build
    pushd ${HW_DIR}
        vivado -source src/create_design.tcl -source src/build_design.tcl -mode batch -nojournal -log ./build/vivado.log
    popd
fi

# Step FW
pushd ${FW_DIR}
    echo "${FW_DIR}"
    # Builds AMC/OSPI images
    ./build_all.sh ${XSA} ${PRODUCT}
popd

# Generate AMI
pushd ${SW_DIR}
    echo "${SW_DIR}"
    ./scripts/build.sh -profile ${PRODUCT}
    ./scripts/gen_package_amr.py -g -n -f
popd
