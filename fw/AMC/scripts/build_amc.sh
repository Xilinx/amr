#!/usr/bin/env bash

# Copyright (C) 2025 - 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
#
# E.g. Build:
# ./scripts/build_amc.sh -xsa <xsa>  -profile <product>
#
# This will build bsp (amc_bsp), amc.elf and AMR SPI images
#

set -Eeuo pipefail

# User input variables
PROFILE="rave"
SDT=""
XSA=""
AMC_ONLY=

# Local variables
ROOT_DIR=$(pwd)
BUILD_DIR=$ROOT_DIR/build
BSP_DIR=$ROOT_DIR/amc_bsp
SCRIPTS_DIR=$ROOT_DIR/scripts
BUILD_LOG=$BUILD_DIR/build.log
OUTPUT_BIN="amc.elf"

OS="freertos10_xilinx"
SDT_DIR=$BSP_DIR/"emb_plus_sdt"
CMAKE_PARAMS=" -DDEBUG_BUILD"

STATIC_ANALYSIS=0

SCRIPT_START_TIME=$SECONDS

function print_help() {
    echo "=================================== AMC Build script ===================================="
    echo
    echo "-profile <profile_name> : set the board profile to build (rave|v80)"
    echo "-xsa <abs_path_to_xsa>  : Absolute path to the XSA file to generate BSP from"
    echo "-sdt                    : Absolute path to SDT folder"
    echo "-amc                    : only builds the AMC application (BSP untouched)"
    echo "-analysis               : triggers a static analysis check on AMC files"
    echo
    echo "Any additional arguments are passed directly into CMAKE"
    echo "Note:"
    echo "     Either the XSA or the SDT must be provided"
    echo "     if both XSA and SDT are provided, then the SDT will be used to generate the BSP"
    echo
    echo "E.g.: To build from scratch:"
    echo " ./scripts/build_amc.sh -xsa /direct/path/to/example.xsa -profile <v80|rave>"
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
    -analysis)
        STATIC_ANALYSIS=1
        ;;
    -amc)
        ### Build AMC application only, no BSP changes
        echo "Building AMC application only"
        AMC_ONLY=1
        ;;
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
        if [ -z "$SDT" ]; then
            echo "Error: Invalid sdt"
            exit 1
        fi
        SDT_DIR=${SDT}
        echo "Using sdt=${SDT}"
        ;;
    *)
    esac
    shift ### shift to next passed option ###
done

# Static analysis function
function static_analysis() {
    # Static analysis Clean
    SECTION_START=$SECONDS
    echo "=== Static analysis clean ==="
    cd $BUILD_DIR
    make clean
    rm -rf coverity/

    echo "*** Cleaning took $((SECONDS - $SECTION_START)) S ***" |& tee -a $BUILD_LOG

    # Static Analysis
    echo "=== Static Analysis ===" |& tee -a $BUILD_LOG
    SECTION_START=$SECONDS
    if [ $PROFILE = "v80" ] || [ $PROFILE = "rave" ]; then
        $SCRIPTS_DIR/cov_analysis.sh -b make amc
    else
        ### if unexpected profile for static analysis build ###
        echo "Error: Please specify a valid profile"
    fi
    echo "*** Static analysis took $((SECONDS - $SECTION_START)) S ***" |& tee -a $BUILD_LOG
}

# Clean BSP function
function clean_bsp() {
    echo "Removing all vitis source files"
    rm -rf .Xil
    rm -rf .metadata
    rm -rf .analytics
    rm -rf IDE.log
}

# Build BSP function
function build_bsp() {
    echo "===    Creating BSP    ==="
    cd ..
    mkdir -p ${BSP_DIR}
    cd ${BSP_DIR}

    if [ -z "$SDT" ]; then

        if [ -z "$XSA" ]; then
            echo "Error: Either XSA or SDT must be provided"
            exit 1
        fi
        # Create SDT
        sdtgen -eval "sdtgen set_dt_param -xsa ${XSA} -dir ${SDT_DIR}; generate_sdt"
        # There is a duplicate node entry axi_addr_mask@0, replace it with axi_addr_mask@1
        #sed -i "s/blp_xdma_lite_addr_mask: axi_addr_mask@0 {/blp_xdma_lite_addr_mask: axi_addr_mask@1 {/" ${SDT}/pl.dtsi
        sed -i "s/STATIC_DESIGN_xdma_lite_addr_mask: axi_addr_mask@0 {/STATIC_DESIGN_xdma_lite_addr_mask: axi_addr_mask@1 {/" ${SDT_DIR}/pl.dtsi
    fi

    # Create BSP
    empyro repo -st ${XILINX_VITIS}/data/embeddedsw
    empyro create_bsp -t empty_application -w amc_bsp -s ${SDT_DIR}/system-top.dts -p psv_cortexr5_0 -o freertos
    empyro config_bsp -d amc_bsp -al xilfpga
    empyro config_bsp -d amc_bsp -al xilloader
    empyro config_bsp -d amc_bsp -st freertos freertos_support_static_allocation:true
    empyro config_bsp -d amc_bsp -st freertos freertos_tick_rate:1000
    empyro config_bsp -d amc_bsp -st freertos freertos_total_heap_size:131072
    empyro build_bsp  -d amc_bsp

    sed -i "s/#define XPS_BOARD_EMB-PLUS-VPR-4616/#define XPS_BOARD_EMB_PLUS_VPR_4616/g" amc_bsp/include/xparameters.h
    cd ..
}

