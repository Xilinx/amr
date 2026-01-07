##### Copyright (C) 2025 - 2026 Advanced Micro Devices, Inc.  All rights reserved.

##### SPDX-License-Identifier: MIT

# AMR - Adaptive Management Runtime

[![License](https://img.shields.io/badge/license-MIT-green)](./LICENSE)
[![Introduction](https://img.shields.io/badge/-1._Introduction-informational)](#1-introduction)
[![Directory Structure](https://img.shields.io/badge/-2._Directory_Structure-bluegreen)](#2-directory-structure)
[![Build Instructions](https://img.shields.io/badge/-3._Build_Instructions-critical)](#3-build-instructions)
[![Test](https://img.shields.io/badge/-4._Test-important)](#4-test)
[![Glossary](https://img.shields.io/badge/-5._Glossary-yellow)](#5-glossary)
[![References](https://img.shields.io/badge/-6._References-lightgrey)](#6-references)

### 1. Introduction

AMR (Adaptive Management Runtime) provides basic management capabilities for Alveo and Embedded+ boards. These boards feature a Versal device connected to an x86 host via PCIe. The Versal device is programmed with a PDI stored in OSPI flash and boots normally. Any updates to the OSPI are performed through the PCIe interface.

Two types of Versal devices are supported: one with CPM and another with PL PCIe IP. The Versal design contains the basic PCIe configuration information for shared memory communication. RPU0 serves as the device Adaptive Management Controller (AMC). The OSPI flash contains three partitions: the first two contain boot PDIs, and the third is used for storing user designs. On power-up, the boot ROM loads the PDI from the OSPI flash selected partition and runs AMC on the RPU0 core (which is part of the boot PDIs). The x86 host enumerates the Versal PCIe device and configures it. Once x86 configuration is complete, communication is established between the x86 Adaptive Management Interface (AMI) and AMC.

To build sample designs from source code in this repository, you will need the following tools installed. Follow the [build instructions](#3-build-instructions) for details:

- A Linux-based host OS with AMD tools installed (requires approximately 50GB free disk space)
- [Vivado][1] 2026.1

[1]: https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/vivado-design-tools.html
[2]: https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/embedded-design-tools.html

**VPR-4616 Board:**

![VPR-4616 Board](https://www.amd.com/content/dam/amd/en/images/products/som/2474370-sapphire-edge-vpr-4616.png)

### 2. Directory Structure

```
amr
├── deploy
│   └── scripts
├── fw
│   └── AMC
│       ├── scripts
│       └── src
│           ├── apps
│           │   ├── asdm
│           │   ├── bim
│           │   ├── in_band
│           │   └── out_of_band
│           ├── common
│           │   ├── core_libs
│           │   │   ├── dal
│           │   │   ├── evl
│           │   │   └── pll
│           │   └── include
│           ├── device_drivers
│           │   ├── eeprom
│           │   │   ├── aved
│           │   │   └── rave
│           │   ├── emmc
│           │   │   └── aved
│           │   ├── gcq_driver
│           │   │   └── src
│           │   ├── i2c
│           │   │   └── aved
│           │   ├── ospi
│           │   │   └── aved
│           │   ├── sensors
│           │   │   ├── cat34ts02
│           │   │   ├── ina3221
│           │   │   ├── isl68221
│           │   │   └── sys_mon
│           │   │       └── aved
│           │   └── smbus_driver
│           │       ├── doc
│           │       └── src
│           ├── fal
│           │   ├── emmc
│           │   ├── gcq
│           │   ├── muxed_device
│           │   ├── ospi
│           │   ├── smbus
│           │   ├── test
│           │   └── uart
│           ├── osal
│           │   └── src
│           │       └── freeRTOS
│           ├── profiles
│           │   ├── rave
│           │   └── v80
│           └── proxy_drivers
│               ├── ami
│               ├── apc
│               ├── asc
│               ├── axc
│               └── bmc
│                   ├── mctp
│                   └── pldm
└── sw
    └── AMI
        ├── api
        │   ├── include
        │   └── src
        ├── app
        │   └── cmd_handlers
        ├── driver
        └── scripts
            └── pkg_data
```

### 3. Build Instructions

**Prerequisites:**

```bash
source /proj/xbuilds/2026.1_daily_latest/installs/lin64/2026.1/Vivado/settings64.sh
```

**Building OSPI Images:**

Use the Yocto procedure to build all necessary binary OSPI images and packages. The AMR package is built using the [Yocto meta-embedded-plus](https://github.com/Xilinx/meta-embedded-plus) template. The final artifacts will be in the `build/tmp/deploy/images/emb_plus_ve2302_amr` folder.

```bash
MACHINE=emb-plus-ve2302-amr bitbake emb-plus-ospi-amr
```

**Building x86 Packages:**

```bash
./sw/AMI/scripts/gen_pkg_driver.py  -o <output_folder>
./sw/AMI/scripts/gen_pkg_libami.py  -o <output_folder>
./sw/AMI/scripts/gen_pkg_amitool.py -o <output_folder>
```

### 4. Test

**Installation:**

Log in to the Embedded+ Linux system, open a terminal, and install the packages:

```bash
sudo dpkg -i ami_x.x.x.xxx.xxx_amd64.deb
sudo dpkg -i libami_x.x.x.xxx.xxx_amd64.deb
sudo dpkg -i amitool_x.x.x.xxx.xxx_amd64.deb
```

**Usage:**

The following table lists available AMI commands. Use `--help` with any command for detailed usage information.

| #  | Category | Operation       | Command                                                                 | Description                                    |
|:---|:---------|:----------------|:------------------------------------------------------------------------|:-----------------------------------------------|
| 1  | AMI      | Help            | `ami_tool --help`                                                       | Display AMI help                               |
| 2  | AMI      | Version         | `ami_tool --version`                                                    | Display AMI version                            |
| 3  | AMI      | Overview        | `ami_tool overview`                                                     | Display AMI overview                           |
| 4  | AMI      | PCIe Info       | `ami_tool pcieinfo -d <b:d:f>`                                          | Display PCIe information                       |
| 5  | AMI      | Reload          | `sudo ami_tool reload -d 1 -t driver`                                   | Reload driver/pci/sbr                          |
| 6  | EEPROM   | Read            | `ami_tool eeprom_rd -d <b:d:f> -a 0 -l 4`                               | Read bytes from EEPROM                         |
| 7  | EEPROM   | Mfg Info        | `ami_tool mfg_info -d <b:d:f>`                                          | Read manufacturing information                 |
| 8  | OSPI     | CfgMem Info     | `ami_tool cfgmem_info -d <b:d:f> -t primary`                            | Read config memory information                 |
| 9  | OSPI     | CfgMem Program  | `sudo ami_tool cfgmem_program -d <b:d:f> -i amr_ospi.bin -p 1 -t primary` | Program PDI bitstream onto device            |
| 10 | OSPI     | CfgMem FPT      | `sudo ami_tool cfgmem_fpt -d <b:d:f> -t primary -i <fpt_file>`          | Program FPT onto OSPI                          |
| 11 | OSPI     | CfgMem Copy     | `sudo ami_tool cfgmem_copy -d 1 -i primary:0 -p primary:1`              | Copy partition to another                      |
| 12 | OSPI     | Device Boot     | `sudo ami_tool device_boot -d <b:d:f> -p 0`                             | Set device boot partition                      |
| 13 | PL       | PDI Program     | `sudo ami_tool pdi_program -d <b:d:f> -i <pdi>`                         | Program partial PDI                            |
| 14 | APU      | PDI Program     | `sudo ami_tool pdi_program -d <b:d:f> -i <apu.image> -a`                | Download APU image                             |
| 15 | Sensor   | Read Sensors    | `ami_tool sensors -d <b:d:f>`                                           | Get sensor values                              |
| 16 | Debug    | Verbosity       | `ami_tool debug_verbosity -d <b:d:f> -l debug`                          | Set log level                                  |

### 5. Glossary

| Acronym | Description                             |
|:--------|:----------------------------------------|
| AMC     | Adaptive Management Controller          |
| AMI     | Adaptive Management Interface           |
| AMR     | Adaptive Management Runtime             |
| BDF     | Bus Device Function                     |
| FPT     | Flash Partition Table                   |
| OSPI    | Octal SPI (Serial Peripheral Interface) |
| PF      | PCIe Physical Function                  |
| PL      | Programmable Logic                      |
| RPU     | Realtime Processing Unit                |
| UUID    | Universal Unique Identifier             |

### 6. References

1. [Versal ACAP Technical Reference Manual](https://docs.xilinx.com/r/en-US/am011-versal-acap-trm/Introduction)
2. [AMD Embedded+](https://www.amd.com/en/products/embedded/embedded-plus.html)
3. [Alveo Versal Example Design](https://xilinx.github.io/AVED/)
