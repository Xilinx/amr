# Copyright (C) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT

cmake_minimum_required( VERSION 3.5.0 )

project( amc )

include( CTest )
enable_testing()

#test setup - repeatable

# add_executable( <testName> <testFileName> <testFilePath> )

# target_link_libraries( <testName>
#                         cmocka
#                         -Wl,--wrap=<wrapperFunctionName>
#                         ...
# )

# add_test( NAME <testName>
#           COMMAND <testName>
# )
