/**
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * AMC Shared Memory Structure Definitions
 *
 * This header defines the shared memory layout between AMC (firmware) and AMI (host).
 * The shared memory partition table is initialized by AMC firmware at BAR0 + 0x1000.
 *
 * @file ami_amc_shared_mem.h
 */

#ifndef _AMI_AMC_SHARED_MEM_H_
#define _AMI_AMC_SHARED_MEM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/

#define AMC_MAGIC_NUMBER          0x564D5230  /* AMC shared memory magic "VMR0" */
#define AMC_STATUS_READY          0x1010101   /* AMC device ready status value */

/* BAR0 memory layout offsets */
#define AMC_SHARED_MEM_OFFSET     0x1000      /* Offset of shared memory structure in BAR0 */
#define AMC_GCQ_RING_OFFSET       0x2000      /* Offset of GCQ ring buffer in BAR0 */

/******************************************************************************/
/* Structs                                                                    */
/******************************************************************************/

/**
 * @struct amc_ring_buffer
 * @brief Ring buffer information from partition table
 */
struct amc_ring_buffer {
    uint32_t ring_buf_off;  /* Offset of GCQ ring buffer (from shared mem base) */
    uint32_t ring_buf_len;  /* Length of GCQ ring buffer */
};

/**
 * @struct amc_status
 * @brief AMC status information from partition table
 */
struct amc_status {
    uint32_t amc_status_off;  /* Offset of AMC device status */
    uint32_t amc_status_len;  /* Length of AMC device status */
};

/**
 * @struct amc_uuid
 * @brief AMC UUID information from partition table
 */
struct amc_uuid {
    uint32_t amc_uuid_off;  /* Offset of AMC UUID */
    uint32_t amc_uuid_len;  /* Length of AMC UUID (typically 16 bytes) */
};

/**
 * @struct amc_log_msg
 * @brief AMC log message information from partition table
 */
struct amc_log_msg {
    uint32_t log_msg_index;      /* Current index in log ring buffer */
    uint32_t log_msg_buf_off;    /* Offset of debug log buffer */
    uint32_t log_msg_buf_len;    /* Length of debug log buffer */
};

/**
 * @struct amc_data
 * @brief AMC data buffer information from partition table
 */
struct amc_data {
    uint32_t amc_data_start;  /* Start offset of data buffer */
    uint32_t amc_data_end;    /* End offset of data buffer */
};

/**
 * @struct amc_shared_mem
 * @brief Complete shared memory partition table
 *
 * This structure is positioned at BAR0 + 0x1000 (gcq_payload_base).
 * It is initialized by AMC firmware and read by AMI host software.
 */
struct amc_shared_mem {
    uint32_t                amc_magic_no;  /* Magic number: 0x564D5230 */
    struct amc_ring_buffer  ring_buf;      /* GCQ ring buffer info */
    struct amc_status       status;        /* AMC status info */
    struct amc_uuid         uuid;          /* AMC UUID info */
    struct amc_log_msg      log_msg;       /* AMC log info */
    struct amc_data         data;          /* AMC data buffer info */
};

#ifdef __cplusplus
}
#endif

#endif /* _AMI_AMC_SHARED_MEM_H_ */
