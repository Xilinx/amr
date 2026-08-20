#!/bin/bash -e

# Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT

if [[ "${BASH_SOURCE[0]}" != "${0}" ]]
then
    echo "ERROR: Sourcing script is not supported - exiting"
else

    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

    # Tool versions
    VITIS_VERSION="2026.1"
    COVERITY_VERSION="2025.3.0"

    Usage()
    {
        echo "Usage: $(basename $0) -c <component> [-a <level>]"
        echo "       [-xsa <path> | -sdt <dir>] [-profile <name>] [-h]"
        echo
        echo "Run Coverity static analysis on AMR components."
        echo
        echo "OPTIONS:"
        echo "  -c <component>  Component to analyze (required)"
        echo "                    amc        - AMC Firmware"
        echo "                    ami_api    - AMI API Library"
        echo "                    ami_app    - AMI App (CLI tool)"
        echo "                    ami_driver - AMI Kernel Driver"
        echo "                    all        - All components"
        echo "  -a <level>      Aggressiveness level: low | medium | high (default: low)"
        echo "  -xsa <path>     XSA to build the AMC firmware from (amc/all only)"
        echo "  -sdt <dir>      SDT folder to build the AMC firmware from (amc/all only)"
        echo "  -profile <name> Board profile: rave | v80 (required with -xsa/-sdt)"
        echo "  -h              Display this help and exit"
        echo
        exit 1
    }

    AGGRESSIVENESS="low"
    COMPONENT=""
    XSA=""
    SDT=""
    PROFILE=""

    while [[ $# -gt 0 ]]; do
        case "$1" in
            -h|-help|--help)
                Usage
                ;;
            -c|-a|-xsa|-sdt|-profile)
                if [[ $# -lt 2 ]]; then
                    echo "Error: $1 requires an argument" >&2
                    exit 1
                fi
                case "$1" in
                    -c)       COMPONENT="$2" ;;
                    -a)       AGGRESSIVENESS="$2" ;;
                    -xsa)     XSA="$2" ;;
                    -sdt)     SDT="$2" ;;
                    -profile) PROFILE="$2" ;;
                esac
                shift 2
                ;;
            *)
                echo "Error: Unknown option \"$1\"" >&2
                echo "" >&2
                Usage >&2
                ;;
        esac
    done

    if [[ -z "$COMPONENT" ]]; then
        echo "Error: -c <component> is required"
        echo ""
        Usage >&2
    fi

    case "$AGGRESSIVENESS" in
        "low") ;;
        "medium") ;;
        "high") ;;
        *)
            echo "Error: Invalid aggressiveness level \"$AGGRESSIVENESS\""
            Usage >&2
            ;;
    esac

    case "$COMPONENT" in
        amc|ami_api|ami_app|ami_driver|all) ;;
        *)
            echo "Error: Invalid component \"$COMPONENT\". Must be one of: amc, ami_api, ami_app, ami_driver, all"
            exit 1
            ;;
    esac

    # Validate the build inputs -xsa/-sdt/-profile
    if [[ -n "$XSA" && -n "$SDT" ]]; then
        echo "Error: -xsa and -sdt are mutually exclusive" >&2
        exit 1
    fi

    BUILD_AMC=0
    if [[ -n "$XSA" || -n "$SDT" ]]; then
        BUILD_AMC=1
    fi

    if [[ $BUILD_AMC -eq 1 && "$COMPONENT" != "amc" && "$COMPONENT" != "all" ]]; then
        echo "Error: -xsa/-sdt only apply to \"-c amc\" or \"-c all\"" >&2
        exit 1
    fi

    # Resolve to absolute paths now - the build and analysis steps change directory.
    if [[ -n "$XSA" ]]; then
        if [[ ! -f "$XSA" ]]; then
            echo "Error: XSA file not found: $XSA" >&2
            exit 1
        fi
        XSA="$(realpath "$XSA")"
    fi

    if [[ -n "$SDT" ]]; then
        if [[ ! -d "$SDT" ]]; then
            echo "Error: SDT directory not found: $SDT" >&2
            exit 1
        fi
        SDT="$(realpath "$SDT")"
    fi

    # Source option passed through to the AMC build script
    BUILD_OPT=()
    if [[ -n "$XSA" ]]; then
        BUILD_OPT=(-xsa "$XSA")
    elif [[ -n "$SDT" ]]; then
        BUILD_OPT=(-sdt "$SDT")
    fi

    # Validate the profile is one of rave or v80
    if [[ $BUILD_AMC -eq 1 ]]; then
        if [[ -z "$PROFILE" ]]; then
            echo "Error: -profile is required when building AMC with -xsa/-sdt" >&2
            exit 1
        fi
        case "$PROFILE" in
            rave|v80) ;;
            *)
                echo "Error: Invalid profile \"$PROFILE\". Must be one of: rave, v80" >&2
                exit 1
                ;;
        esac
    elif [[ -n "$PROFILE" ]]; then
        echo "Error: -profile only applies when building AMC with -xsa or -sdt" >&2
        exit 1
    fi

    # Setup build environment - only AMC needs the Vitis toolchain
    if [[ "$COMPONENT" == "amc" || "$COMPONENT" == "all" ]]; then
        VITIS_SETTINGS="/proj/xbuilds/${VITIS_VERSION}_daily_latest/installs/lin64/${VITIS_VERSION}/Vitis/settings64.sh"
        if [[ -f "$VITIS_SETTINGS" ]]; then
            source "$VITIS_SETTINGS"
        else
            echo "Error: Vitis settings not found at $VITIS_SETTINGS"
            exit 1
        fi
    fi

    export PATH=/tools/batonroot/coverity/${COVERITY_VERSION}/bin/:$PATH

    # Verify Coverity tools are available
    if ! command -v cov-analyze &>/dev/null; then
        echo "Error: cov-analyze not found in PATH."
        echo "       Coverity version ${COVERITY_VERSION} may not be installed."
        echo "       Expected at: /tools/batonroot/coverity/${COVERITY_VERSION}/bin/"
        exit 1
    fi

    # Tracks per-component results for the summary table
    SUMMARY_COMPONENTS=()
    SUMMARY_PROFILES=()
    SUMMARY_DEFECTS=()
    SUMMARY_REPORTS=()

    # Set when any component fails, so the summary still prints for the others
    FAILED=0

    # =========================================================================
    # Count defects in the generated report - the intermediate directory is not
    # used here, as it still holds the entries excluded by cov-format-errors
    # =========================================================================
    count_defects()
    {
        grep -o '<error>' "$1" 2>/dev/null | wc -l
    }

    # =========================================================================
    # Run Coverity analysis on a single component
    # =========================================================================
    run_analysis()
    {
        local comp_name="$1"
        local build_dir="$2"
        local compiler="$3"
        local build_cmd="$4"
        local profile="${5:-}"
        local output_dir="$REPO_ROOT/coverity_output/$comp_name"

        if [[ ! -d "$build_dir" ]]; then
            echo ""
            echo "Error: Build directory not found: $build_dir" >&2
            return 1
        fi

        echo ""
        echo "============================================================"
        echo "  Coverity Static Analysis"
        echo "  Component:       $comp_name"
        if [[ -n "$profile" ]]; then
            echo "  Profile:         $profile"
        fi
        echo "  Aggressiveness:  $AGGRESSIVENESS"
        echo "  Compiler:        $compiler"
        echo "  Build dir:       $build_dir"
        echo "  Build command:   $build_cmd"
        echo "  Output:          $output_dir"
        echo "============================================================"
        echo ""

        mkdir -p "$output_dir" || return 1
        cd "$build_dir" || return 1

        local cov_dir="$output_dir/cov_intermediate"
        rm -rf "$cov_dir"

        local config="$cov_dir/config.xml"
        cov-configure --config "$config" --compiler "$compiler" --comptype gcc --template || return 1

        cov-build --emit-complementary-info --config "$config" --dir "$cov_dir" $build_cmd || return 1

        cov-analyze --dir "$cov_dir" --all \
            --aggressiveness-level "$AGGRESSIVENESS" \
            --enable-parse-warnings \
            --enable-default \
            --enable-exceptions \
            --enable-fnptr \
            --config "$config" || return 1

        cov-format-errors --exclude-files "(\.pb.*|/amc_bsp/|/linux-headers-)" --dir "$cov_dir" \
            --html-output "$output_dir/html" || return 1

        rm -f "$cov_dir/build-log.txt"

        local defect_count
        defect_count=$(count_defects "$output_dir/html/index.xml")

        SUMMARY_COMPONENTS+=("$comp_name")
        SUMMARY_PROFILES+=("${profile:--}")
        SUMMARY_DEFECTS+=("$defect_count")
        SUMMARY_REPORTS+=("$output_dir/html/index.html")
    }

    build_amc_fw()
    {
        echo ""
        echo "============================================================"
        echo "  Building AMC firmware"
        echo "  Profile:  $PROFILE"
        echo "  Source:   ${BUILD_OPT[0]} ${BUILD_OPT[1]}"
        echo "============================================================"
        echo ""

        ( cd "$REPO_ROOT" && ./fw/AMC/scripts/build.sh "${BUILD_OPT[@]}" -profile "$PROFILE" )
    }

    analyze_amc()
    {
        local build_dir="$REPO_ROOT/build_amc"
        local profile

        if [[ $BUILD_AMC -eq 1 ]]; then
            build_amc_fw || return 1
        elif [[ ! -f "$build_dir/Makefile" || ! -f "$build_dir/CMakeCache.txt" ]]; then
            echo ""
            echo "Error: No AMC build found at $build_dir" >&2
            echo "       Provide an XSA (or SDT) and profile to build it first:" >&2
            echo "         $(basename $0) -c amc -xsa <path> -profile <rave|v80>" >&2
            if [[ -d "$REPO_ROOT/fw/AMC/build_amc" ]]; then
                echo "       Note: fw/AMC/build_amc exists but is not used - AMC analysis" >&2
                echo "             uses the build tree at the repo root." >&2
            fi
            return 1
        fi

        # Report which board variant the tree was configured for, since the
        # profile decides which src/profiles sources are compiled.
        profile=$(grep '^PROFILE:' "$build_dir/CMakeCache.txt" | cut -d '=' -f 2)

        run_analysis "amc" \
            "$build_dir" \
            "armr5-none-eabi-gcc" \
            "make clean amc.elf" \
            "$profile"
    }

    analyze_ami_api()
    {
        run_analysis "ami_api" \
            "$REPO_ROOT/sw/AMI/api" \
            "cc" \
            "make clean all"
    }

    analyze_ami_app()
    {
        # The app links against libami.so, which the packaging scripts clean up
        # after themselves. Build it outside cov-build so the API sources stay
        # out of the app's report.
        make -C "$REPO_ROOT/sw/AMI/api" all || return 1

        run_analysis "ami_app" \
            "$REPO_ROOT/sw/AMI/app" \
            "cc" \
            "make clean all"
    }

    analyze_ami_driver()
    {
        run_analysis "ami_driver" \
            "$REPO_ROOT/sw/AMI/driver" \
            "gcc" \
            "make"
    }

    # =========================================================================
    # Print summary table
    # =========================================================================
    print_summary()
    {
        echo ""
        echo "============================================================"
        echo "  Summary"
        echo "============================================================"
        printf "  %-12s %-9s %-9s %s\n" "Component" "Profile" "Defects" "Report"
        printf "  %-12s %-9s %-9s %s\n" "---------" "-------" "-------" "------"
        for i in "${!SUMMARY_COMPONENTS[@]}"; do
            printf "  %-12s %-9s %-9s %s\n" \
                "${SUMMARY_COMPONENTS[$i]}" \
                "${SUMMARY_PROFILES[$i]}" \
                "${SUMMARY_DEFECTS[$i]}" \
                "${SUMMARY_REPORTS[$i]}"
        done
        echo "============================================================"
    }

    # =========================================================================
    # Run one component, recording a failure instead of aborting the whole run
    # =========================================================================
    run_component()
    {
        local comp_name="$1"

        if ! "analyze_$comp_name"; then
            SUMMARY_COMPONENTS+=("$comp_name")
            SUMMARY_PROFILES+=("-")
            SUMMARY_DEFECTS+=("FAILED")
            SUMMARY_REPORTS+=("-")
            FAILED=1
        fi
    }

    # =========================================================================
    # Main
    # =========================================================================
    COMPONENTS=("$COMPONENT")
    if [[ "$COMPONENT" == "all" ]]; then
        # ami_api runs before ami_app so libami.so is already in place
        COMPONENTS=(amc ami_api ami_app ami_driver)
    fi

    for comp in "${COMPONENTS[@]}"; do
        run_component "$comp"
    done

    print_summary
    exit $FAILED
fi
