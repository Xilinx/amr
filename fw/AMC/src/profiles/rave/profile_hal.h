/**
 * Copyright (c) 2024 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the hal profile for the Rave
 *
 * @file profile_hal.h
 */

#ifndef _PROFILE_HAL_H_
#define _PROFILE_HAL_H_

#include "eeprom.h"
#include "xparameters.h"
#include "xil_io.h"
#include "xil_cache.h"

#define HAL_UUID_SIZE       ( 16 )
#define HAL_EMMC_FEATURE    ( 0 )
#define HAL_EMMC_DEVICE_ID  ( 0 )               /* XPAR_XSDPS_0_DEVICE_ID */
#define HAL_EMMC_BLOCK_SIZE ( 512 )
#define HAL_EMMC_MAX_BLOCKS ( 0x7690000 )       /* 64 GBytes / 512 Bytes */

#define HAL_SMBUS_FEATURE   ( 0 )

#define HAL_EEPROM_VERSION              ( EEPROM_VERSION_1_0 )
#define HAL_EEPROM_I2C_BUS              ( 0 )
#define HAL_EEPROM_SLAVE_ADDRESS        ( 0x57 )
#define HAL_EEPROM_ADDRESS_SIZE         ( 2 )
#define HAL_EEPROM_PAGE_SIZE            ( 32 )
#define HAL_EEPROM_NUM_PAGES            ( 256 )
#define HAL_EEPROM_DEVICE_ID            ( 0x0A01 )
#define HAL_EEPROM_DEVICE_ID_ADDRESS    ( 0x1A )
#define HAL_EEPROM_DEVICE_ID_REGISTER   ( 0x07 )

#define HAL_AMC_CLOCK_CONTROL           ( 1 )
#if ( 0 != HAL_AMC_CLOCK_CONTROL )
#ifdef XPAR_SHELL_UTILS_UCC_0_BASEADDR
#define HAL_USER_CLOCK_CONTROL_BASE_ADDRESS ( XPAR_SHELL_UTILS_UCC_0_BASEADDR )
#else
#define HAL_USER_CLOCK_CONTROL_BASE_ADDRESS ( 0x1 )
#endif
#endif

/* Apps */
/* AMC */
#define HAL_PARTITION_TABLE_SIZE                ( 0x1000 )
#define HAL_PARTITION_TABLE_MAGIC_NO            ( 0x564D5230 )
#define HAL_ENABLE_AMI_COMMS                    ( 0x1 )
#define HAL_RPU_RING_BUFFER_LEN                 ( 0x1000 )
#define HAL_BASE_LOGIC_GCQ_M2R_S01_AXI_BASEADDR ( 0x10000000 )
#define HAL_RPU_SHARED_MEMORY_BASE_ADDR         ( HAL_BASE_LOGIC_GCQ_M2R_S01_AXI_BASEADDR + 0x1000 )
#define HAL_RPU_SHARED_MEMORY_END_ADDR          ( 0x107FF000 )
#define HAL_RPU_SHARED_MEMORY_SIZE              ( HAL_RPU_SHARED_MEMORY_END_ADDR - HAL_RPU_SHARED_MEMORY_BASE_ADDR )
#define HAL_RPU_RING_BUFFER_BASE                ( HAL_RPU_SHARED_MEMORY_BASE_ADDR + HAL_PARTITION_TABLE_SIZE )
#define HAL_RPU_MEMORY_BUFFER_BASE              ( 0x800000 ) /* 8MB - 128MB RPU Memory (0-8MB RPU code/data) */

#define HAL_FLUSH_CACHE_DATA( addr, size ) Xil_DCacheFlushRange( addr, size )

/* Definitions for peripheral CIPS_PSPMC_0_PSV_I2C_0 */
#define HAL_I2C_BUS_0_DEVICE_ID         ( 0 )
#define HAL_I2C_BUS_0_BASEADDR          ( XPAR_XIICPS_0_BASEADDR )
#define HAL_I2C_BUS_0_HIGHADDR          ( XPAR_XIICPS_0_HIGHADDR )
#define HAL_I2C_BUS_0_I2C_CLK_FREQ_HZ   ( UTIL_100KHZ )
#define HAL_I2C_BUS_0_RESET_ON_INIT     ( TRUE )
#define HAL_I2C_BUS_0_HW_DEVICE_RESET   ( FALSE )

/* Definitions for peripheral CIPS_PSPMC_0_PSV_I2C_1 */
#define HAL_I2C_BUS_1_DEVICE_ID         ( 0 )
#define HAL_I2C_BUS_1_BASEADDR          ( XPAR_XIICPS_0_BASEADDR )
#define HAL_I2C_BUS_1_HIGHADDR          ( XPAR_XIICPS_0_HIGHADDR )
#define HAL_I2C_BUS_1_I2C_CLK_FREQ_HZ   ( UTIL_100KHZ )
#define HAL_I2C_BUS_1_RESET_ON_INIT     ( FALSE )
#define HAL_I2C_BUS_1_HW_DEVICE_RESET   ( TRUE )

