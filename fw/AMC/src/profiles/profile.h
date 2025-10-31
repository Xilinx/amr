/**
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the common hal profile
 *
 * @file profile.h
 */

#ifndef _PROFILE_H_
#define _PROFILE_H_

#include "standard.h"
#include "xil_io.h"
#include "xil_cache.h"

#define HAL_UUID_SIZE                   ( 16 )

/* Apps */
/* AMC */
#if 0
#define HAL_SHARED_MEM_TABLE_SIZE       ( 0x1000 )
#define HAL_SHARED_MEM_TABLE_MAGIC_NO   ( 0x564D5230 )
#define HAL_ENABLE_AMI_COMMS            ( 0x1 )
#define HAL_RPU_RING_BUFFER_LEN         ( 0x1000 )
#define HAL_GCQ_SHARED_BASEADDR         ( 0x10000000 )
#define HAL_RPU_SHARED_MEMORY_BASEADDR  ( HAL_GCQ_SHARED_BASEADDR + 0x1000 )
#define HAL_RPU_SHARED_MEMORY_ENDADDR   ( 0x107FF000 )
#define HAL_RPU_SHARED_MEMORY_SIZE      ( HAL_RPU_SHARED_MEMORY_ENDADDR  - HAL_RPU_SHARED_MEMORY_BASEADDR )
#define HAL_RPU_RING_BUFFER_BASE        ( HAL_RPU_SHARED_MEMORY_BASEADDR + HAL_SHARED_MEM_TABLE_SIZE )
#define HAL_RPU_MEMORY_BUFFER_BASE      ( 0x2000000 ) /* 32MB - 128MB RPU Memory (0-32MB RPU code/data) */
#endif

/* FAL */
#define HAL_FLUSH_CACHE_DATA( addr, size ) Xil_DCacheFlushRange( addr, size )

#define HAL_IO_WRITE32( val, addr ) \
    ( { Xil_Out32( addr, val );     \
        Xil_DCacheFlushRange( addr, sizeof( uint32_t ) ); } )

#define HAL_IO_WRITE32_NO_FLUSH( val, addr ) ( { Xil_Out32( addr, val ); } )

#define HAL_IO_READ32( addr )       \
    ( { Xil_DCacheFlushRange( addr, sizeof( uint32_t ) ); \
        Xil_In32( addr ); } )

/**
 * @struct  HALShmTableRingBuffer
 *
 * @brief   Stores the ring buffer info - part of the partition table.
 */
typedef struct
{
    uint32_t ulRingBufOffset;
    uint32_t ulRingBufLen;

} HALShmTableRingBuffer;

/**
 * @struct  HALShmTableStatus
 *
 * @brief   Stores the AMC status info - part of the partition table.
 */
typedef struct
{
    uint32_t ulStatusOff;
    uint32_t ulStatusLen;

} HALShmTableStatus;

/**
 * @struct  HALShmTableUUID
 *
 * @brief   Stores the AMC UUID info - part of the partition table.
 */
typedef struct
{
    uint32_t ulUuidOff;
    uint32_t ulUuidLen;

} HALShmTableUUID;

/**
 * @struct  HALShmTableLogMsg
 *
 * @brief   Stores the AMC logs and info - part of the partition table.
 */
typedef struct
{
    uint32_t ulLogMsgIndex;
    uint32_t ulLogMsgBufOffset;
    uint32_t ulLogMsgBufLen;

} HALShmTableLogMsg;

/**
 * @struct  HALShmTableData
 *
 * @brief   Stores the AMC data - part of the partition table.
 */
typedef struct
{
    uint32_t ulDataStart;
    uint32_t ulDataEnd;

} HALShmTableData;

/**
 * @struct  HALShmTable
 *
 * @brief   Table stored at the top of the shared memory and used by
 *          AMI to read offsets & state.
 */
typedef struct
{
    uint32_t              ulMagicNum;
    HALShmTableRingBuffer xRingBuf;
    HALShmTableStatus     xStatus;
    HALShmTableUUID       xUuid;
    HALShmTableLogMsg     xLogMsg;
    HALShmTableData       xData;

} HALShmTable;

#endif /* _PROFILE_H_ */
