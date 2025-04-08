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
    -w -o ./build/amc_ospi.bin

# final pdi generation
./scripts/fpt_pdi_gen.py \
    --fpt ./build/amc_fpt.bin \
    --pdi ./build/amc_ospi.bin \
    --output ./build/amc_ospi_fpt.bin
