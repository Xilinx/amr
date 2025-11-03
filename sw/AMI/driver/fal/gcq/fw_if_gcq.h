/**
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This header file containing the FW IF sGCQ abstraction definitions.
 *
 * @file fw_if_gcq.h
 */

#ifndef _FW_IF_GCQ_H_
#define _FW_IF_GCQ_H_


#include "fw_if.h"


/*****************************************************************************/
/* Defines                                                                   */
/*****************************************************************************/

#define FW_IF_GCQ_UDID_LEN                  ( 16 )


/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/**
 * @enum FW_IF_GCQ_ERROR_TYPE
 * @brief Enumeration of sGCQ return values
 */
typedef enum
{
    FW_IF_GCQ_ERRORS_NONE             = FW_IF_ERRORS_NONE,
    FW_IF_GCQ_ERRORS_NO_FREE_PROFILES = MAX_FW_IF_ERROR,
    FW_IF_GCQ_ERRORS_INVALID_PROFILE,
    FW_IF_GCQ_ERRORS_NOT_SUPPORTED,
    FW_IF_GCQ_ERRORS_DRIVER_NOT_INITIALISED,
    FW_IF_GCQ_ERRORS_DRIVER_NO_FREE_INSTANCES,
    FW_IF_GCQ_ERRORS_DRIVER_INVALID_INSTANCE,
    FW_IF_GCQ_ERRORS_DRIVER_INVALID_ARG,
    FW_IF_GCQ_ERRORS_DRIVER_INVALID_SLOT_SIZE,
    FW_IF_GCQ_ERRORS_DRIVER_INVALID_VERSION,
    FW_IF_GCQ_ERRORS_DRIVER_INVALID_NUM_SLOTS,
    FW_IF_GCQ_ERRORS_DRIVER_CONSUMER_NOT_ATTACHED,
    FW_IF_GCQ_ERRORS_DRIVER_CONSUMER_NOT_AVAILABLE,
    FW_IF_GCQ_ERRORS_DRIVER_CONSUMER_NO_DATA_RECEIVED,
    FW_IF_GCQ_ERRORS_DRIVER_PRODUCER_NO_FREE_SLOTS,

    MAX_FW_IF_GCQ_ERROR

} FW_IF_GCQ_ERRORS_TYPE;

/**
 * @enum FW_IF_GCQ_IOCTL_TYPE
 * @brief ioctrl options for sGCQ interfaces (generic across all sGCQ interfaces)
 */
typedef enum
{
    FW_IF_GCQ_IOCTRL_SET_OPAQUE_HANDLE = MAX_FW_IF_COMMON_IOCTRL_OPTION,
    FW_IF_GCQ_IOCTRL_GET_OPAQUE_HANDLE,

    MAX_FW_IF_GCQ_IOCTRL_OPTION

} FW_IF_GCQ_IOCTL_TYPE;


/*****************************************************************************/
/* Structs                                                                   */
/*****************************************************************************/

/**
 * @struct  FW_IF_GCQInitCfg
 * @brief   config options for sGCQ initialisation (generic across all sGCQ interfaces)
 */
typedef struct
{
    void                *pvIOAccess;

} FW_IF_GCQInitCfg;

/**
 * @struct  FW_IF_GCQCfg
 * @brief   config options for sGCQ interfaces (generic across all sGCQ interfaces)
 */
typedef struct
{
    uint64_t            ullBaseAddress;
    uint64_t            ullRingAddress;
    uint32_t            ulRingLength;
    uint32_t            ulCompletionQueueSlotSize;
    uint32_t            ulSubmissionQueueSlotSize;
    uint8_t             udid[FW_IF_GCQ_UDID_LEN];

    void                *pvProfile;      /* opaque handle to store internal context */

} FW_IF_GCQCfg;


/*****************************************************************************/
/* Public Functions                                                          */
/*****************************************************************************/

/**
 * @brief   initialisation function for sGCQ interfaces (generic across all sGCQ interfaces)
 *
 * @param   xInitCfg    pointer to the config to initialise the driver with
 *
 * @return  See FW_IF_ERRORS
 */
uint32_t ulFW_IF_GCQ_Init(FW_IF_GCQInitCfg *pxInitCfg);

/**
 * @brief   creates an instance of the sGCQ interface
 *
 * @param   xFWIf       fw_if handle to the interface instance
 * @param   xGCQCfg     unique data of this instance
 *
 * @return  See FW_IF_ERRORS
 */
uint32_t ulFW_IF_GCQ_Create(FW_IF_CFG *pxFwIf, FW_IF_GCQCfg *pxGCQCfg);

#endif /* _FW_IF_GCQ_H_ */
