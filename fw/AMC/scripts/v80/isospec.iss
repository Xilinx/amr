{
  "spec_file_version": 4,
  "info": {
    "arch_data_version": 100,
    "device_name": "v80",
    "settings_file": "default_settings",
    "settings_suffix": "ps-pmc",
    "format": "shallow"
  },
  "flags": {"debugger_access": true, "enable_protection": false, "write_subsystems": true},
  "default_settings": {
    "flags": {"debugger_access": true, "enable_protection": true, "write_subsystems": true},
    "base_protection": {
      "access": [
        {
          "name": "firmware_and_cci",
          "comment": "root-access for memory, opens SMID 0 for CCI cache management",
          "type": "smid_list",
          "SMIDs": ["FPD_CCI", "PMC_DMA0", "PMC_DMA1", "RCU_and_PPU"]
        },
        {
          "name": "firmware_and_cci.debug",
          "comment": "root-access for memory, opens SMID 0 for CCI cache management",
          "type": "smid_list",
          "SMIDs": ["DAP", "FPD_CCI", "PMC_DMA_and_DPC", "RCU_and_PPU"]
        },
        {
          "name": "firmware",
          "comment": "Firmware masters, used as a masters list for protection unit root-access",
          "type": "smid_list",
          "SMIDs": ["PMC_DMA0", "PMC_DMA1", "PSM", "RCU_and_PPU"]
        },
        {
          "name": "firmware.debug",
          "comment": "Firmware masters, used for root-access if debugger_access is enabled",
          "type": "smid_list",
          "SMIDs": ["DAP", "PMC_DMA_and_DPC", "PSM", "RCU_and_PPU"]
        },
        {
          "name": "open_rw",
          "comment": "Any master can access these as read/write",
          "destinations": [
            "CoreSight",
            "IPI_buffers",
            "IPI_channels",
            "OCM_CSR",
            "RPU_DUAL_CSR",
            "USB2_XHCI"
          ],
          "SMIDs": ["ANY"]
        },
        {
          "name": "open_rw_fpd",
          "comment": "Any master can access these as read/write",
          "destinations": ["APU_DUAL_CSR", "APU_GIC", "FPD_SMMU", "FPD_SMMU_CSR", "FPD_SWDT"],
          "SMIDs": ["ANY"]
        },
        {
          "name": "open_ro",
          "comment": "Any master can access these as read-only",
          "destinations": ["CRP", "SCNTR_SECURE"],
          "flags": {"read_only": true},
          "SMIDs": ["ANY"]
        },
        {
          "name": "open_ro_2",
          "comment": "Any master can access these as read-only",
          "destinations": ["PLM_RTCA"],
          "flags": {"read_only": true},
          "SMIDs": ["ANY"]
        },
        {
          "name": "apu_rw",
          "destinations": ["FPD_CCI_CORE"],
          "SMIDs": ["APU"]
        },
        {
          "name": "bootmedia",
          "comment": "Allow booting from different boot media",
          "destinations": [
            "CFU_STREAM_mem",
            "PMC_OSPI_mem",
            "PPU",
            "PSM_DCACHE_mem",
            "PSM_ICACHE_mem",
            "RPU_Memory"
          ],
          "SMIDs": ["OSPI", "QSPI", "SD_eMMC"]
        },
        {
          "name": "bootmedia_pmc",
          "destinations": ["PMC_RAM_mem"],
          "SMIDs": ["OSPI", "QSPI", "SD_eMMC"]
        },
        {
          "name": "PLM_image_store",
          "comment": "Destination for PLM image store",
          "SMIDs": ["PMC_DMA0", "PMC_DMA1", "PSM", "RCU_and_PPU"]
        },
        {
          "name": "firmware_SLR1",
          "comment": "Firmware masters, used as a masters list for protection unit root-access",
          "type": "smid_list",
          "SMIDs": ["PMC_DMA0_SLR1", "PMC_DMA1_SLR1", "RCU_and_PPU_SLR1"]
        },
        {
          "name": "firmware_SLR1.debug",
          "comment": "Firmware masters, used for root-access if debugger_access is enabled",
          "type": "smid_list",
          "SMIDs": ["DAP", "DPC", "PMC_DMA_and_DPC_SLR1", "RCU_and_PPU_SLR1"]
        },
        {
          "name": "firmware_SLR2",
          "comment": "Firmware masters, used as a masters list for protection unit root-access",
          "type": "smid_list",
          "SMIDs": ["PMC_DMA0_SLR2", "PMC_DMA1_SLR2", "RCU_and_PPU_SLR2"]
        },
        {
          "name": "firmware_SLR2.debug",
          "comment": "Firmware masters, used for root-access if debugger_access is enabled",
          "type": "smid_list",
          "SMIDs": ["DAP", "DPC", "PMC_DMA_and_DPC_SLR2", "RCU_and_PPU_SLR2"]
        },
        {
          "name": "slr_ro",
          "comment": "Any master can access these as read-only",
          "destinations": ["PMC_SYSMON_CSR_SLR1", "PMC_SYSMON_CSR_SLR2"],
          "flags": {"read_only": true},
          "SMIDs": ["ANY"]
        },
        {
          "name": "slr_ro_2",
          "comment": "Any master can access these as read-only",
          "destinations": ["PLM_RTCA_SLR1", "PLM_RTCA_SLR2", "SSIT_COMM", "SSIT_COMM_SLR1", "SSIT_COMM_SLR2"],
          "flags": {"read_only": true},
          "SMIDs": ["ANY"]
        },
        {
          "name": "slr_rw",
          "comment": "Any master can access these as read-write",
          "destinations": ["PMC_SBI_STREAM_mem", "PMC_SBI_STREAM_mem_SLR1", "PMC_SBI_STREAM_mem_SLR2"],
          "SMIDs": ["ANY"]
        }
      ],
      "units": {
        "FPD_XMPU": {
          "root_access": "firmware",
          "flags": {"enable": true, "interrupt_enable": true, "lock": true}
        },
        "LPD_XPPU": {
          "root_access": "firmware",
          "flags": {"enable": true, "interrupt_enable": true, "lock": true}
        },
        "OCM_XMPU": {
          "root_access": "firmware_and_cci",
          "flags": {"interrupt_enable": true, "lock": true}
        },
        "PMC_XMPU": {
          "root_access": "firmware",
          "flags": {"enable": true, "interrupt_enable": true, "lock": true}
        },
        "PMC_XMPU_SLR1": {
          "root_access": "firmware_SLR1",
          "flags": {"enable": true, "interrupt_enable": true, "lock": true}
        },
        "PMC_XMPU_SLR2": {
          "root_access": "firmware_SLR2",
          "flags": {"enable": true, "interrupt_enable": true, "lock": true}
        },
        "PMC_XPPU": {
          "root_access": "firmware",
          "flags": {"enable": true, "interrupt_enable": true, "lock": true}
        },
        "PMC_XPPU_NPI": {
          "root_access": "firmware",
          "flags": {"enable": true, "interrupt_enable": true, "lock": true}
        },
        "PMC_XPPU_NPI_SLR1": {
          "root_access": "firmware_SLR1",
          "flags": {"enable": true, "interrupt_enable": true, "lock": true}
        },
        "PMC_XPPU_NPI_SLR2": {
          "root_access": "firmware_SLR2",
          "flags": {"enable": true, "interrupt_enable": true, "lock": true}
        },
        "PMC_XPPU_SLR1": {
          "root_access": "firmware_SLR1",
          "flags": {"enable": true, "interrupt_enable": true, "lock": true}
        },
        "PMC_XPPU_SLR2": {
          "root_access": "firmware_SLR2",
          "flags": {"enable": true, "interrupt_enable": true, "lock": true}
        }
      }
    },
    "subsystems": {
      "default": {
        "id": "0x0",
        "access": [
          {
            "name": "cpus",
            "type": "cpu_list",
            "SMIDs": ["APU"]
          },
          {
            "name": "secure_cpus",
            "type": "cpu_list",
            "flags": {"secure": true},
            "SMIDs": ["RPU0"]
          },
          {
            "name": "uart",
            "destinations": ["UART0", "UART1"],
            "flags": {"requested": true, "requested_emit_wakeup": true, "requested_full_access": true, "requested_preserve_context": true, "shared": true}
          },
          {
            "name": "gem",
            "destinations": ["GEM0", "GEM1"],
            "flags": {"requested": true, "requested_emit_wakeup": true, "requested_full_access": true, "requested_preserve_context": true, "requested_secure": false, "secure": false, "shared": true}
          },
          {
            "name": "mem_ctrl",
            "destinations": ["DDR0"],
            "type": "ss_management",
            "flags": {"shared": true},
            "SMIDs": ["ANY"]
          },
          {
            "name": "mem",
            "destinations": ["OCM0_mem", "OCM1_mem", "OCM2_mem", "OCM3_mem"],
            "flags": {"requested": true, "requested_full_access": true, "requested_preserve_context": true, "shared": true},
            "SMIDs": ["ANY"]
          },
          {
            "name": "bootmedia",
            "destinations": ["OSPI", "QSPI", "SD_eMMC0", "SD_eMMC1"],
            "flags": {"requested": true, "requested_full_access": true, "requested_secure": false, "secure": false, "shared": true}
          },
          {
            "name": "OSPI_mem",
            "comment": "OSPI may require PMC_OSPI_mem",
            "destinations": ["PMC_OSPI_mem"],
            "flags": {"requested": true, "requested_full_access": true},
            "SMIDs": ["APU", "RPU"]
          },
          {
            "name": "non_secure",
            "destinations": [
              "LPD_I2C0",
              "LPD_I2C1",
              "TTC0",
              "TTC1",
              "TTC2",
              "TTC3"
            ],
            "flags": {"requested": true, "requested_full_access": true, "shared": true}
          },
          {
            "name": "ipi0",
            "destinations": ["IPI0"],
            "flags": {"requested": true, "requested_full_access": true},
            "SMIDs": ["APU"]
          },
          {
            "name": "ipi1",
            "destinations": ["IPI1"],
            "flags": {"requested": true, "requested_full_access": true},
            "SMIDs": ["RPU0"]
          },
          {
            "name": "ipi2",
            "destinations": ["IPI2"],
            "flags": {"requested": true, "requested_full_access": true},
            "SMIDs": ["RPU1"]
          },
          {
            "name": "ipi5",
            "destinations": ["IPI5"],
            "flags": {"requested": true, "requested_full_access": true},
            "SMIDs": ["DAP_and_DPC"]
          },
          {
            "name": "ipi_unused",
            "destinations": ["IPI3", "IPI4", "IPI6"],
            "flags": {"requested": true, "requested_full_access": true},
            "SMIDs": ["UNUSED"]
          },
          {
            "name": "resets",
            "destinations": [
              "RST_NOC",
              "RST_NOC_POR",
              "RST_NPI",
              "RST_PL0",
              "RST_PL1",
              "RST_PL2",
              "RST_PL3",
              "RST_PL_POR",
              "RST_PL_SRST",
              "RST_PMC",
              "RST_PMC_POR",
              "RST_SYS_RST_1",
              "RST_SYS_RST_2",
              "RST_SYS_RST_3"
            ],
            "type": "ss_management"
          },
          {
            "name": "not_requested_secure",
            "destinations": ["USB2_CSR"],
            "flags": {"secure": false, "shared": true}
          },
          {
            "name": "ggs_pggs",
            "destinations": [
              "GGS0",
              "GGS1",
              "GGS2",
              "GGS3",
              "PGGS0",
              "PGGS1"
            ],
            "type": "ss_management"
          },
          {
            "name": "not_requested_mgr",
            "destinations": [
              "LPD_DMA_CH0",
              "LPD_DMA_CH1",
              "LPD_DMA_CH2",
              "LPD_DMA_CH3",
              "LPD_DMA_CH4",
              "LPD_DMA_CH5",
              "LPD_DMA_CH6",
              "LPD_DMA_CH7",
              "PMC_SYSMON_CSR"
            ],
            "flags": {"shared": true}
          },
          {
            "name": "not_requested_fpd",
            "destinations": ["FPD_SWDT"],
            "flags": {"shared": true}
          },
          {
            "name": "not_requested_mem",
            "destinations": ["RPU0_TCMA_mem", "RPU0_TCMB_mem", "RPU1_TCMA_mem_dual", "RPU1_TCMB_mem_dual"],
            "flags": {"shared": true}
          },
          {
            "name": "not_requested",
            "destinations": [
              "CANFD0",
              "CANFD1",
              "LPD_GPIO",
              "LPD_SWDT",
              "PMC_EFUSE_CACHE",
              "PMC_GPIO",
              "PMC_I2C",
              "PMC_RTC",
              "SPI0",
              "SPI1"
            ],
            "flags": {"shared": true}
          }
        ]
      }
    }
  },
  "design": {
    "name": "design",
    "destinations": [
      {"name": "amr_mem_dram", "addr": "0x0", "size": "256M", "mem": true, "nodeid": "0x18900000"},
      {"name": "rpu1_mem_dram", "addr": "0x10800000", "size": "128M", "mem": true},
      {"name": "apu_mem_dram", "addr": "0x1c800000", "size": "1592M", "mem": true, "nodeid": "0x18900001"}
    ],
    "cells": {
      "axi_noc_cips": {
        "type": "noc",
        "XMPUs": [
          {
            "name": "HBM0_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4000000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X0Y0_PC0"]
          },
          {
            "name": "HBM0_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4040000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X0Y0_PC1"]
          },
          {
            "name": "HBM10_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4500000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X10Y0_PC0"]
          },
          {
            "name": "HBM10_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4540000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X10Y0_PC1"]
          },
          {
            "name": "HBM11_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4580000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X11Y0_PC0"]
          },
          {
            "name": "HBM11_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x45c0000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X11Y0_PC1"]
          },
          {
            "name": "HBM12_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4600000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X12Y0_PC0"]
          },
          {
            "name": "HBM12_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4640000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X12Y0_PC1"]
          },
          {
            "name": "HBM13_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4680000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X13Y0_PC0"]
          },
          {
            "name": "HBM13_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x46c0000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X13Y0_PC1"]
          },
          {
            "name": "HBM14_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4700000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X14Y0_PC0"]
          },
          {
            "name": "HBM14_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4740000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X14Y0_PC1"]
          },
          {
            "name": "HBM15_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4780000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X15Y0_PC0"]
          },
          {
            "name": "HBM15_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x47c0000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X15Y0_PC1"]
          },
          {
            "name": "HBM1_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4080000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X1Y0_PC0"]
          },
          {
            "name": "HBM1_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x40c0000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X1Y0_PC1"]
          },
          {
            "name": "HBM2_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4100000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X2Y0_PC0"]
          },
          {
            "name": "HBM2_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4140000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X2Y0_PC1"]
          },
          {
            "name": "HBM3_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4180000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X3Y0_PC0"]
          },
          {
            "name": "HBM3_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x41c0000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X3Y0_PC1"]
          },
          {
            "name": "HBM4_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4200000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X4Y0_PC0"]
          },
          {
            "name": "HBM4_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4240000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X4Y0_PC1"]
          },
          {
            "name": "HBM5_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4280000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X5Y0_PC0"]
          },
          {
            "name": "HBM5_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x42c0000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X5Y0_PC1"]
          },
          {
            "name": "HBM6_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4300000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X6Y0_PC0"]
          },
          {
            "name": "HBM6_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4340000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X6Y0_PC1"]
          },
          {
            "name": "HBM7_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4380000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X7Y0_PC0"]
          },
          {
            "name": "HBM7_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x43c0000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X7Y0_PC1"]
          },
          {
            "name": "HBM8_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4400000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X8Y0_PC0"]
          },
          {
            "name": "HBM8_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4440000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X8Y0_PC1"]
          },
          {
            "name": "HBM9_PC0",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x4480000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X9Y0_PC0"]
          },
          {
            "name": "HBM9_PC1",
            "module": "hbm_xmpu",
            "addrs": [
              {"addr": "0x44c0000000", "size": "1G"}
            ],
            "sites": ["HBM_MC_X9Y0_PC1"]
          }
        ],
        "destinations": [
          {"name": "HBM0_PC0", "addr": "0x4000000000", "size": "1G", "mem": true},
          {"name": "HBM0_PC1", "addr": "0x4040000000", "size": "1G", "mem": true},
          {"name": "HBM1_PC0", "addr": "0x4080000000", "size": "1G", "mem": true},
          {"name": "HBM1_PC1", "addr": "0x40c0000000", "size": "1G", "mem": true},
          {"name": "HBM2_PC0", "addr": "0x4100000000", "size": "1G", "mem": true},
          {"name": "HBM2_PC1", "addr": "0x4140000000", "size": "1G", "mem": true},
          {"name": "HBM3_PC0", "addr": "0x4180000000", "size": "1G", "mem": true},
          {"name": "HBM3_PC1", "addr": "0x41c0000000", "size": "1G", "mem": true},
          {"name": "HBM4_PC0", "addr": "0x4200000000", "size": "1G", "mem": true},
          {"name": "HBM4_PC1", "addr": "0x4240000000", "size": "1G", "mem": true},
          {"name": "HBM5_PC0", "addr": "0x4280000000", "size": "1G", "mem": true},
          {"name": "HBM5_PC1", "addr": "0x42c0000000", "size": "1G", "mem": true},
          {"name": "HBM6_PC0", "addr": "0x4300000000", "size": "1G", "mem": true},
          {"name": "HBM6_PC1", "addr": "0x4340000000", "size": "1G", "mem": true},
          {"name": "HBM7_PC0", "addr": "0x4380000000", "size": "1G", "mem": true},
          {"name": "HBM7_PC1", "addr": "0x43c0000000", "size": "1G", "mem": true},
          {"name": "HBM8_PC0", "addr": "0x4400000000", "size": "1G", "mem": true},
          {"name": "HBM8_PC1", "addr": "0x4440000000", "size": "1G", "mem": true},
          {"name": "HBM9_PC0", "addr": "0x4480000000", "size": "1G", "mem": true},
          {"name": "HBM9_PC1", "addr": "0x44c0000000", "size": "1G", "mem": true},
          {"name": "HBM10_PC0", "addr": "0x4500000000", "size": "1G", "mem": true},
          {"name": "HBM10_PC1", "addr": "0x4540000000", "size": "1G", "mem": true},
          {"name": "HBM11_PC0", "addr": "0x4580000000", "size": "1G", "mem": true},
          {"name": "HBM11_PC1", "addr": "0x45c0000000", "size": "1G", "mem": true},
          {"name": "HBM12_PC0", "addr": "0x4600000000", "size": "1G", "mem": true},
          {"name": "HBM12_PC1", "addr": "0x4640000000", "size": "1G", "mem": true},
          {"name": "HBM13_PC0", "addr": "0x4680000000", "size": "1G", "mem": true},
          {"name": "HBM13_PC1", "addr": "0x46c0000000", "size": "1G", "mem": true},
          {"name": "HBM14_PC0", "addr": "0x4700000000", "size": "1G", "mem": true},
          {"name": "HBM14_PC1", "addr": "0x4740000000", "size": "1G", "mem": true},
          {"name": "HBM15_PC0", "addr": "0x4780000000", "size": "1G", "mem": true},
          {"name": "HBM15_PC1", "addr": "0x47c0000000", "size": "1G", "mem": true}
        ]
      },
      "axi_noc_mc_ddr4_0": {
        "type": "noc",
        "XMPUs": [
          {
            "name": "ddrmc",
            "module": "ddr4_xmpu",
            "addrs": [
              {"addr": "0x0", "size": "2G"},
              {"addr": "0x50000000000", "size": "512G"}
            ],
            "sites": ["DDRMC_X0Y0"]
          }
        ],
        "destinations": [
          {"name": "DDR_LOW0", "addr": "0x0", "size": "2G", "mem": true},
          {"name": "DDR_CH1", "addr": "0x50080000000", "size": "2G", "mem": true}
        ]
      },
      "axi_noc_mc_ddr4_1": {
        "type": "noc",
        "XMPUs": [
          {
            "name": "ddrmc",
            "module": "ddr4_xmpu",
            "addrs": [
              {"addr": "0x60000000000", "size": "512G"}
            ],
            "sites": ["DDRMC_X1Y0"]
          }
        ],
        "destinations": [
          {"name": "DDR_CH2", "addr": "0x60000000000", "size": "32G", "mem": true}
        ]
      },
      "ps": {
        "type": "ps",
        "SMIDs": [
          {"name": "ANY", "comment": "Matches all SMIDs", "value": "0x0", "count": 1024},
          {"name": "PL_AIE_CPM", "value": "0x0", "count": 512},
          {"name": "PL_AIE", "value": "0x0", "count": 256, "altname": "PL"},
          {"name": "FPD_CCI", "comment": "Issued by CCI-500 cache management", "value": "0x0"},
          {"name": "AIE", "value": "0xf0", "count": 16},
          {"name": "CPM", "value": "0x100", "count": 256},
          {"name": "CIPS", "value": "0x200", "count": 128, "perSLR": true},
          {"name": "LPD", "value": "0x200", "count": 64},
          {"name": "RPU", "value": "0x200", "count": 8, "cpu": true, "type": "rpu"},
          {"name": "RPU0", "value": "0x200", "count": 4, "cpu": true, "type": "rpu", "nodeid": "0x18110005"},
          {"name": "RPU1", "value": "0x204", "count": 4, "cpu": true, "type": "rpu", "nodeid": "0x18110006"},
          {"name": "LPD_DMA_CH", "value": "0x210", "count": 16},
          {"name": "LPD_DMA_CH0", "value": "0x210", "count": 2},
          {"name": "LPD_DMA_CH1", "value": "0x212", "count": 2},
          {"name": "LPD_DMA_CH2", "value": "0x214", "count": 2},
          {"name": "LPD_DMA_CH3", "value": "0x216", "count": 2},
          {"name": "LPD_DMA_CH4", "value": "0x218", "count": 2},
          {"name": "LPD_DMA_CH5", "value": "0x21a", "count": 2},
          {"name": "LPD_DMA_CH6", "value": "0x21c", "count": 2},
          {"name": "LPD_DMA_CH7", "value": "0x21e", "count": 2},
          {"name": "USB2", "value": "0x230", "count": 2},
          {"name": "GEM", "value": "0x234", "count": 2},
          {"name": "GEM0", "value": "0x234", "count": 1},
          {"name": "GEM1", "value": "0x235", "count": 1},
          {"name": "PSM_and_DPC_DMA", "comment": "includes: PSM; DPC_DMA", "value": "0x238", "count": 2},
          {"name": "PSM", "value": "0x238", "count": 1},
          {"name": "DPC_DMA", "value": "0x239", "count": 1},
          {"name": "PMC", "value": "0x240", "count": 16, "perSLR": true},
          {"name": "DAP", "value": "0x240", "count": 1},
          {"name": "PMC_SYSMON", "value": "0x241", "count": 1, "perSLR": true},
          {"name": "SD_eMMC", "value": "0x242", "count": 2},
          {"name": "SD_eMMC0", "value": "0x242", "count": 1},
          {"name": "SD_eMMC1", "value": "0x243", "count": 1},
          {"name": "QSPI", "value": "0x244", "count": 1},
          {"name": "OSPI", "value": "0x245", "count": 1},
          {"name": "RCU_and_PPU", "value": "0x246", "count": 2, "perSLR": true},
          {"name": "RCU", "comment": "ROM Code Unit", "value": "0x246", "count": 1, "cpu": true},
          {"name": "PPU", "value": "0x247", "count": 1},
          {"name": "DAP_and_DPC", "comment": "used for IPI; includes PMC_SYSMON and PMC_DMA0 ", "value": "0x248", "mask_n": "0x9"},
          {"name": "PMC_DMA_and_DPC", "comment": "includes: PMC_DMA0; DPC; unused SMID 0x24A; PMC_DMA1", "value": "0x248", "count": 4, "perSLR": true},
          {"name": "PMC_DMA0", "value": "0x248", "count": 1, "perSLR": true},
          {"name": "DPC", "value": "0x249", "count": 1, "perSLR": true},
          {"name": "PMC_DMA1", "value": "0x24b", "count": 1, "perSLR": true},
          {"name": "PCIe", "value": "0x250", "count": 8},
          {"name": "APU", "comment": "APU bits 3:0 are determined by the AXI_ID", "value": "0x260", "count": 16, "cpu": true, "type": "apu"},
          {"name": "APU0", "comment": "APU0 & 1 have the same SMIDs but can specify which core is used in a subsystem", "value": "0x260", "count": 16, "cpu": true, "type": "apu", "nodeid": "0x1810c003"},
          {"name": "APU1", "value": "0x260", "count": 16, "cpu": true, "type": "apu", "nodeid": "0x1810c004"},
          {"name": "APU_GIC", "value": "0x272", "count": 1},
          {"name": "DEBUG", "comment": "CoreSight", "value": "0x273", "count": 1},
          {"name": "FPD_SMMU", "value": "0x274", "count": 1},
          {"name": "UNUSED", "comment": "Unused SMID used for IPI channels", "value": "0x3ff", "altname": "NONE"}
        ],
        "destinations": [
          {"name": "DDR0", "addr": "0x0", "size": "0", "nodeid": "0x18320010"},
          {"name": "GGS0", "addr": "0x0", "size": "0", "nodeid": "0x18248000"},
          {"name": "GGS1", "addr": "0x0", "size": "0", "nodeid": "0x18248001"},
          {"name": "GGS2", "addr": "0x0", "size": "0", "nodeid": "0x18248002"},
          {"name": "GGS3", "addr": "0x0", "size": "0", "nodeid": "0x18248003"},
          {"name": "HB_MON0", "addr": "0x0", "size": "0", "nodeid": "0x18250000"},
          {"name": "HB_MON1", "addr": "0x0", "size": "0", "nodeid": "0x18250001"},
          {"name": "HB_MON2", "addr": "0x0", "size": "0", "nodeid": "0x18250002"},
          {"name": "HB_MON3", "addr": "0x0", "size": "0", "nodeid": "0x18250003"},
          {"name": "HB_MON4", "addr": "0x0", "size": "0", "nodeid": "0x18250004"},
          {"name": "HB_MON5", "addr": "0x0", "size": "0", "nodeid": "0x18250005"},
          {"name": "HB_MON6", "addr": "0x0", "size": "0", "nodeid": "0x18250006"},
          {"name": "HB_MON7", "addr": "0x0", "size": "0", "nodeid": "0x18250007"},
          {"name": "PGGS0", "addr": "0x0", "size": "0", "nodeid": "0x1824c004"},
          {"name": "PGGS1", "addr": "0x0", "size": "0", "nodeid": "0x1824c005"},
          {"name": "RST_NOC", "addr": "0x0", "size": "0", "nodeid": "0xc41000c"},
          {"name": "RST_NOC_POR", "addr": "0x0", "size": "0", "nodeid": "0xc30c005"},
          {"name": "RST_NPI", "addr": "0x0", "size": "0", "nodeid": "0xc41000d"},
          {"name": "RST_PL0", "addr": "0x0", "size": "0", "nodeid": "0xc410012"},
          {"name": "RST_PL1", "addr": "0x0", "size": "0", "nodeid": "0xc410013"},
          {"name": "RST_PL2", "addr": "0x0", "size": "0", "nodeid": "0xc410014"},
          {"name": "RST_PL3", "addr": "0x0", "size": "0", "nodeid": "0xc410015"},
          {"name": "RST_PL_POR", "addr": "0x0", "size": "0", "nodeid": "0xc30c004"},
          {"name": "RST_PL_SRST", "addr": "0x0", "size": "0", "nodeid": "0xc41000b"},
          {"name": "RST_PMC", "addr": "0x0", "size": "0", "nodeid": "0xc410002"},
          {"name": "RST_PMC_POR", "addr": "0x0", "size": "0", "nodeid": "0xc30c001"},
          {"name": "RST_SYS_RST_1", "addr": "0x0", "size": "0", "nodeid": "0xc41000e"},
          {"name": "RST_SYS_RST_2", "addr": "0x0", "size": "0", "nodeid": "0xc41000f"},
          {"name": "RST_SYS_RST_3", "addr": "0x0", "size": "0", "nodeid": "0xc410010"},
          {"name": "PMC_OSPI_mem", "addr": "0xc0000000", "size": "512M", "mem": true, "nodeid": "0x18900002"},
          {"name": "PMC_I2C", "addr": "0xf1000000", "size": "64K", "nodeid": "0x1822402d"},
          {"name": "OSPI", "addr": "0xf1010000", "size": "64K", "nodeid": "0x1822402a"},
          {"name": "PMC_GPIO", "addr": "0xf1020000", "size": "64K", "nodeid": "0x1822402c"},
          {"name": "QSPI", "addr": "0xf1030000", "size": "64K", "nodeid": "0x1822402b"},
          {"name": "SD_eMMC0", "addr": "0xf1040000", "size": "64K", "nodeid": "0x1822402e"},
          {"name": "SD_eMMC1", "addr": "0xf1050000", "size": "64K", "nodeid": "0x1822402f"},
          {"name": "PMC_EFUSE_CACHE", "addr": "0xf1250000", "size": "64K", "nodeid": "0x18330054"},
          {"name": "PMC_SYSMON_CSR", "addr": "0xf1270000", "size": "64K", "nodeid": "0x18224055"},
          {"name": "PMC_RTC", "addr": "0xf12a0000", "size": "64K", "nodeid": "0x18224034"},
          {"name": "FPD_SWDT", "addr": "0xfd4d0000", "size": "64K", "nodeid": "0x18224029"},
          {"name": "UART0", "addr": "0xff000000", "size": "64K", "nodeid": "0x18224021"},
          {"name": "UART1", "addr": "0xff010000", "size": "64K", "nodeid": "0x18224022"},
          {"name": "LPD_I2C0", "addr": "0xff020000", "size": "64K", "nodeid": "0x1822401d"},
          {"name": "LPD_I2C1", "addr": "0xff030000", "size": "64K", "nodeid": "0x1822401e"},
          {"name": "SPI0", "addr": "0xff040000", "size": "64K", "nodeid": "0x1822401b"},
          {"name": "SPI1", "addr": "0xff050000", "size": "64K", "nodeid": "0x1822401c"},
          {"name": "CANFD0", "addr": "0xff060000", "size": "64K", "nodeid": "0x1822401f"},
          {"name": "CANFD1", "addr": "0xff070000", "size": "64K", "nodeid": "0x18224020"},
          {"name": "LPD_GPIO", "addr": "0xff0b0000", "size": "64K", "nodeid": "0x18224023"},
          {"name": "GEM0", "addr": "0xff0c0000", "size": "64K", "nodeid": "0x18224019"},
          {"name": "GEM1", "addr": "0xff0d0000", "size": "64K", "nodeid": "0x1822401a"},
          {"name": "TTC0", "addr": "0xff0e0000", "size": "64K", "nodeid": "0x18224024"},
          {"name": "TTC1", "addr": "0xff0f0000", "size": "64K", "nodeid": "0x18224025"},
          {"name": "TTC2", "addr": "0xff100000", "size": "64K", "nodeid": "0x18224026"},
          {"name": "TTC3", "addr": "0xff110000", "size": "64K", "nodeid": "0x18224027"},
          {"name": "LPD_SWDT", "addr": "0xff120000", "size": "64K", "nodeid": "0x18224028"},
          {"name": "IPI0", "addr": "0xff330000", "size": "64K", "nodeid": "0x1822403d"},
          {"name": "IPI1", "addr": "0xff340000", "size": "64K", "nodeid": "0x1822403e"},
          {"name": "IPI2", "addr": "0xff350000", "size": "64K", "nodeid": "0x1822403f"},
          {"name": "IPI3", "addr": "0xff360000", "size": "64K", "nodeid": "0x18224040"},
          {"name": "IPI4", "addr": "0xff370000", "size": "64K", "nodeid": "0x18224041"},
          {"name": "IPI5", "addr": "0xff380000", "size": "64K", "nodeid": "0x18224042"},
          {"name": "IPI6", "addr": "0xff3a0000", "size": "4K", "nodeid": "0x18224043"},
          {"name": "USB2_CSR", "addr": "0xff9d0000", "size": "64K", "nodeid": "0x18224018"},
          {"name": "LPD_DMA_CH0", "addr": "0xffa80000", "size": "64K", "nodeid": "0x18224035"},
          {"name": "LPD_DMA_CH1", "addr": "0xffa90000", "size": "64K", "nodeid": "0x18224036"},
          {"name": "LPD_DMA_CH2", "addr": "0xffaa0000", "size": "64K", "nodeid": "0x18224037"},
          {"name": "LPD_DMA_CH3", "addr": "0xffab0000", "size": "64K", "nodeid": "0x18224038"},
          {"name": "LPD_DMA_CH4", "addr": "0xffac0000", "size": "64K", "nodeid": "0x18224039"},
          {"name": "LPD_DMA_CH5", "addr": "0xffad0000", "size": "64K", "nodeid": "0x1822403a"},
          {"name": "LPD_DMA_CH6", "addr": "0xffae0000", "size": "64K", "nodeid": "0x1822403b"},
          {"name": "LPD_DMA_CH7", "addr": "0xffaf0000", "size": "64K", "nodeid": "0x1822403c"},
          {"name": "RPU0_TCMA_mem", "addr": "0xffe00000", "size": "64K", "mem": true, "nodeid": "0x1831800b"},
          {"name": "RPU0_TCMA_mem_lockstep", "addr": "0xffe10000", "size": "64K", "mem": true, "nodeid": "0x18900003"},
          {"name": "RPU0_TCMB_mem", "addr": "0xffe20000", "size": "64K", "mem": true, "nodeid": "0x1831800c"},
          {"name": "RPU0_TCMB_mem_lockstep", "addr": "0xffe30000", "size": "64K", "mem": true, "nodeid": "0x18900004"},
          {"name": "RPU0_iCACHE_mem", "addr": "0xffe40000", "size": "32K", "mem": true, "nodeid": "0x18900005"},
          {"name": "RPU0_dCACHE_mem", "addr": "0xffe50000", "size": "32K", "mem": true, "nodeid": "0x18900006"},
          {"name": "RPU1_TCMA_mem_dual", "addr": "0xffe90000", "size": "64K", "mem": true, "nodeid": "0x1831800d"},
          {"name": "RPU1_TCMB_mem_dual", "addr": "0xffeb0000", "size": "64K", "mem": true, "nodeid": "0x1831800e"},
          {"name": "OCM_mem", "addr": "0xfffc0000", "size": "256K", "mem": true, "nodeid": "0x18900007"},
          {"name": "OCM0_mem", "addr": "0xfffc0000", "size": "64K", "mem": true, "nodeid": "0x18314007"},
          {"name": "OCM1_mem", "addr": "0xfffd0000", "size": "64K", "mem": true, "nodeid": "0x18314008"},
          {"name": "OCM2_mem", "addr": "0xfffe0000", "size": "64K", "mem": true, "nodeid": "0x18314009"},
          {"name": "OCM3_mem", "addr": "0xffff0000", "size": "64K", "mem": true, "nodeid": "0x1831400a"}
        ]
      }
    },
    "base_protection": {
      "access": [
        {"same_as_default": "firmware_and_cci"},
        {"same_as_default": "firmware_and_cci.debug"},
        {"same_as_default": "firmware"},
        {"same_as_default": "firmware.debug"},
        {
          "name": "open_rw",
          "comment": "Any master can access these as read/write",
          "destinations": [
            "CRP",
            "CoreSight",
            "IPI_buffers",
            "IPI_channels",
            "OCM_CSR",
            "RPU_DUAL_CSR",
            "USB2_XHCI"
          ],
          "SMIDs": ["ANY"]
        },
        {"same_as_default": "open_rw_fpd"},
        {
          "name": "open_ro",
          "comment": "Any master can access these as read-only",
          "destinations": ["SCNTR_SECURE"],
          "flags": {"read_only": true},
          "SMIDs": ["ANY"]
        },
        {"same_as_default": "open_ro_2"},
        {"same_as_default": "apu_rw"},
        {
          "name": "bootmedia",
          "comment": "Allow booting from different boot media",
          "destinations": [
            "CFU_STREAM_mem",
            "PMC_OSPI_mem",
            "PPU",
            "PSM_DCACHE_mem",
            "PSM_ICACHE_mem",
            "RPU_Memory"
          ],
          "SMIDs": ["OSPI", "SD_eMMC"]
        },
        {
          "name": "bootmedia_pmc",
          "destinations": ["PMC_RAM_mem"],
          "SMIDs": ["OSPI", "SD_eMMC"]
        },
        {"same_as_default": "PLM_image_store"},
        {"same_as_default": "firmware_SLR1"},
        {"same_as_default": "firmware_SLR1.debug"},
        {"same_as_default": "firmware_SLR2"},
        {"same_as_default": "firmware_SLR2.debug"},
        {"same_as_default": "slr_ro"},
        {"same_as_default": "slr_ro_2"},
        {"same_as_default": "slr_rw"}
      ],
      "units": {
        "/axi_noc_cips/HBM0_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM0_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM10_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM10_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM11_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM11_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM12_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM12_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM13_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM13_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM14_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM14_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM15_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM15_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM1_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM1_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM2_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM2_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM3_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM3_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM4_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM4_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM5_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM5_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM6_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM6_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM7_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM7_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM8_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM8_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM9_PC0": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_cips/HBM9_PC1": {
          "root_access": "",
          "flags": {}
        },
        "/axi_noc_mc_ddr4_0/ddrmc": {
          "root_access": "firmware_and_cci",
          "flags": {}
        },
        "/axi_noc_mc_ddr4_1/ddrmc": {
          "root_access": "firmware_and_cci",
          "flags": {}
        },
        "FPD_XMPU": {"same_as_default": true},
        "IPI": {
          "flags": {"enable": true}
        },
        "IPI_PERM": {
          "flags": {}
        },
        "IPI_PERM_SLR1": {
          "flags": {}
        },
        "IPI_PERM_SLR2": {
          "flags": {}
        },
        "LPD_XPPU": {"same_as_default": true},
        "OCM_XMPU": {"same_as_default": true},
        "PLM": {
          "flags": {}
        },
        "PMC_EM": {
          "flags": {}
        },
        "PMC_XMPU": {"same_as_default": true},
        "PMC_XMPU_SLR1": {"same_as_default": true},
        "PMC_XMPU_SLR2": {"same_as_default": true},
        "PMC_XPPU": {"same_as_default": true},
        "PMC_XPPU_NPI": {"same_as_default": true},
        "PMC_XPPU_NPI_SLR1": {"same_as_default": true},
        "PMC_XPPU_NPI_SLR2": {"same_as_default": true},
        "PMC_XPPU_SLR1": {"same_as_default": true},
        "PMC_XPPU_SLR2": {"same_as_default": true},
        "PSM_EM": {
          "flags": {}
        },
        "SECURE_REG_ACCESS": {
          "root_access": "",
          "flags": {}
        },
        "SW_EM": {
          "flags": {}
        }
      }
    },
    "subsystems": {
      "subsystem_amr": {
        "id": "0x1c000006",
        "access": [
          {
            "name": "access_amr",
            "destinations": [
              "LPD_I2C0",
              "LPD_I2C1",
              "PMC_GPIO",
              "PMC_OSPI_mem",
              "RPU0_TCMA_mem",
              "RPU0_TCMA_mem_lockstep",
              "RPU0_TCMB_mem",
              "RPU0_TCMB_mem_lockstep",
              "RPU0_dCACHE_mem",
              "RPU0_iCACHE_mem",
              "SPI0"
            ],
            "flags": {"requested": true, "shared": true}
          },
          {
            "name": "perm_0",
            "type": "ss_permissions",
            "flags": {"powerdown": true, "wake": true},
            "domains": ["subsystem_apu", "subsystem_rpu1"]
          },
          {
            "name": "cpus_0",
            "type": "cpu_list",
            "SMIDs": ["RPU0"]
          },
          {
            "name": "access_amr_ttc",
            "destinations": ["TTC0"],
            "flags": {"requested": true}
          },
          {
            "name": "access_amr_ipi",
            "destinations": ["IPI3", "IPI4"],
            "flags": {"requested": true}
          },
          {
            "name": "access_amr_dram",
            "destinations": ["amr_mem_dram"]
          },
          {
            "name": "access_amr_ospi",
            "destinations": ["OSPI"],
            "flags": {"shared": true}
          }
        ]
      },
      "subsystem_apu": {
        "id": "0x1c000008",
        "access": [
          {
            "name": "access_apu",
            "destinations": ["PMC_I2C", "TTC2", "TTC3", "UART1"],
            "flags": {"requested": true}
          },
          {
            "name": "access_apu_ipi",
            "destinations": ["IPI0", "IPI1"],
            "flags": {"requested": true}
          },
          {
            "name": "access_apu_ocm",
            "destinations": ["OCM_mem"]
          },
          {
            "name": "access_apu_dram",
            "destinations": ["apu_mem_dram"]
          },
          {
            "name": "cpus_0",
            "type": "cpu_list",
            "SMIDs": ["APU"]
          }
        ]
      },
      "subsystem_rpu1": {
        "id": "0x1c000007",
        "access": [
          {
            "name": "access_rpu1_uart",
            "destinations": ["UART0"],
            "flags": {"requested": true, "shared": true}
          },
          {
            "name": "access_rpu1_ipi",
            "destinations": ["IPI5", "IPI6"],
            "flags": {"requested": true}
          },
          {
            "name": "access_rpu1_dram"
          },
          {
            "name": "cpus_0",
            "type": "cpu_list",
            "SMIDs": ["RPU1"]
          }
        ]
      }
    }
  }
}