Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
SPDX-License-Identifier: MIT

# AMR Management Control (AMC)

Refer to https://pages.gitenterprise.xilinx.com/XBB/AMC/ for the Architecture and API descriptions.

## Overview

The AMR Management Controller (AMC) provides management and control of the AMR on RPU-0. Its basic features include, but are not limited to:

- In-Band Telemetry
- Built in Monitoring
- Host (AMI) communication
- Sensor Control
- QSFP Control (v80 only)
- Download and Programming to Flash

In addition, the AMC is fully abstracted from:

- The OS (Operating System Abstraction Layer (OSAL))
- The Firmware Driver (Firmware Interface Abstraction Layer (FAL))

Event driven architecture is provided by the Event Library (EVL).


---

## Building

### Set-up

Copy source (ssh recommended):
```
git clone git@gitenterprise.xilinx.com:Platform-Management/amr.git
```
cd into top-level of the cloned repo

Enable Xilinx software command-line tools:

### Build

To build from clean:
```
$ ./scripts/build.sh -os <target os (freertos10_xilinx, standalone etc.)> -profile <target profile (v70, v80, rave etc.)> -xsa <path to xsa>
```
-xsa parameter is not required for Linux builds, or if building with the "-amc" option.

This will generate a BSP (on non-Linux builds) using vitis build tools to get all the libraries required by AMC, then build using CMake with the CMakeLists file on the top level.


To see the full list of options:
```
$ ./scripts/build.sh -help
```

A minimum CMake version of 3.5.0 is required therefore you must ensure that this is installed and used in the place where you are building AMC i.e. server, VM or your local machine.

Compiled binary and auto-generated version information will be available:
```
build/
├── amc.elf
├── amc_version.json
```

### Compile AMC separately

```
$ mkdir build
$ cd build
$ cmake ..
$ make
```

### Clean

```
$ rm -rf build/
```

---

## Unit testing

Compile Unit Tests:
```
$ cd build
$ cmake .. -DTEST_ENABLE=true
$ make
```
