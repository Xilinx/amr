/**
 * Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the SMBus IP register offsets and bit mask defintions
 * for the SMBus driver.
 *
 * @file smbus_hw_access.h
 */

#ifndef _SMBus_HW_ACCESS_H_
#define _SMBus_HW_ACCESS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "smbus.h"
#include "smbus_internal.h"
#include "smbus_hw.h"

#define SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS     ( 0 )
#define SMBUS_HW_DESCRIPTOR_WRITE_FAIL        ( 1 )

typedef volatile int* SMBUS_BASE_ADDRESS_TYPE;

typedef enum
{
    DESC_TARGET_READ            = 0,
    DESC_TARGET_READ_PEC        = 1,
    DESC_TARGET_WRITE_ACK       = 2,
    DESC_TARGET_WRITE_NACK      = 3,
    DESC_TARGET_WRITE_PEC       = 4,
    DESC_CONTROLLER_READ_START  = 5,
    DESC_CONTROLLER_READ_QUICK  = 6,
    DESC_CONTROLLER_READ_BYTE   = 7,
    DESC_CONTROLLER_READ_STOP   = 8,
    DESC_CONTROLLER_READ_PEC    = 9,
    DESC_CONTROLLER_WRITE_START = 10,
    DESC_CONTROLLER_WRITE_QUICK = 11,
    DESC_CONTROLLER_WRITE_BYTE  = 12,
    DESC_CONTROLLER_WRITE_STOP  = 13,
    DESC_CONTROLLER_WRITE_PEC   = 14,
    DESC_MAX                    = 15

} SMBUS_HW_DESCRIPTOR_TYPE;