# Clean AMC function
function clean_amc() {
    SECTION_START=$SECONDS
    echo "=== Removing BSP, build, and CMake files ==="
    rm -rf $ROOT_DIR/.Xil/
    rm -rf $ROOT_DIR/.metadata/
    rm -rf $BSP_DIR/
    rm -rf $ROOT_DIR/.analytics
    rm -rf $ROOT_DIR/IDE.log

    rm -rf $BUILD_DIR
    echo "*** Cleaning took $((SECONDS - $SECTION_START)) S ***"
}

# Build AMC function
function build_amc() {
    # Remake build direcory
    rm -rf $BUILD_DIR/
    mkdir -p $BUILD_DIR

    ### start initial build ###
    echo "$(date)" |& tee $BUILD_LOG


    ### print out os and profile names ###
    echo "OS path set ==> $OS" |& tee -a $BUILD_LOG
    echo "profile set ==> $PROFILE" |& tee -a $BUILD_LOG

    ### handle xsa file path ###
    if [ "$AMC_ONLY" == 1 ]; then
        ### xsa not required when building the firmware along ###
        echo "Only building $OUTPUT_BIN - skipping BSP step" |& tee -a $BUILD_LOG
    else
        ### print out xsa path ###
        echo ".xsa path set ==> $XSA" |& tee -a $BUILD_LOG
        echo "sdt  path set ==> $SDT" |& tee -a $BUILD_LOG

        ### Removes and regenerates BSP ###
        clean_amc
        clean_bsp
        mkdir -p $BUILD_DIR

        echo "=== Building BSP ===" |& tee -a $BUILD_LOG
        SECTION_START=$SECONDS
        cd $SCRIPTS_DIR

        build_bsp

        cd $ROOT_DIR
        echo "*** Building BSP took $((SECONDS - $SECTION_START)) S ***" |& tee -a $BUILD_LOG
    fi

    cd $BUILD_DIR

    # Running CMake
    echo "=== Executing CMake build process ===" |& tee -a $BUILD_LOG
    SECTION_START=$SECONDS
    cmake -DOS="$OS" -DPROFILE="$PROFILE" "$CMAKE_PARAMS" .. |& tee -a $BUILD_LOG
    echo "*** CMake took $((SECONDS - $SECTION_START)) S ***" |& tee -a $BUILD_LOG

    #Running Make
    echo "=== Compiling $OUTPUT_BIN ===" |& tee -a $BUILD_LOG
    SECTION_START=$SECONDS
    make -j8 |& tee -a $BUILD_LOG
    echo "*** Compiling took $((SECONDS - $SECTION_START)) S ***" |& tee -a $BUILD_LOG

    # static_analysis
    if [ $STATIC_ANALYSIS == 1 ]; then
        static_analysis
    fi

    # Complete
    OUTPUT_DIR=$(realpath .)
    echo "Done - AMC build in $OUTPUT_DIR" |& tee -a $BUILD_LOG
    cd $ROOT_DIR
    echo "*** Complete build time: $((SECONDS - $SCRIPT_START_TIME)) S ***" |& tee -a $BUILD_LOG
}

# Build AMC elf
build_amc

# Takes in board profile and produces fpt.bin
./scripts/gen_fpt.py \
        -p rave \
        -o build


# Generate PDI w/ bootgen
cat << EOF > ./build/amr_ospi_pdi.bif
all:
{
    image { { type=bootimage, file=$(find ${SDT_DIR} -name "*.pdi" | sort | sed -n '1p') } }
    image { id = 0x1c000000, name=rpu_subsystem, delay_handoff
            { core=r5-0, file=./build/amc.elf } }
}
EOF

bootgen \
    -arch versal \
    -image ./build/amr_ospi_pdi.bif \
    -w -o  ./build/amr_ospi.bin

# final pdi generation
./scripts/fpt_pdi_gen.py \
    --fpt    ./build/amr_fpt.bin \
    --pdi    ./build/amr_ospi.bin \
    --output ./build/amr_ospi_fpt.bin
