##### Copyright (C) 2025 - 2026 Advanced Micro Devices, Inc.  All rights reserved.

##### SPDX-License-Identifier: MIT

# AMR - Adaptive Management Runtime

[![License](https://img.shields.io/badge/license-MIT-green)](./LICENSE)
[![Introduction](https://img.shields.io/badge/-1._Introduction-informational)](#1-introduction)
[![Test](https://img.shields.io/badge/-2._Test-important)](#2-test)
[![Glossary](https://img.shields.io/badge/-3._Glossary-yellow)](#3-glossary)
[![References](https://img.shields.io/badge/-4._References-lightgrey)](#4-references)

### 1. Introduction

AMR (Adaptive Management Runtime) provides basic management capabilities for Alveo and Embedded+ boards. These boards feature a Versal device connected to an x86 host via PCIe. The Versal device is programmed with a PDI stored in OSPI flash and boots normally. Any updates to the OSPI are performed through the PCIe interface.

Two types of Versal devices are supported: one with CPM and another with PL PCIe IP. The Versal design contains the basic PCIe configuration information for shared memory communication. RPU0 serves as the device Adaptive Management Controller (AMC). The OSPI flash contains three partitions: the first two contain boot PDIs, and the third is used for storing user designs. On power-up, the boot ROM loads the PDI from the OSPI flash selected partition and runs AMC on the RPU0 core (which is part of the boot PDIs). The x86 host enumerates the Versal PCIe device and configures it. Once x86 configuration is complete, communication is established between the x86 Adaptive Management Interface (AMI) and AMC.

To build sample designs from source code in this repository, you will need the following tools installed. Follow the [build and install instructions](#2-build-instructions) link for details.


**VPR-4616 Board:**

![VPR-4616 Board](https://www.amd.com/content/dam/amd/en/images/products/som/2474370-sapphire-edge-vpr-4616.png)

**Alveo V80:**
![V80 Board](https://www.amd.com/content/dam/amd/en/images/products/aecg/alveo/v80/2612751-amd-alveo-v80-de-lidded.png)

[1]: https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/vivado-design-tools.html
[2]: https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/embedded-design-tools.html


### 2. Test

**Usage:**

The following table lists available AMI commands. Use `--help` with any command for detailed usage information.


| #  | Category | Operation      | Command                                                                   | Description                       |
|:---|:---------|:---------------|:--------------------------------------------------------------------------|:----------------------------------|
| 1  | AMI      | Help           | `ami_tool --help`                                                         | Display AMI help                  |
| 2  | AMI      | Version        | `ami_tool --version`                                                      | Display AMI version               |
| 3  | AMI      | Overview       | `ami_tool overview`                                                       | Display AMI overview              |
| 4  | AMI      | PCIe Info      | `ami_tool pcieinfo -d <b:d.f>`                                            | Display PCIe information          |
| 5  | AMI      | Reload         | `sudo ami_tool reload -d 1 -t driver`                                     | Reload driver/pci/sbr             |
| 6  | EEPROM   | Read           | `ami_tool eeprom_rd -d <b:d.f> -a 0 -l 4`                                 | Read bytes from EEPROM            |
| 7  | EEPROM   | Mfg Info       | `ami_tool mfg_info -d <b:d.f>`                                            | Read manufacturing information    |
| 8  | OSPI     | CfgMem Info    | `ami_tool cfgmem_info -d <b:d.f> -t primary`                              | Read config memory information    |
| 9  | OSPI     | CfgMem Flags   | `ami_tool cfgmem_flags_rd -d <b:d.f> -t primary -p 2`                     | Read config memory information    |
| 10 | OSPI     | CfgMem Flags   | `ami_tool cfgmem_flags_wr -d <b:d.f> -t primary -p 2 -i <on/off>`         | Write config memory information   |
| 11 | OSPI     | CfgMem Program | `sudo ami_tool cfgmem_program -d <b:d.f> -i <OSPI image> -p 1 -t primary` | Program PDI bitstream onto device |
| 12 | OSPI     | CfgMem FPT     | `sudo ami_tool cfgmem_fpt -d <b:d.f> -t primary -i <fpt_file>`            | Program FPT onto OSPI             |
| 13 | OSPI     | CfgMem Copy    | `sudo ami_tool cfgmem_copy -d 1 -i primary:0 -p primary:1`                | Copy partition to another         |
| 14 | OSPI     | Device Boot    | `sudo ami_tool device_boot -d <b:d.f> -p 0`                               | Set device boot partition         |
| 15 | PL       | PDI Program    | `sudo ami_tool pdi_program -d <b:d.f> -i <pdi>`                           | Program partial PDI               |
| 16 | APU      | PDI Program    | `sudo ami_tool pdi_program -d <b:d.f> -i <apu.image> -a`                  | Download APU image                |
| 17 | RPU      | PDI Program    | `sudo ami_tool pdi_program -d <b:d.f> -i <rpu.image> -r`                  | Download RPU1 image               |
| 18 | Sensor   | Read Sensors   | `ami_tool sensors -d <b:d.f>`                                             | Get sensor values                 |
| 19 | Debug    | Verbosity      | `ami_tool debug_verbosity -d <b:d.f> -l debug`                            | Set log level                     |

### 3. Glossary
[Versal ACAP Technical Reference Manual](https://docs.xilinx.com/r/en-US/am011-versal-acap-trm/Introduction)

### 4. References

1. [Versal ACAP Technical Reference Manual](https://docs.xilinx.com/r/en-US/am011-versal-acap-trm/Introduction)
2. [AMD Embedded+](https://www.amd.com/en/products/embedded/embedded-plus.html)
3. [AMR Docs](https://xilinx.github.io/AMR/)
