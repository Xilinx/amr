/**
 * Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the hal profile for the Rave
 *
 * @file profile_hal.h
 */

#ifndef _PROFILE_HAL_H_
#define _PROFILE_HAL_H_

#include "xparameters.h"
#include "profile.h"

#define HAL_PROFILE_RAVE                ( TRUE )

#define HAL_EMMC_FEATURE                ( 0 )
#define HAL_EMMC_BASE_ADDR              ( 0 )
#define HAL_EMMC_BLOCK_SIZE             ( 512 )
#define HAL_EMMC_MAX_BLOCKS             ( 0x7690000 )   /* 64 GBytes / 512 Bytes */
#define HAL_SMBUS_FEATURE               ( 0 )

#define HAL_EEPROM_VERSION              ( EEPROM_VERSION_1_0 )
#define HAL_EEPROM_I2C_BUS              ( 0 )
#define HAL_EEPROM_SLAVE_ADDRESS        ( 0x57 )
#define HAL_EEPROM_ADDRESS_SIZE         ( 2 )
#define HAL_EEPROM_PAGE_SIZE            ( 32 )
#define HAL_EEPROM_NUM_PAGES            ( 256 )
#define HAL_EEPROM_DEVICE_ID            ( 0x0A01 )
#define HAL_EEPROM_DEVICE_ID_ADDRESS    ( 0x1A )
#define HAL_EEPROM_DEVICE_ID_REGISTER   ( 0x07 )

/* Definitions for peripheral CIPS_PSPMC_0_PSV_I2C_0 */
#define HAL_I2C_BUS_0_DEVICE_ID         ( 0 )
#define HAL_I2C_BUS_0_BASEADDR          ( XPAR_XIICPS_0_BASEADDR )
#define HAL_I2C_BUS_0_HIGHADDR          ( XPAR_XIICPS_0_HIGHADDR )
#define HAL_I2C_BUS_0_I2C_CLK_FREQ_HZ   ( UTIL_100KHZ )
#define HAL_I2C_BUS_0_RESET_ON_INIT     ( TRUE )
#define HAL_I2C_BUS_0_HW_DEVICE_RESET   ( FALSE )

/* Definitions for peripheral CIPS_PSPMC_0_PSV_I2C_0 */
#define HAL_I2C_BUS_1_DEVICE_ID         ( 0 )
#define HAL_I2C_BUS_1_BASEADDR          ( XPAR_XIICPS_0_BASEADDR )
#define HAL_I2C_BUS_1_HIGHADDR          ( XPAR_XIICPS_0_HIGHADDR )
#define HAL_I2C_BUS_1_I2C_CLK_FREQ_HZ   ( UTIL_100KHZ )
#define HAL_I2C_BUS_1_RESET_ON_INIT     ( FALSE )
#define HAL_I2C_BUS_1_HW_DEVICE_RESET   ( TRUE )

#define HAL_I2C_SW_RESET_BASEADDR       ( XPAR_PSV_CRL_0_BASEADDR )
#define HAL_I2C_BUS_0_SW_RESET_OFFSET   ( 0x330 )
#define HAL_I2C_BUS_1_SW_RESET_OFFSET   ( 0x334 )
#define HAL_I2C_BUS_0_HW_RESET_ADDR     ( 0 )
#define HAL_I2C_BUS_0_HW_RESET_MASK     ( 0 )
#define HAL_I2C_BUS_1_HW_RESET_ADDR     ( 0xFF0B0040 )
#define HAL_I2C_BUS_1_HW_RESET_MASK     ( 1 << 13 )


#define HAL_I2C_DEFAULT_SCLK_RATE       ( 33333333 )
#define HAL_I2C_RETRY_COUNT             ( 5 )

#define HAL_I2C_MUXED_DEVICE            ( FALSE )
#define HAL_POWER_SENSORS_ENBALED       ( FALSE )
#define HAL_SYS_MON_RAVE                ( TRUE )

/* Definitions OSPI */
#define HAL_OSPI_0_DEVICE_ID            ( XPAR_OSPI_BASEADDR )
#define HAL_OSPI_PAGE_SIZE              ( 256 )
#define HAL_OSPI_RPU_BASE_ADDR          ( 0x0 )
#define HAL_OSPI_RPU_LENGTH             ( 0x08000000 )    /* 1Gb (128MB) */

/* Proxies */
/* APC */
#define HAL_APC_PMC_BOOT_REG            ( XPAR_PSV_PMC_GLOBAL_0_BASEADDR + 0x00004 )
#define HAL_APC_PDI_BIT_MASK            ( 0x14 )

/* Core libs */
/* PLL */
#define HAL_PLM_LOG_ADDRESS             ( 0xF2019000 )
#define HAL_PLM_LOG_SIZE                ( 0x4000 )

#endif /* _PROFILE_HAL_H_ */
