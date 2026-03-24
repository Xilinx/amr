#!/usr/bin/env bash

# Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
#
# E.g. Build:
# ./build.sh -xsa <xsa> -profile <v80|rave>
#
# This will build bsp (amc_bsp), amc.elf and OSPI images for the given profile
#

set -Eeuo pipefail

# Init
XSA="ve2302_xdma_base.xsa"
PROFILE="rave"
HW_DIR=build_$PROFILE
USE_XSA=false
USE_SDT=false

function print_help() {
	echo "=================================== AMR Build script ===================================="
	echo
	echo "-profile <profile_name> : set the profile to build for (rave/v80, etc)"
	echo "-xsa <path_to_xsa>      : XSA to generate BSP from"
	echo "-sdt                    : SDT folder name"
	echo
	echo "Any additional arguments are passed directly into CMAKE"
	echo
	echo "E.g.: To build:"
	echo " ./scripts/build.sh -profile <v80|rave>"
	echo " ./scripts/build.sh -sdt /direct/path/to/sdt -profile <v80|rave>"
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
		shift
		USE_XSA=true

		if [ "$1" = "" ]; then
			echo "Error: Invalid xsa"
			exit 1
		fi

		if [[ "$1" = /* ]]; then
			XSA="$1"
		else
			XSA="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
		fi
		echo "Using xsa=${XSA}"
		;;
	-profile)
		shift
		PROFILE=$1
		HW_DIR=build_$PROFILE
		echo "Using profile=${PROFILE}"
		;;
	-sdt)
		shift
		SDT=$1
		USE_SDT=true
		echo "Using sdt=${SDT}"
		;;
	*)
	esac
	shift ### shift to next passed option ###
done

# Check for mutually exclusive options
if $USE_XSA && $USE_SDT; then
	echo "Error: -xsa and -sdt are mutually exclusive"
	exit 1
fi

# Build HW if no XSA or SDT is provided
if ! $USE_XSA && ! $USE_SDT; then
	rm -rf ${HW_DIR}
	mkdir -p ${HW_DIR}
	pushd ${HW_DIR}

	if [ "$PROFILE" == "rave" ]; then
		#TODO: Add support for Rave
		vivado -source src/create_design.tcl -source src/build_design.tcl -mode batch -nojournal -log ./build/vivado.log
	elif [ "$PROFILE" == "v80" ]; then
		#TODO: Add support for github repo matching V80_SRC_REV
		git clone https://gitenterprise.xilinx.com/PAEG/amr_vivado_designs.git
		cd amr_vivado_designs/alveo_v80/platforms/v80_base
		make xsa
		XSA="$(realpath project/v80_base.xsa)"
		USE_XSA=true
	else
		echo "Invalid profile"
		exit 1
	fi
	popd
fi

# Build OSPI and AMC FW
if $USE_XSA; then
	./fw/AMC/scripts/build.sh -xsa ${XSA} -profile ${PROFILE}
else
	./fw/AMC/scripts/build.sh -sdt ${SDT} -profile ${PROFILE}
fi

# Build AMI/debian packages
./sw/AMI/scripts/build.sh
