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

# Init
XSA=${1:-"ve2302_pcie_qdma_base.xsa"}
PRODUCT=${2:-"rave"}

# Step FW
./scripts/build.sh -os freertos10_xilinx -xsa ../$XSA

# Takes in fpt.json and produces fpt.bin
./scripts/gen_fpt.py \
    -f ./fpt/fpt_${PRODUCT}.json \
    -o build

# Generate PDI w/ bootgen
bootgen \
    -arch versal \
    -image ./fpt/pdi_combine_${PRODUCT}.bif \
    -w -o ./build/ospi_${PRODUCT}.bin

# final pdi generation
./scripts/fpt_pdi_gen.py \
    --fpt ./build/fpt.bin \
    --pdi ./build/ospi_${PRODUCT}.bin \
    --output ./build/ospi_${PRODUCT}_fpt.bin
