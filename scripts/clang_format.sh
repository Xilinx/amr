#!/bin/bash -e

# Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Format (or check the formatting of) AMC C sources, by default those under
# fw/AMC/src, using clang-format and the style in scripts/clang-format.yaml.
#
# The style file is passed to clang-format explicitly (--style=file:<path>) so
# it applies no matter where this script is invoked from, and without relying
# on a .clang-format file being present in an ancestor of the source tree.


if [[ "${BASH_SOURCE[0]}" != "${0}" ]]
then
    echo "ERROR: Sourcing script is not supported - exiting"
else

    # Without pipefail, a failing 'find' in the 'find | xargs' pipelines is
    # masked by xargs' success, so a formatting run could silently no-op.
    set -o pipefail

    # Different clang-format majors format this tree slightly differently, so a
    # too-old binary is rejected rather than silently reformatting everything.
    # 18 is the version the tree is formatted with.
    CLANG_FORMAT_MIN_VERSION=18
    MAX_FORMAT_PASSES=5

    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"   # repo root
    SRC_DIR="$ROOT_DIR/fw/AMC/src"
    STYLE_FILE="$SCRIPT_DIR/clang-format.yaml"

    # Prints usage to stdout. Callers decide the exit status: an explicit -h is
    # success, an argument error is a failure that redirects this to stderr.
    Usage()
    {
        local self
        self=$(basename "$0")
        echo "Usage: $self [-h] [-c] [-f <file> | -d <dir>]"
        echo
        echo "-h    Display usage instructions and exit"
        echo "-c    Check formatting only: dry-run, non-zero exit if changes needed"
        echo "-f    Operate on a single file only"
        echo "-d    Operate on all *.c/*.h under a directory, recursively"
        echo
        echo "With no options the files are rewritten in place."
        echo "With neither -f nor -d the default directory is fw/AMC/src."
        echo
        echo "E.g.: Reformat tree:   $self"
        echo "      Reformat a dir:  $self -d fw/AMC/src/apps/asdm"
        echo "      Reformat a file: $self -f fw/AMC/src/apps/asdm/asdm.c"
        echo "      Check a dir:     $self -c -d fw/AMC/src/proxy_drivers"
    }

    MODE="write"
    FILE=
    DIR=

    while getopts ":cd:f:h" opt
    do
        case "${opt}" in
            h)
                Usage
                exit 0
                ;;
            c)
                MODE="check"
                ;;
            f)
                FILE=$OPTARG
                ;;
            d)
                DIR=$OPTARG
                ;;
            \?)
                echo "Invalid option: -$OPTARG" >&2
                exit 1
                ;;
            :)
                echo "Option -$OPTARG requires an argument." >&2
                exit 1
                ;;
            *)
                Usage >&2
                exit 1
                ;;
        esac
    done

    # Empty (rather than a failure) when the binary is missing or unparseable,
    # so that callers can report a useful error instead of -e aborting silently.
    CfMajorVersion()
    {
        "$1" --version 2>/dev/null | grep -oE 'version [0-9]+' | grep -oE '[0-9]+' || true
    }

    # clang-format must be present and recent enough for the .clang-format options.
    # Set $CLANG_FORMAT to use a binary other than the one on $PATH.
    if [[ -z "$CLANG_FORMAT" ]] && ! CLANG_FORMAT=$(command -v clang-format)
    then
        echo "Error: clang-format not found (>= $CLANG_FORMAT_MIN_VERSION required)" >&2
        echo "       Set \$CLANG_FORMAT or add clang-format to \$PATH" >&2
        exit 1
    fi

    CF_VERSION=$(CfMajorVersion "$CLANG_FORMAT")
    if [[ -z "$CF_VERSION" || "$CF_VERSION" -lt "$CLANG_FORMAT_MIN_VERSION" ]]
    then
        echo "Error: clang-format >= $CLANG_FORMAT_MIN_VERSION required (found ${CF_VERSION:-unknown} at $CLANG_FORMAT)" >&2
        exit 1
    fi

    # Without the style file clang-format silently falls back to LLVM style,
    # which would rewrite every source file to the wrong style.
    if [[ ! -f "$STYLE_FILE" ]]
    then
        echo "Error: .clang-format not found: $STYLE_FILE" >&2
        exit 1
    fi

    if [[ -n "$FILE" && -n "$DIR" ]]
    then
        echo "Error: -f and -d are mutually exclusive" >&2
        exit 1
    fi

    if [[ -n "$FILE" && ! -f "$FILE" ]]
    then
        echo "Error: file not found: $FILE" >&2
        exit 1
    fi

    TARGET_DIR=${DIR:-$SRC_DIR}

    # A wrong directory would otherwise format nothing and still exit 0, silently
    # turning a CI formatting gate into a no-op.
    if [[ -z "$FILE" && ! -d "$TARGET_DIR" ]]
    then
        echo "Error: directory not found: $TARGET_DIR" >&2
        exit 1
    fi

    if [[ "$MODE" == "check" ]]
    then
        CF_ARGS=(--dry-run --Werror)
    else
        CF_ARGS=(-i)
    fi
    CF_ARGS+=("--style=file:$STYLE_FILE")

    RunClangFormat()
    {
        if [[ -n "$FILE" ]]
        then
            "$CLANG_FORMAT" "$@" "$FILE"
        else
            find "$TARGET_DIR" -type f \( -name '*.c' -o -name '*.h' \) -print0 \
                | xargs -0 -r "$CLANG_FORMAT" "$@"
        fi
    }

    if [[ "$MODE" == "check" ]]
    then
        RC=0
        RunClangFormat "${CF_ARGS[@]}" || RC=1
        exit $RC
    else
        # A single pass is not always a fixed point: a wrapped string literal can
        # land on an indent that the next pass revises. Repeat until a pass leaves
        # nothing to change.
        SETTLED=0
        for (( PASS = 1; PASS <= MAX_FORMAT_PASSES; PASS++ ))
        do
            RunClangFormat "${CF_ARGS[@]}"

            if RunClangFormat --dry-run --Werror "--style=file:$STYLE_FILE" 2>/dev/null
            then
                SETTLED=1
                echo "Formatting settled after $PASS pass(es)"
                break
            fi
        done

        if [[ "$SETTLED" -ne 1 ]]
        then
            echo "Error: formatting did not settle after $MAX_FORMAT_PASSES passes" >&2
            exit 1
        fi
        exit 0
    fi
fi