#define HAL_I2C_SW_RESET_BASEADDR       ( XPAR_BLP_CIPS_PSPMC_0_PSV_CRL_0_BASEADDR )
#define HAL_I2C_BUS_0_SW_RESET_OFFSET   ( 0x330 )
#define HAL_I2C_BUS_1_SW_RESET_OFFSET   ( 0x330 )
#define HAL_I2C_BUS_0_HW_RESET_ADDR     ( 0 )
#define HAL_I2C_BUS_0_HW_RESET_MASK     ( 0 )
#define HAL_I2C_BUS_1_HW_RESET_ADDR     ( 0xFF0B0040 )
#define HAL_I2C_BUS_1_HW_RESET_MASK     ( 1 << 13 )


#define HAL_I2C_DEFAULT_SCLK_RATE       ( 33333333 )
#define HAL_I2C_RETRY_COUNT             ( 5 )

/* Definitions OSPI */
#define HAL_OSPI_0_DEVICE_ID            ( XPAR_OSPI_BASEADDR )

/* FAL */
/* sGCQ */
#ifndef HAL_IO_WRITE32
#define HAL_IO_WRITE32( val, addr ) ( { Xil_Out32( addr, val ); \
                                        Xil_DCacheFlushRange( addr, sizeof( uint32_t ) ); } )
#endif

#ifndef HAL_IO_WRITE32_NO_FLUSH
#define HAL_IO_WRITE32_NO_FLUSH( val, addr ) ( { Xil_Out32( addr, val ); } )
#endif

#ifndef HAL_IO_READ32
#define HAL_IO_READ32( addr ) ( { Xil_DCacheFlushRange( addr, sizeof( uint32_t ) ); \
                                  Xil_In32( addr ); } )
#endif

#ifndef HAL_IO_READ32_NO_FLUSH
#define HAL_IO_READ32_NO_FLUSH( addr ) ( { Xil_In32( addr ); } )
#endif

/* Proxies */
/* APC */
#define HAL_APC_PMC_BOOT_REG ( XPAR_BLP_CIPS_PSPMC_0_PSV_PMC_GLOBAL_0_BASEADDR + 0x00004 )
#define HAL_APC_PMC_SRST_REG ( XPAR_BLP_CIPS_PSPMC_0_PSV_PMC_GLOBAL_0_BASEADDR + 0x20084 )
#define HAL_APC_PDI_BIT_MASK ( 0x14 )

/* Core libs */
/* PLL */
#define HAL_PLM_LOG_ADDRESS ( 0xF2019000 )
#define HAL_PLM_LOG_SIZE    ( 0x4000 )

/**
 * @struct  HAL_PARTITION_TABLE_RING_BUFFER
 *
 * @brief   Stores the ring buffer info - part of the partition table.
 */
typedef struct HAL_PARTITION_TABLE_RING_BUFFER
{
    uint32_t ulRingBufferOff;
    uint32_t ulRingBufferLen;

} HAL_PARTITION_TABLE_RING_BUFFER;

/**
 * @struct  HAL_PARTITION_TABLE_STATUS
 *
 * @brief   Stores the AMC status info - part of the partition table.
 */
typedef struct HAL_PARTITION_TABLE_STATUS
{
    uint32_t ulStatusOff;
    uint32_t ulStatusLen;

} HAL_PARTITION_TABLE_STATUS;

/**
 * @struct  HAL_PARTITION_TABLE_UUID
 *
 * @brief   Stores the AMC UUID info - part of the partition table.
 */
typedef struct HAL_PARTITION_TABLE_UUID
{
    uint32_t ulUuidOff;
    uint32_t ulUuidLen;

} HAL_PARTITION_TABLE_UUID;

/**
 * @struct  HAL_PARTITION_TABLE_LOG_MSG
 *
 * @brief   Stores the AMC logs and info - part of the partition table.
 */
typedef struct HAL_PARTITION_TABLE_LOG_MSG
{
    uint32_t ulLogMsgIndex;
    uint32_t ulLogMsgBufferOff;
    uint32_t ulLogMsgBufferLen;

} HAL_PARTITION_TABLE_LOG_MSG;

/**
 * @struct  HAL_PARTITION_TABLE_DATA
 *
 * @brief   Stores the AMC data - part of the partition table.
 */
typedef struct HAL_PARTITION_TABLE_DATA
{
    uint32_t ulDataStart;
    uint32_t ulDataEnd;

} HAL_PARTITION_TABLE_DATA;

/**
 * @struct  HAL_PARTITION_TABLE
 *
 * @brief   Table stored at the top of the shared memory and used by
 *          AMI to read offsets & state.
 */
typedef struct HAL_PARTITION_TABLE
{
    uint32_t                        ulMagicNum;
    HAL_PARTITION_TABLE_RING_BUFFER xRingBuffer;
    HAL_PARTITION_TABLE_STATUS      xStatus;
    HAL_PARTITION_TABLE_UUID        xUuid;
    HAL_PARTITION_TABLE_LOG_MSG     xLogMsg;
    HAL_PARTITION_TABLE_DATA        xData;

} HAL_PARTITION_TABLE;

#endif /* _PROFILE_HAL_H_ */
