##### Copyright (C) 2025 Advanced Micro Devices, Inc.  All rights reserved.
##### SPDX-License-Identifier: MIT
# AMR - Adaptive Management Runtime
[![License](https://img.shields.io/badge/license-MIT-green)](./LICENSE)
[![Introduction](https://img.shields.io/badge/-1._Introduction-informational)](#1-introduction)
[![Directory Structure](https://img.shields.io/badge/-2._Directory_Structure-bluegreen)](#3-directory-structure)
[![Build Instructions](https://img.shields.io/badge/-3._Build_Instructions-critical)](#2-build-instructions)
[![Test](https://img.shields.io/badge/-4._Test-important)](#4-test)
[![Glossary](https://img.shields.io/badge/-6._Glossary-yellow)](#5-glossary)
[![References](https://img.shields.io/badge/-7._References-lightgrey)](#6-references)

### 1. Introduction
AMR: Adaptive Management Runtime is to basic management of Rave embedded-plus boards. These boards have Versal device  connected to x86 host via PCIe. The versal is programmed with PDI in the OSPI and will boot normally. Any update to the OSPI is done through the PCIe.

There are two types of Versal devices are used, one with CPM5 and another with PCIe IP.
The Versal design will contain the basic PCIe configuration information for shared memory communication. The RPU0 will be used as device management controller. The OSPI device will contain two partitions each with boot PDIs. On power up the bootrom loads the pdi from OSPI selected partition and runs Adaptive Management Controller (AMC) on RPU0 core which is part of boot PDIs. The x86 will enumerate the Versal PCIe device and configures it. Once the x86 configuration is done, the communication is established between the x86 and AMC.
```
 ```

To build sample designs from source code in this repository, you will need to have the
following tools installed and follow the [build instructions](#2-build-instructions):

- A Linux-based host OS with AMD tools installed. It requires about 50GB free disk space
- [Vivado][1] 2025.1

[1]: https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/vivado-design-tools.html
[2]: https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/embedded-design-tools.html

<b>VPR-4616 Board:</b>
![VPR-4616 Board](https://www.amd.com/content/dam/amd/en/images/products/som/2474370-sapphire-edge-vpr-4616.png)

### 2. Directory Structure
```
.amr
.
├── deploy
│   └── scripts
├── fw
│   └── AMC
│       ├── fpt
│       ├── scripts
│       └── src
│           ├── apps
│           │   └── asdm
│           ├── common
│           │   ├── core_libs
│           │   └── include
│           ├── device_drivers
│           │   ├── eeprom
│           │   ├── emmc
│           │   ├── gcq_driver
│           │   ├── i2c
│           │   ├── ospi
│           │   ├── sensors
│           │   └── smbus_driver
│           ├── fal
│           │   ├── emmc
│           │   ├── gcq
│           │   ├── muxed_device
│           │   ├── ospi
│           │   ├── smbus
│           │   ├── test
│           │   └── uart
│           ├── osal
│           │   └── src
│           ├── profiles
│           │   ├── Linux
│           │   ├── rave
│           │   ├── v70
│           │   └── v80
│           └── proxy_drivers
│               ├── ami
│               ├── apc
│               ├── asc
│               ├── axc
│               └── bmc
└── sw
    └── AMI
        ├── api
        │   ├── include
        │   ├── src
        │   └── test
        ├── app
        │   ├── cmd_handlers
        │   └── test
        ├── driver
        │   ├── fal
        │   │   └── gcq
        │   └── gcq-driver
        │       └── src
        └── scripts
            └── pkg_data

```
### 3. Build Instructions
```
Defaults:
source /proj/xbuilds/2025.1_daily_latest/installs/lin64/2025.1/Vivado/settings64.sh
```

Use yocto procedure to build all necessary binary OSPI images and packages. The following commands will build the AMR OSPI package and create the necessary artifacts. The AMR package is built using the Yocto https://github.com/Xilinx/meta-embedded-plus template. The final artifacts will be in the
build/tmp/deploy/images/emb_plus_ve2302_amr folder.<br>
- `MACHINE=emb-plus-ve2302-amr bitbake emb-plus-ospi-amr`

Build x86 packages using the following commands.
- `./sw/AMI/scripts/gen_pkg_driver.py  -o <output folder>`
- `./sw/AMI/scripts/gen_pkg_libami.py  -o <output folder>`
- `./sw/AMI/scripts/gen_pkg_amitool.py -o <output folder>`

### 4. Test
Login to Embedded+ linux system, open a terminal and use the following interface
commands for usage. Some of the commands are mentioned below. Each command has
the help to get more info and command usage.
```
INSTALL:
- sudo dpkg -i ami_x.x.x.xxx.xxx_amd64_22.04.deb
- sudo dpkg -i amitool_x.x.x.xxx.xxx_amd64_22.04.deb
- sudo dpkg -i libami.x.x.xxx.xxx_amd64_22.04.deb
```
TEST:
|   |Device|  AMI          |   Command                                                             |         Notes            |
|:--|:-----|:--------------|:----------------------------------------------------------------------|:-------------------------|
| 1 | ami  | help          |ami_tool --help                                                        |AMI help                  |
| 2 | ami  | version       |ami_tool --version                                                     |AMI version               |
| 3 | ami  | overview      |ami_tool overview                                                      |AMI overview              |
| 4 | ami  | pcie info     |ami_tool pcieinfo -d <b:d:f>                                           |AMI PCIe information      |
| 5 | ami  | bar read      |sudo ami_tool bar_rd -d <b:d:f> -b 0 -a 0 -l 4                         |AMI PCIe read bar memory  |
| 6 | ami  | bar write     |sudo ami_tool bar_wr -d <b:d:f> -b 0 -a 0 -i 0x1234                    |AMI PCIe write bar memory |
| 7 | ami  | reload        |sudo ami_tool reload -d 1 -t driver	                                   |AMI reload driver/pci/sbr |
| 8 |EEPROM| eeprom read   |ami_tool eeprom_rd -d <b:d:f> -a 0 -l 4	                               |Read one or more bytes of data from the EEPROM |
| 9 |EEPROM| mfg info      |ami_tool mfg_info -d <b:d:f>	                                       |Read manufacturing  information|
|10 |OSPI  | cfgmem info   |ami_tool cfgmem_info -d <b:d:f> -t primary                             |Read config memory information|
|11 |OSPI  | cfgmem program|sudo ami_tool cfgmem_program -d <b:d:f> -i amr_ospi.bin -p 1 -t primary|Program a .pdi bitstream onto a device |
|12 |OSPI  | cfgmem fpt    |sudo ami_tool cfgmem_fpt -d <b:d:f> -t primary -i <fpt file>	       |Program FPT onto a OSPI |
|13 |OSPI  | cfgmem copy   |sudo ami_tool cfgmem_copy -d 1 -i primary:0 -p primary:1	           |Copy one device partition to another |
|14 |OSPI  | device boot   |sudo ami_tool device_boot -d <b:d:f> -p 0	                           |Set the device boot partition |
|15 |PL    | PDI program   |sudo ami_tool pdi_program -d <b:d:f> -i <pdi>	                       |Program partial pdi |
|16 |Sensor| sensors       |ami_tool sensors -d <b:d:f>	                                           |Get the value of a sensor |
|17 |Device|debug verbosity|ami_tool debug_verbosity -d <b:d:f> -l debug	                       |Set log level |

### 5. Glossary
| Name | Description   				             |
| :----| :---------------------------------------|
| AMC  | Adaptive Management Controller          |
| AMI  | Adaptive Management Interface           |
| AMR  | Adapative Management Runtime            |
| BDF  | Bus Device Function                     |
| FPT  | Flash Partition Table		             |
| OSPI | Octal SPI (Serial Peripheral Interface) |
| RPU  | Realtime Processing Unit	             |
| PF   | PCIe Physical Function  	             |
| PL   | Programmable Logic			             |
| UUID | Universal Unique Identifier             |

## 6. References
1. [Versal ACAP Technical Reference Manual](https://docs.xilinx.com/r/en-US/am011-versal-acap-trm/Introduction)
2. [AMD Embedded+](https://www.amd.com/en/products/embedded/embedded-plus.html)
3. [Alveo Versal Example Design](https://xilinx.github.io/AVED/)
