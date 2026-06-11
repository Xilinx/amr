##### Copyright (C) 2025 - 2026 Advanced Micro Devices, Inc.  All rights reserved.

##### SPDX-License-Identifier: MIT

# AMR - Adaptive Management Runtime

[![License](https://img.shields.io/badge/license-MIT-green)](./LICENSE)
[![Introduction](https://img.shields.io/badge/-1._Introduction-informational)](#1-introduction)
[![Test](https://img.shields.io/badge/-2._Test-important)](#2-test)
[![Glossary](https://img.shields.io/badge/-3._Glossary-yellow)](#3-glossary)
[![References](https://img.shields.io/badge/-4._References-lightgrey)](#4-references)

### 1. Introduction

Adaptive Management Runtime (AMR) is the next-generation platform software stack for Alveo V80 and RAVE product lines designed to enable a more modular, software-defined approach to system bring-up and device management. In contrast to the legacy AVED model (for V80) - where platform initialization, management functions, and programmable logic (PL) were tightly coupled within a single, static configuration - AMR decouples control, configuration, and application layers through a lightweight runtime and mailbox-based communication model. This allows the platform to boot with minimal dependencies and dynamically load or reconfigure functionality (such as PL images) at runtime. The result is improved flexibility for partial and segmented configurations, more robust system control without reliance on PL-based mechanisms, and a clearer separation between infrastructure and application logic. For users, this translates into faster iteration, easier integration into automated workflows, and a more scalable foundation for deploying and managing Alveo V80 and RAVE systems in production environments.


For more detailed information on architecture, APIs, and usage guides, see **[AMR Docs](https://xilinx.github.io/amr-docs/)**.

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
| 16 | APU      | PDI Program    | `sudo ami_tool pdi_program -d <b:d.f> -i <apu_linux.pdi> -a`              | Download APU image                |
| 17 | RPU      | PDI Program    | `sudo ami_tool pdi_program -d <b:d.f> -i <rpu1.pdi> -r`                   | Download RPU1 image               |
| 18 | Sensor   | Read Sensors   | `ami_tool sensors -d <b:d.f>`                                             | Get sensor values                 |
| 19 | Debug    | Verbosity      | `ami_tool debug_verbosity -d <b:d.f> -l debug`                            | Set log level                     |

### 3. Glossary

| Name | Description                             |
| :----| :---------------------------------------|
| AMC  | Adaptive Management Controller          |
| AMI  | Adaptive Management Interface           |
| AMR  | Adapative Management Runtime            |
| BDF  | Bus Device Function                     |
| FPT  | Flash Partition Table                   |
| OSPI | Octal SPI (Serial Peripheral Interface) |
| RPU  | Realtime Processing Unit                |
| PF   | PCIe Physical Function                  |
| PL   | Programmable Logic                      |
| UUID | Universal Unique Identifier             |


### 4. References

1. [Versal ACAP Technical Reference Manual](https://docs.xilinx.com/r/en-US/am011-versal-acap-trm/Introduction)
2. [AMD Embedded+](https://www.amd.com/en/products/embedded/embedded-plus.html)
3. [AMR Docs](https://xilinx.github.io/amr-docs/)
4. [AVED: For legacy AVED solution for Alveo V80, please refer to this link](https://github.com/Xilinx/AVED)
