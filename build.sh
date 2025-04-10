#!/usr/bin/env bash

# Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
#
# E.g. Build:
# ./build.sh -xsa <xsa> -profile <v80|rave>
#
# This will build bsp (amc_bsp), amc.elf and OSPI images
#

set -Eeuo pipefail

# Init
XSA="ve2302_pcie_qdma_base.xsa"
PROFILE="rave"
FW_DIR=$(realpath ./fw/AMC)
SW_DIR=$(realpath ./sw/AMI)

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
	echo " ./scripts/build.sh -xsa /direct/path/to/example.xsa -profile <v80|rave>"
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

# Step HW
if [ "$PROFILE" != "rave" ]; then
	echo "${PROFILE}"
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
	./build_all.sh -xsa ${XSA} -profile ${PROFILE}
popd

# Generate AMI
pushd ${SW_DIR}
	echo "${SW_DIR}"
	./scripts/build.sh -profile ${PROFILE}
	./scripts/gen_package_amr.py -g -n -f
popd
