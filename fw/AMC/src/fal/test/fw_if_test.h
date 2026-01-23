/**
* Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
* SPDX-License-Identifier: MIT
*
* This header file contains the FW IF test interface definitions.
*
* @file fw_if_test.h
*/

#ifndef _FW_IF_TEST_H_
#define _FW_IF_TEST_H_

#include "fw_if.h"


/*****************************************************************************/
/* defines                                                                   */
/*****************************************************************************/

#define FW_IF_TEST_MAX_DATA    ( 256 )


/*****************************************************************************/
/* enums                                                                     */
/*****************************************************************************/

/**
 * @enum    FW_IF_TEST_IOCTRL_OPTION
 * @brief   ioctrl options for test interfaces (generic across all test interfaces)
 */
typedef enum
{
    FW_IF_TEST_IOCTRL_ENABLE_DEBUG_PRINT = MAX_FW_IF_COMMON_IOCTRL_OPTION,
    FW_IF_TEST_IOCTRL_DISABLE_DEBUG_PRINT,

    FW_IF_TEST_IOCTRL_SET_NEXT_RX_DATA,
    FW_IF_TEST_IOCTRL_SET_NEXT_RX_SIZE,

    FW_IF_TEST_IOCTRL_TRIGGER_EVENT,

    MAX_FW_IF_TEST_IOCTRL_OPTION

} FW_IF_TEST_IOCTRL_OPTIONS;


/*****************************************************************************/
/* structs                                                                   */
/*****************************************************************************/

/**
 * @struct  FWIfTestInitCfg
 * @brief   config options for test initialisation (generic across all test interfaces)
 */
typedef struct
{
    uint32_t  driverId;
    char      *driverName;
    int       debugPrint;

} FWIfTestInitCfg;

/**
 * @struct  FWIfTestCfg
 * @brief   config options for test interfaces (generic across all test interfaces)
 */
typedef struct
{
    uint32_t  ifId;
    char      *ifName;

} FWIfTestCfg;


/*****************************************************************************/
/* public functions                                                          */
/*****************************************************************************/

/**
 * @brief   initialisation function for test interfaces (generic across all test interfaces)
 *
 * @param   pxCfg       pointer to the config to initialise the driver with
 *
 * @return  See FW_IF_ERRORS
 */
extern uint32_t FW_IF_test_init( FWIfTestInitCfg *pxCfg );

/**
 * @brief   creates an instance of the test interface
 *
 * @param   fwIf        fw_if handle to the interface instance
 * @param   testCfg     unique data of this instance (port, address, etc)
 *
 * @return  See FW_IF_ERRORS
 */
extern uint32_t FW_IF_test_create( FWIfCfg *pxFwIf, FWIfTestCfg *pxTestCfg );

#endif /* _FW_IF_TEST_H_ */