/**
 * @brief    Reads the hardware register SMBUS_REG_IP_VERSION
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadIPVersion( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_IP_REVISION
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadIPRevision( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_IP_MAGIC_NUM
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadIPMagicNum( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_IP_BUILD_CONFIG_0
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadBuildConfig0( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_IP_BUILD_CONFIG_1
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadBuildConfig1( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the GIE_ENABLE bitfield from hardware register SMBUS_REG_IRQ_GIE
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadIRQGIEEnable( SMBusProfile* pxSMBusProfile );

/**
 *
 * @brief    Reads the hardware register SMBUS_REG_IRQ_IER
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadIRQIER( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_IRQ_ISR
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadIRQISR( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_ERR_IRQ_IER
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadErrIRQIER( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_ERR_IRQ_ISR
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadErrIRQISR( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_PHY_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYStatus( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the SMBUS_PHY_STATUS_SMBDAT_LOW_TIMEOUT bitfield from hardware
 *           register SMBUS_REG_PHY_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYStatusSMBDATLowTimeout( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the SMBUS_PHY_STATUS_SMBCLK_LOW_TIMEOUT bitfield from hardware
 *           register SMBUS_REG_PHY_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
* @return   uint32_t register value read
*/
uint32_t ulSMBusHWReadPHYStatusSMBClkLowTimeout( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the SMBUS_PHY_STATUS_BUS_IDLE bitfield from hardware register
 *           SMBUS_REG_PHY_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYStatusBusIdle( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_PHY_FILTER_CONTROL
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYFilterControl( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the SMBUS_PHY_FILTER_CONTROL_ENABLE bitfield from hardware
 *           register SMBUS_REG_PHY_FILTER_CONTROL
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYFilterControlEnable( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the SMBUS_PHY_FILTER_CONTROL_DURATION bitfield from hardware
 *           register SMBUS_REG_PHY_FILTER_CONTROL
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYFilterControlDuration( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_PHY_BUS_FREE_TIME
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYBusFreetime( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the IDLE_THRESHOLD bitfield from hardware register
 *           SMBUS_REG_PHY_IDLE_THRESHOLD
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYIdleThresholdIdleThreshold( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_PHY_TIMEOUT_PRESCALER
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYTimeoutPrescaler( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_PHY_TIMEOUT_MIN
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
*/
uint32_t ulSMBusHWReadPHYTimeoutMin( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the MIN_TIMEOUT_ENABLE bitfield from hardware register
 *           SMBUS_REG_PHY_TIMEOUT_MIN
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYTimeoutMinTimeoutEnable( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the MIN_TIMEOUT_MIN bitfield from hardware register
 *           SMBUS_REG_PHY_TIMEOUT_MIN
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYTimeoutMinTimeoutMin( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_PHY_TIMEOUT_MAX
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYTimeoutMax( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the SMBCLK_FORCE_LOW bitfield from hardware register
 *           SMBUS_REG_PHY_RESET_CONTROL
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYResetControlSMBClkForce( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the TGT_DATA_SETUP bitfield from hardware register
 *           SMBUS_REG_PHY_TGT_DATA_SETUP
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYTgtDataSetupTgtDataSetup( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the TGT_TEXT_PRESCALER bitfield from hardware register
 *           SMBUS_REG_PHY_TGT_TEXT_PRESCALER
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYTgtTextPrescaler( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the TGT_TEXT_TIMEOUT bitfield from hardware register
 *           SMBUS_REG_PHY_TGT_TEXT_TIMEOUT
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYTgtTextTimeout( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the TGT_TEXT_MAX bitfield from hardware register
 *           SMBUS_REG_PHY_TGT_TEXT_MAX
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYTgtTextMax( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the DBG_STATE bitfield from hardware register
 *           SMBUS_REG_PHY_TGT_DBG_STATE
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYTgtDbgState( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the DATA_HOLD bitfield from hardware register
 *           SMBUS_REG_PHY_TGT_DATA_HOLD
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYTgtDataHold( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_TGT_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtStatus( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the TGT_STATUS_ACTIVE bitfield from hardware register
 *           SMBUS_REG_TGT_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtStatusActive( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the TGT_STATUS_ADDRESS bitfield from hardware register
 *           SMBUS_REG_TGT_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtStatusAddress( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the TGT_STATUS_RW bitfield from hardware register
 *           SMBUS_REG_TGT_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtStatusRW( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the FILL_LEVEL bitfield from hardware register
 *           SMBUS_REG_TGT_DESC_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 *
 */
uint32_t ulSMBusHWReadTgtDescStatusFillLevel( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the FULL bitfield from hardware register
 *           SMBUS_REG_TGT_DESC_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtDescStatusFull( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ALMOST_FULL bitfield from hardware register
 *           SMBUS_REG_TGT_DESC_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtDescStatusAlmostFull( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ALMOST_EMPTY bitfield from hardware register
 *           SMBUS_REG_TGT_DESC_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtDescStatusAlmostEmpty( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the EMPTY bitfield from hardware register
 *           SMBUS_REG_TGT_DESC_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtDescStatusEmpty( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the PAYLOAD bitfield from hardware register SMBUS_REG_TGT_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
 uint32_t ulSMBusHWReadTgtRxFifoPayload( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the FILL_LEVEL bitfield from hardware register SMBUS_REG_TGT_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 *
 */
uint32_t ulSMBusHWReadTgtRxFifoStatusFillLevel( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the RESET_BUSY bitfield from hardware register SMBUS_REG_TGT_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtRxFifoStatusResetBusY( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the FULL bitfield from hardware register SMBUS_REG_TGT_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtRxFifoStatusFull( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ALMOST_FULL bitfield from hardware register SMBUS_REG_TGT_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtRxFifoStatusAlmostFull( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ALMOST_EMPTY bitfield from hardware register SMBUS_REG_TGT_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtRxFifoStatusAlmostEmpty( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the EMPTY bitfield from hardware register SMBUS_REG_TGT_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtRxFifoStatusEmpty( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the FILL_THRESHOLD bitfield from hardware register
 *           SMBUS_REG_TGT_RX_FIFO_FILL_THRESHOLD
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtRxFifoFillThresholdFillThresh( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the hardware register SMBUS_REG_TGT_DBG
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtDbg( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ENABLE bitfield from hardware register SMBUS_REG_TGT_CONTROL_0
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl0Enable( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ADDRESS bitfield from hardware register SMBUS_REG_TGT_CONTROL_0
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl0Address( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ENABLE bitfield from hardware register SMBUS_REG_TGT_CONTROL_1
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl1Enable( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ADDRESS bitfield from hardware register SMBUS_REG_TGT_CONTROL_1
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl1Address( SMBusProfile* pxSMBusProfile );

/**
* @brief    Reads the ENABLE bitfield from hardware register SMBUS_REG_TGT_CONTROL_2
*
* @param    pxSMBusProfile is a pointer to the SMBus profile structure.
*
* @return   uint32_t register value read
*/
uint32_t ulSMBusHWReadTgtControl2Enable( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ADDRESS bitfield from hardware register SMBUS_REG_TGT_CONTROL_2
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl2Address( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ENABLE bitfield from hardware register SMBUS_REG_TGT_CONTROL_3
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl3Enable( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ADDRESS bitfield from hardware register SMBUS_REG_TGT_CONTROL_3
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl3Address( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ENABLE bitfield from hardware register SMBUS_REG_TGT_CONTROL_4
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl4Enable( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ADDRESS bitfield from hardware register SMBUS_REG_TGT_CONTROL_4
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl4Address( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ENABLE bitfield from hardware register SMBUS_REG_TGT_CONTROL_5
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl5Enable( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ADDRESS bitfield from hardware register SMBUS_REG_TGT_CONTROL_5
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl5Address( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ENABLE bitfield from hardware register SMBUS_REG_TGT_CONTROL_6
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl6Enable( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ADDRESS bitfield from hardware register SMBUS_REG_TGT_CONTROL_6
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl6Address( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ENABLE bitfield from hardware register SMBUS_REG_TGT_CONTROL_7
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl7Enable( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ADDRESS bitfield from hardware register SMBUS_REG_TGT_CONTROL_7
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadTgtControl7Address( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the DATA_HOLD bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_DATA_HOLD
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlDataHold( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the START_HOLD bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_START_HOLD
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlStartHold( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the START_SETUP bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_START_SETUP
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlStartSetup( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the STOP_SETUP bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_STOP_SETUP
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlStopSetup( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the CLK_TLOW bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_CLK_TLOW
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlClkTLow( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the CLK_THIGH bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_CLK_THIGH
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlClkTHigh( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the TEXT_PRESCALER bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_TEXT_PRESCALER
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlTextPrescaler( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the TEXT_TIMEOUT bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_TEXT_TIMEOUT
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlTextTimeout( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the TEXT_MAX bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_TEXT_MAX
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlTextMax( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the CEXT_PRESCALER bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_CEXT_PRESCALER
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlCextPrescaler( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the CEXT_TIMEOUT bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_CEXT_TIMEOUT
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlCextTimeout( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the CEXT_MAX bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_CEXT_MAX
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlCextMax( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the DBG_STATE bitfield from hardware register
 *           SMBUS_REG_PHY_CTLR_DBG_STATE
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadPHYCtrlDbgState( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ENABLE bitfield from hardware register SMBUS_REG_CTLR_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlStatusEnable( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the FILL_LEVEL bitfield from hardware register
 *           SMBUS_REG_CTLR_DESC_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlDescStatusFillLevel( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the RESET_BUSY bitfield from hardware register
 *           SMBUS_REG_CTLR_DESC_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlDescStatusResetBusy( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the FULL bitfield from hardware register
 *           SMBUS_REG_CTLR_DESC_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlDescStatusFull( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ALMOST_FULL bitfield from hardware register
 *           SMBUS_REG_CTLR_DESC_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlDescStatusAlmostFull( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ALMOST_EMPTY bitfield from hardware register
 *           SMBUS_REG_CTLR_DESC_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlDescStatusAlmostEmpty( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the EMPTY bitfield from hardware register
 *           SMBUS_REG_CTLR_DESC_STATUS
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlDescStatusEmpty( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the PAYLOAD bitfield from hardware register
 *           SMBUS_REG_CTLR_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlRxFifoPayload( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the FILL_LEVEL bitfield from hardware register
 *           SMBUS_REG_CTLR_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlRxFifoStatusFillLevel( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the RESET_BUSY bitfield from hardware register
 *           SMBUS_REG_CTLR_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlRxFifoStatusResetBusy( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the FULL bitfield from hardware register SMBUS_REG_CTLR_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlRxFifoStatusFull( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ALMOST_FULL bitfield from hardware register
 *           SMBUS_REG_CTLR_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlRxFifoStatusAlmostFull( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the ALMOST_EMPTY bitfield from hardware register
 *           SMBUS_REG_CTLR_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlRxFifoStatusAlmostEmpty( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the EMPTY bitfield from hardware register
 *           SMBUS_REG_CTLR_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlRxFifoStatusEmpty( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the THRESHOLD bitfield from hardware register
 *           SMBUS_REG_CTLR_RX_FIFO_FILL_THRESHOLD
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlRxFifoFillThresholdFillThresh( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Reads the DBG_STATE bitfield from hardware register
 *           SMBUS_REG_CTLR_DBG
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   uint32_t register value read
 */
uint32_t ulSMBusHWReadCtrlDbgState( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_IRQ_GIE
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteIRQGIEEnable( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_IRQ_IER
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteIRQIER( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_IRQ_ISR
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteIRQISR( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_ERR_IRQ_IER
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteERRIRQIER( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_ERR_IRQ_ISR
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteERRIRQISR( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to ENABLE bitfield of hardware register
 *           SMBUS_REG_PHY_FILTER_CONTROL
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYFilterControlEnable( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to DURATION bitfield of hardware register
 *           SMBUS_REG_PHY_FILTER_CONTROL
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYFilterControlDuration( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_BUS_FREE_TIME
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYBusFreetime( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to IDLE_THRESHOLD bitfield of hardware register
 *           SMBUS_REG_PHY_IDLE_THRESHOLD
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYIdleThresholdIdleThreshold( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_TIMEOUT_PRESCALER
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYTimeoutPrescaler( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_TIMEOUT_MIN
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYTimeoutMin( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to ENABLE bitfield of hardware register
 *           SMBUS_REG_PHY_TIMEOUT_MIN
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYTimeoutMinTimeoutEnable( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to TIMEOUT_MIN bitfield of hardware register
 *           SMBUS_REG_PHY_TIMEOUT_MIN
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYTimeoutMinTimeoutMin( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_TIMEOUT_MAX
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYTimeoutMax( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to SMBCLK_FORCE_TIMEOUT bitfield of hardware register
 *           SMBUS_REG_PHY_RESET_CONTROL
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYResetControlSMBClkForceTimeout( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
* @brief    Writes ulValue to SMBCLK_FORCE_LOW bitfield of hardware register
*           SMBUS_REG_PHY_RESET_CONTROL
*
* @param    pxSMBusProfile is a pointer to the SMBus profile structure.
* @param    ulValue value to write to the register bitfield
*
* @return   None
*/
void vSMBusHWWritePHYResetControlSMBClkForceLow( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to TGT_DATA_SETUP bitfield of hardware register
 *           SMBUS_REG_PHY_TGT_DATA_SETUP
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYTgtDataSetupTgtDataSetup( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_TGT_TEXT_PRESCALER
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYTgtTextPrescaler( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_TGT_TEXT_TIMEOUT
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYTgtTextTimeout( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_TGT_TEXT_MAX
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYTgtTextMax( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
* @brief    Writes ulValue to hardware register SMBUS_REG_PHY_TGT_DATA_HOLD
*
* @param    pxSMBusProfile is a pointer to the SMBus profile structure.
* @param    ulValue value to write to the register bitfield
*
* @return   None
*

*
*/
void vSMBusHWWritePHYTgtDataHold( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_TGT_DESC_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteTgtDescFifo( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to FIFO_ID bitfield of hardware register
 *           SMBUS_REG_TGT_DESC_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteTgtDescFifoId( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to PAYLOAD bitfield of hardware register
 *           SMBUS_REG_TGT_DESC_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteTgtDescFifoPayload( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to RESET bitfield of hardware register
 *           SMBUS_REG_TGT_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteTgtRxFifoReset( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to THRESHOLD bitfield of hardware register
 *           SMBUS_REG_TGT_RX_FIFO_FILL_THRESHOLD
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteRxFifoFillThresholdFillThresh( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Passes the ulValue to the desired TgtControl register dependent
 *           on the instance value
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucInstance is SMBus instance to use
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteTgtControlEnable( SMBusProfile* pxSMBusProfile, uint8_t ucInstance, uint32_t ulValue );

/**
 * @brief    Passes the ulValue to the desired TgtControl register dependent
 *           on the instance value
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucInstance is SMBus instance to use
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
*/
void vSMBusHWWriteTgtControlAddress( SMBusProfile* pxSMBusProfile, uint8_t ucInstance, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_CTLR_DATA_HOLD
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYCtrlDataHold( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_CTLR_START_HOLD
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYCtrlStartHold( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_CTLR_START_SETUP
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYCtrlStartSetup( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_CTLR_STOP_SETUP
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYCtrlStopSetup( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
* @brief    Writes ulValue to hardware register SMBUS_REG_PHY_CTLR_CLK_TLOW
*
* @param    pxSMBusProfile is a pointer to the SMBus profile structure.
* @param    ulValue value to write to the register bitfield
*
* @return   None
*/
void vSMBusHWWritePHYCtrlClkTLow( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_CTLR_CLK_THIGH
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYCtrlClkTHigh( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_CTLR_TEXT_PRESCALER
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYCtrlTextPrescaler( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/*``*
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_CTLR_TEXT_TIMEOU``T
 ``*
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure``.
 * @param    ulValue value to write to the register bitfiel``d
 ``*
 * @return   Non``e
 */
void vSMBusHWWritePHYCtrlTextTimeout( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_CTLR_TEXT_MAX
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYCtrlTextMax( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_CTLR_CEXT_PRESCALER
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYCtrlCextPrescaler( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_CTLR_CEXT_TIMEOUT
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYCtrlCextTimeout( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_PHY_CTLR_CEXT_MAX
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWritePHYCtrlCextMax( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_CTLR_CONTROL
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteCtrlControlEnable( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to hardware register SMBUS_REG_CTLR_DESC_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteCtrlDescFifo( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to RESET bitfield of hardware register SMBUS_REG_CTLR_DESC_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteCtrlDescFifoReset( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to RESET bitfield of hardware register
 *           SMBUS_REG_CTLR_RX_FIFO
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteCtrlRxFifoReset( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/**
 * @brief    Writes ulValue to FILL_THRESHOLD bitfield of hardware register
 *           SMBUS_REG_CTLR_RX_FIFO_FILL_THRESHOLD
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ulValue value to write to the register bitfield
 *
 * @return   None
 */
void vSMBusHWWriteCtrlRxFifoFillThreshold( SMBusProfile* pxSMBusProfile, uint32_t ulValue );

/* Descriptor read functions */

/**
 * @brief    Writes a data byte along with a Target Read - Read Descriptor ID
 *           to transmit the data byte
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucData is the data byte being returned by the target
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusTargetReadDescriptorRead( SMBusProfile* pxSMBusProfile, uint8_t ucData );

/**
 * @brief    Writes a Target Read - PEC Descriptor ID to the Target Descriptor FIFO
 *           To iform the IP to transmit the PEC byte it has calculated on the data
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusTargetReadDescriptorPECRead( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Writes a Target Write - ACK Descriptor ID to the Target Descriptor FIFO
 *           To inform the IP to transmit an ACK
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusTargetWriteDescriptorACK( SMBusProfile* pxSMBusProfile, uint8_t ucNoStatusCheck );

/**
 * @brief    Writes a Target Write - NACK Descriptor ID to the Target Descriptor FIFO
 *           To inform the IP to transmit a NACK
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusTargetWriteDescriptorNACK( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Writes a Target Write - PEC Descriptor ID to the Target Descriptor FIFO
 *           To inform the IP to interpret the previous byte as a PEC
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusTargetWriteDescriptorPEC( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Writes a Controller Write - START Descriptor ID to the Controller Descriptor FIFO
 *           To inform the IP to transmit a START condition
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucDestination is the Target address to transmit to
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusControllerWriteDescriptorStartWrite( SMBusProfile* pxSMBusProfile, uint8_t ucDestination );

/**
 * @brief    Writes a Controller Write - QUICK Descriptor ID to the Controller Descriptor FIFO
 *           To inform the IP to transmit a START condition followed by a STOP
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucDestination is the Target address to transmit to
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusControllerWriteDescriptorQuickWrite( SMBusProfile* pxSMBusProfile, uint8_t ucDestination );

/**
 * @brief    Writes a Controller Write - BYTE Descriptor ID to the Controller Descriptor FIFO
 * along with a data byte
 *           To inform the IP to transmit the data byte
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucData is the data byte to send
 * @param    ucNoStatusCheck allows the function to bypass a read of the fill status of the FIFO
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusControllerWriteDescriptorByte( SMBusProfile* pxSMBusProfile, uint8_t ucData, uint8_t ucNoStatusCheck );

/**
 * @brief    Writes a Controller Write - STOP Descriptor ID to the Controller Descriptor FIFO
 * along with a data byte
 *           To inform the IP to transmit the data byte followed by a STOP condition
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucData is the final data byte to send
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusControllerWriteDescriptorStopWrite( SMBusProfile* pxSMBusProfile, uint8_t ucData );

/**
 * @brief    Writes a Controller Write - PEC Descriptor ID to the Controller Descriptor FIFO
 *           To inform the IP to transmit the PEC verify an ACK and then transmit a STOP
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusControllerWriteDescriptorPECWrite( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Writes a Controller Read - START Descriptor ID to the Controller Descriptor FIFO
 * along with Target address
 *           To inform the IP to start a new READ transaction
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucDestination is the Target address to transmit to with Read bit
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusControllerReadDescriptorStart( SMBusProfile* pxSMBusProfile, uint8_t ucDestination );

/**
 * @brief    Writes a Controller Read - QUICK Descriptor ID to the Controller Descriptor FIFO
 * along with Target address
 *           To inform the IP to start a new READ transaction followed by a STOP
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucDestination is the Target address to transmit to with Read bit
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusControllerReadDescriptorQuickRead( SMBusProfile* pxSMBusProfile, uint8_t ucDestination );

/**
 * @brief    Writes a Controller Read - BYTE Descriptor ID to the Controller Descriptor FIFO
 *           To inform the IP to transmit an ACK for the previous byte received
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucNoStatusCheck allows the function to bypass a read of the fill status of the FIFO
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusControllerReadDescriptorByte( SMBusProfile* pxSMBusProfile, uint8_t ucNoStatusCheck );

/**
 * @brief    Writes a Controller Read - STOP Descriptor ID to the Controller Descriptor FIFO
 *           To inform the IP to transmit a NACK followed by a STOP condition
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusControllerReadDescriptorStop( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Writes a Controller Read - PEC Descriptor ID to the Controller
 *           Descriptor FIFO to inform the IP to transmit a NACK followed by a
 *           STOP and use the last received byte to perform a PEC check
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 *
 * @return   SMBUS_HW_DESCRIPTOR_WRITE_FAIL      - if write to descriptor fails
 *           SMBUS_HW_DESCRIPTOR_WRITE_SUCCESS   - if write to descriptor succeeds
 */
uint8_t ucSMBusControllerReadDescriptorPEC( SMBusProfile* pxSMBusProfile );

/**
 * @brief    Converts an Descriptor enum value to a text string for logging
 *
 * @param    SMBUS_HW_DESCRIPTOR_TYPE is any Descriptor enum value
 *
 * @return   A text string of the event
 */
char* pcDescriptorToString( SMBUS_HW_DESCRIPTOR_TYPE xDescriptor );

#ifdef __cplusplus
}
#endif

#endif /* _SMBus_HW_ACCESS_H_ */
