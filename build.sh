#!/usr/bin/env bash

# Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Top-level AMR build script.
# Builds BSP (amc_bsp), amc.elf, OSPI images, and AMI/debian packages.
#
# Usage:
#   ./build.sh -profile <v80|rave>
#   ./build.sh -xsa <path_to_xsa> -profile <v80|rave>
#   ./build.sh -sdt <path_to_sdt> -profile <v80|rave>

set -Eeuo pipefail

XSA="ve2302_xdma_base.xsa"
PROFILE="rave"
USE_XSA=false
USE_SDT=false
SDT=""

# Profile name -> repo-relative path to the Vivado platform directory
declare -A PROFILE_PLATFORM_PATHS=(
	[rave]="emb_plus_ve2302/platforms/ve2302_amr_base"
	[v80]="alveo_v80/platforms/v80_base"
)

print_help() {
	cat <<-EOF
	=================================== AMR Build script ====================================

	  -profile <name>     Profile to build for (${!PROFILE_PLATFORM_PATHS[*]})
	  -xsa <path>         XSA to generate BSP from
	  -sdt <path>         SDT folder path
	  -h, -help, --help   Show this help message

	Examples:
	  ./build.sh -profile rave
	  ./build.sh -sdt /path/to/sdt -profile v80
	  ./build.sh -xsa /path/to/example.xsa -profile rave

	=========================================================================================
	EOF
}

resolve_absolute_path() {
	if [[ "$1" = /* ]]; then
		echo "$1"
	else
		echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
	fi
}

while [[ $# -gt 0 ]]; do
	case "$1" in
	-h|-help|--help)
		print_help
		exit 0
		;;
	-xsa)
		shift
		if [[ -z "${1:-}" ]]; then
			echo "Error: -xsa requires a path argument"
			exit 1
		fi
		XSA="$(resolve_absolute_path "$1")"
		USE_XSA=true
		echo "Using xsa=${XSA}"
		;;
	-profile)
		shift
		if [[ -z "${1:-}" ]]; then
			echo "Error: -profile requires a profile name"
			exit 1
		fi
		PROFILE="$1"
		echo "Using profile=${PROFILE}"
		;;
	-sdt)
		shift
		if [[ -z "${1:-}" ]]; then
			echo "Error: -sdt requires a path argument"
			exit 1
		fi
		SDT="$1"
		USE_SDT=true
		echo "Using sdt=${SDT}"
		;;
	*)
		echo "Warning: Unknown option '$1' (ignored)"
		;;
	esac
	shift
done

if $USE_XSA && $USE_SDT; then
	echo "Error: -xsa and -sdt are mutually exclusive"
	exit 1
fi

# Build HW from source when no XSA or SDT is provided
if ! $USE_XSA && ! $USE_SDT; then
	if [[ -z "${PROFILE_PLATFORM_PATHS[$PROFILE]+x}" ]]; then
		echo "Error: Invalid profile '${PROFILE}'. Supported: ${!PROFILE_PLATFORM_PATHS[*]}"
		exit 1
	fi

	HW_DIR="build_${PROFILE}"
	rm -rf "${HW_DIR}"
	mkdir -p "${HW_DIR}"
	pushd "${HW_DIR}"

	#TODO: Add support for github repo matching V80_SRC_REV
	platform_path="${PROFILE_PLATFORM_PATHS[$PROFILE]}"
	git clone https://gitenterprise.xilinx.com/PAEG/amr_vivado_designs.git
	cd "amr_vivado_designs/${platform_path}"
	make xsa
	XSA="$(realpath "project/$(basename "${platform_path}").xsa")"
	USE_XSA=true

	popd
fi

# Build OSPI and AMC FW
if $USE_XSA; then
	./fw/AMC/scripts/build.sh -xsa "${XSA}" -profile "${PROFILE}"
else
	./fw/AMC/scripts/build.sh -sdt "${SDT}" -profile "${PROFILE}"
fi

# Build AMI/debian packages
./sw/AMI/scripts/build.sh
