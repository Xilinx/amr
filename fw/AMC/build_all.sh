#!/usr/bin/env bash

# Copyright (C) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
#
# E.g. Build:
# ./build_all.sh -xsa <xsa>  -profile <product>
#
# This will build bsp (amc_bsp), amc.elf and OSPI images
#

set -Eeuo pipefail

# Init
PROFILE="rave"
SDT="emb_plus_sdt"
XSA="ve2302_pcie_qdma.xsa"

function print_help() {
	echo "=================================== AMC Build script ===================================="
	echo
	echo "-profile <profile_name> : set the profile to build for (v70/v80/Linux/rave, etc)"
	echo "-xsa <abs_path_to_xsa>  : XSA to generate BSP from"
	echo "-sdt                    : SDT folder name"
	echo
	echo "Any additional arguments are passed directly into CMAKE"
	echo
	echo "E.g.: To build from scratch:"
	echo " ./scripts/build_all.sh -xsa /direct/path/to/example.xsa -profile <v80|rave>"
	echo
	echo "========================================================================================="
}

### Script Starting Point  ###

### handle options ###

### while-done structure defines a loop ###
### that executes once for each passed option ###
while [ $# -gt 0 ]; do
	### case-esac structure evaluates each option ###
	case "$1" in
	-help)
		print_help
		exit 0;;
	-xsa)
		shift  ### shift to next passed variable (-xsa *) ###
		XSA=$1 ### store option into xsa variable ###

		### handle empty string ###
		if [ "$1" = "" ]; then
			echo "Error: Invalid xsa"
			exit 1
		fi
		echo "Using xsa=${XSA}"
		;;
	-profile)
		shift
		PROFILE=$1
		echo "Using profile=${PROFILE}"
		;;
	-sdt)
		shift
		SDT=$1
		echo "Using sdt=${SDT}"
		;;
	*)
	esac
	shift ### shift to next passed option ###
done

# Step FW
./scripts/build.sh -os freertos10_xilinx -xsa $XSA -sdt $SDT

# Takes in fpt.json and produces fpt.bin
./scripts/gen_fpt.py \
	-f ./fpt/fpt_${PROFILE}.json \
	-o build

# Generate PDI w/ bootgen
cp ./fpt/pdi_combine.bif ./build
sed -i "s|.pdi|$(find ./amc_bsp/${SDT} -name *.pdi)|" \
	./build/pdi_combine.bif

bootgen \
	-arch versal \
	-image ./build/pdi_combine.bif \
	-w -o  ./build/amc_ospi.bin

# final pdi generation
./scripts/fpt_pdi_gen.py \
	--fpt    ./build/amc_fpt.bin \
	--pdi    ./build/amc_ospi.bin \
	--output ./build/amc_ospi_fpt.bin

