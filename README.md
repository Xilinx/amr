##### Copyright (C) 2025 Advanced Micro Devices, Inc.  All rights reserved.
##### SPDX-License-Identifier: MIT
# AMR - Adaptive Management Runtime
[![License](https://img.shields.io/badge/license-MIT-green)](./LICENSE)
[![Introduction](https://img.shields.io/badge/-1._Introduction-informational)](#1-introduction)
[![Build Instructions](https://img.shields.io/badge/-2._Build_Instructions-critical)](#2-build-instructions)
[![Directory Structure](https://img.shields.io/badge/-3._Directory_Structure-bluegreen)](#3-directory-structure)
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
[3]: https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/embedded-design-tools.html

<b>VPR-4616 Board:</b>
![VPR-4616 Board](https://www.amd.com/content/dam/amd/en/images/products/som/2474370-sapphire-edge-vpr-4616.png)


### 2. Build Instructions
```
Defaults:
 RELEASE=2025.1
 BOARD=
 VIVADO_SETTINGS=/proj/xbuilds/2025.1_daily_latest/installs/lin64/2025.1/Vivado/settings64.sh
```
```bash
./settings.sh	# Verify environment variable settings in a shell session
```

Use yocto procedure to build all necessary binary images
The final artifacts will be in the build.<BOARD>/images folder.<br>
- `MACHINE=emb-plus-ve2302-amr bitbake emb-plus-ospi-amr`


### 3. Directory Structure
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
### 4. Test
Login to Embedded+ linux system, open a terminal and use the following interface
commands for usage. Some of the commands are mentioned below. Each command has
the help to get more info and command usage.

* ami_tool --help
* ami_tool --version
* ami_tool overview
* ami_tool pcieinfo -d <B:D:F>
* ami_tool sensors -d <B:D:F>
* ami_tool cfgmem_info -d <B:D:F> -t primary
* ami_tool mfg_info -d <B:D:F>

## 5. Glossary
| Name | Description   				             |
| :----| :---------------------------------------|
| AMC  | Adaptive Management Controller          |
| AMI  | Adaptive Management Interface           |
| AMR  | Adapative Management Runtime            |
| BDF  |	Bus Device Function                  |
| FPT  | Flash Partition Table		             |
| OSPI | Octal SPI (Serial Peripheral Interface) |
| RPU  | Realtime Processing Unit	             |
| PF   | PCIe Physical Function  	             |
| PL   | Programmable Logic			             |
| PLD  | Programmable Logic Domain	             |
| UUID |  Universal Unique Identifier            |

## 6. References
[versal-acap-trm]:	https://docs.xilinx.com/r/en-US/am011-versal-acap-trm/Introduction
[embedded-plus]:	https://www.amd.com/en/products/embedded/embedded-plus.html
1. [Versal ACAP Technical Reference Manual][versal-acap-trm]<br>
2. [AMD Embedded+][embedded-plus]


Full documentation for the ALVEO Versal Example Design can be found at the following link:
[xilinx.github.io/AVED/](https://xilinx.github.io/AVED/)
