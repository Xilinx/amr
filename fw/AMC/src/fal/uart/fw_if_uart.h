/**
 * Copyright (c) 2024 - 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This header file contains the FW IF UART abstraction definitions.
 *
 * @file fw_if_uart.h
 */

#ifndef _FW_IF_UART_H_
#define _FW_IF_UART_H_

#include "fw_if.h"


/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/**
 * @enum   FW_IF_UART_ERRORS
 * @brief  Enumeration of UART return values
 */
typedef enum
{
    FW_IF_UART_ERRORS_INVALID_STATE = MAX_FW_IF_ERROR,
    FW_IF_UART_ERRORS_DRIVER_FAILURE,

    MAX_FW_IF_UART_ERRORS

} FW_IF_UART_ERRORS;

/**
 * @enum   FW_IF_UART_STATE
 * @brief  ioctrl options for UART interfaces (generic across all UART interfaces)
 */
typedef enum
{
    FW_IF_UART_STATE_INIT,
    FW_IF_UART_STATE_CREATE,
    FW_IF_UART_STATE_OPENED,
    FW_IF_UART_STATE_CLOSED,
    FW_IF_UART_STATE_ERROR,

    MAX_FW_IF_UART_STATE

} FW_IF_UART_STATE;


/**
 * @enum    FW_IF_UART_DATA_BITS
 * @brief   number of data bits for the UART configuration
*/
typedef enum
{
    FW_IF_UART_6_BITS,
    FW_IF_UART_7_BITS,
    FW_IF_UART_8_BITS,

    MAX_FW_IF_UART_DATA_BITS

} FW_IF_UART_DATA_BITS;


/**
 * @enum    FW_IF_UART_STOP_BITS
 * @brief   number of stop bits for the UART configuration
*/
typedef enum
{
    FW_IF_UART_1_STOP_BIT,
    FW_IF_UART_2_STOP_BITS,

    MAX_FW_IF_UART_STOP_BITS

} FW_IF_UART_STOP_BITS;


/**
 * @enum    FW_IF_UART_PARITY
 * @brief   parity options for the UART configuration
*/
typedef enum
{
    FW_IF_UART_PARITY_NONE,
    FW_IF_UART_PARITY_ODD,
    FW_IF_UART_PARITY_EVEN,
    FW_IF_UART_PARITY_MARK,
    FW_IF_UART_PARITY_SPACE,

    MAX_FW_IF_UART_PARITY

} FW_IF_UART_PARITY;


/**
 * @enum    FW_IF_UART_FLOW_CTRL
 * @brief   flow control options for the UART configuration
*/
typedef enum
{
    FW_IF_UART_FLOW_CTRL_NONE,
    FW_IF_UART_FLOW_CTRL_XONOFF,
    FW_IF_UART_FLOW_CTRL_RTS_CTS,
    FW_IF_UART_FLOW_CTRL_DSR_DTR,

    MAX_FW_IF_UART_FLOW_CTRL

} FW_IF_UART_FLOW_CTRL;


/*****************************************************************************/
/* Structs                                                                   */
/*****************************************************************************/

/**
 * @struct  FWIfUartInitCfg
 * @brief   Config options for UART initialisation (generic across all UART interfaces)
 */
typedef struct
{
    uint8_t     ucUartDeviceId;
    uint32_t    ulBaseAddr;

} FWIfUartInitCfg;

/**
 * @struct  FWIfUartCfg
 * @brief   Config options for UART interfaces (generic across all UART interfaces)
 */
typedef struct
{
    uint32_t                ulPort;
    uint32_t                ulBaudRate;
    FW_IF_UART_DATA_BITS    xDataBits;
    FW_IF_UART_STOP_BITS    xStopBits;
    FW_IF_UART_PARITY       xParity;
    FW_IF_UART_FLOW_CTRL    xFlowCtrl;
    FW_IF_UART_STATE        xState;

} FWIfUartCfg;


/*****************************************************************************/
/* Public Functions                                                          */
/*****************************************************************************/

/**
 * @brief   Initialisation function for UART interfaces (generic across all UART unterfaces)
 *
 * @param   pxInitCfg   pointer to the config to initialise the driver with
 *
 * @return  See FW_IF_ERRORS
 */
uint32_t ulFW_IF_UART_Init( FWIfUartInitCfg *pxInitCfg );

/**
 * @brief   Creates an instance of the UART interface
 *
 * @param   fwIf        fw_if handle to the interface instance
 * @param   uartCfg     unique data of this instance (port, address, etc)
 *
 * @return  See FW_IF_ERRORS
*/
uint32_t ulFW_IF_UART_Create( FWIfCfg *pxFwIf, FWIfUartCfg *pxUartCfg );

/**
 *
 * @brief    Print all the stats gathered by the interface
 *
 * @return   OK                  Stats retrieved from interface successfully
 *           ERROR               Stats not retrieved successfully
 */
int iFW_IF_UART_PrintStatistics( void );

/**
 *
 * @brief    Clears all the stats gathered by the interface
 *
 * @return   OK                  Stats cleared successfully
 *           ERROR               Stats not cleared successfully
 */
int iFW_IF_UART_ClearStatistics( void );

#endif /* _FW_IF_UART_H_ */
