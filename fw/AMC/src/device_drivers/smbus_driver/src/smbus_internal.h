/**
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This header file contains private definitions for the SMBus driver.
 *
 * @file smbus_internal.h
 */

#ifndef _SMBUS_INTERNAL_H_
#define _SMBUS_INTERNAL_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include "smbus.h"
#include "i2c.h"

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/
#define SMBUS_INVALID_ADDRESS_MASK              ( 0x80 )
#define SMBUS_DEVICE_DEFAULT_ARP_ADDRESS        ( 0x61 )
#define SMBUS_NOTIFY_ARP_MASTER_ADDRESS         ( 0x10 )
#define SMBUS_ARP_INSTANCE_ID                   ( 7 )
#define SMBUS_FIRST_SMBUS_INSTANCE              ( 0 )
#define SMBUS_LAST_SMBUS_INSTANCE               ( 7 )
#define SMBUS_LAST_NON_ARP_SMBUS_INSTANCE       ( SMBUS_LAST_SMBUS_INSTANCE - 1 )
#define SMBUS_INSTANCE_UNDETERMINED             ( 88 )
#define SMBUS_UDID_ASSIGNED_ADDRESS_BYTE        ( 17 )
#define SMBUS_UDID_ASSIGNED_ADDRESS_BIT0        ( 0x01 )
#define SMBUS_GET_UDID_DATA_LENGTH              ( 17 )
#define SMBUS_GET_UDID_MSG_LENGTH               ( SMBUS_GET_UDID_DATA_LENGTH + 1 )
#define SMBUS_UDID_DEVICE_CAPABILITIES_BYTE     ( 15 )
#define SMBUS_UDID_DTA_AV_FLAG_CLEAR            ( 0xFF )
#define SMBUS_UDID_DTA_BIT_0_SET                ( 0x01 )
#define SMBUS_UDID_ADDRESS_TYPE_MASK            ( 0xC0 )
#define SMBUS_UDID_FIXED_ADDRESS                ( 0x00 )
#define SMBUS_UDID_DYNAMIC_AND_PERSISTENT       ( 0x40 )
#define SMBUS_UDID_DYNAMIC_AND_VOLATILE         ( 0x80 )
#define SMBUS_UDID_RANDOM_NUMBER                ( 0xC0 )
#define SMBUS_MAGIC_NUMBER                      ( 0x534D4273 )
#define SMBUS_FIREWALL1                         ( 0xF15E2112 )
#define SMBUS_FIREWALL2                         ( 0xAC1DBA78 )
#define SMBUS_FIREWALL3                         ( 0x1CE1CE99 )
#define SMBUS_UDID_PEC_SUPPORTED_BIT            ( 0x01 )
#define SMBUS_RX_FIFO_IS_EMPTY                  ( 1 )
#define SMBUS_DESC_FIFO_IS_FULL                 ( 1 )
#define SMBUS_COMMAND_INVALID                   ( 66 )
#define SMBUS_FIFO_DEPTH                        ( 64 )
#define SMBUS_HALF_FIFO_DEPTH                   ( 32 )
#define SMBUS_FIFO_SPACE_FOR_TWO_BYTES          ( SMBUS_FIFO_DEPTH - 2 )
#define SMBUS_FIFO_FILL_TRIGGER                 ( 20 )
#define SMBUS_INIT_CODE                         ( 0x7AB3F91D )
#define SMBUS_DEINIT_CODE                       ( 0x0 )
#define SMBUS_TBUF_DIVISOR                      ( 100000000 )
#define SMBUS_CTRL_FIXED_INPUT_LATENCY          ( 7    )
#define SMBUS_TBUF_MIN_100KHZ                   ( 470  )
#define SMBUS_TBUF_MIN_400KHZ                   ( 130  )
#define SMBUS_TBUF_MIN_1MHZ                     ( 50   )
#define SMBUS_TSU_DAT_MIN_100KHZ                ( 125  )
#define SMBUS_TSU_DAT_MIN_400KHZ                ( 40   )
#define SMBUS_TSU_DAT_MIN_1MHZ                  ( 17   )
#define SMBUS_TGT_DATA_HOLD_100KHZ              ( 33   )
#define SMBUS_TGT_DATA_HOLD_400KHZ              ( 0.11 )
#define SMBUS_TGT_DATA_HOLD_1MHZ                ( 0.11 )
#define SMBUS_CTLR_DATA_HOLD_100KHZ             ( 33   )
#define SMBUS_CTLR_DATA_HOLD_400KHZ             ( 0.11 )
#define SMBUS_CTLR_DATA_HOLD_1MHZ               ( 0.11 )
#define SMBUS_CTLR_START_HOLD_100KHZ            ( 440  )
#define SMBUS_CTLR_START_HOLD_400KHZ            ( 66   )
#define SMBUS_CTLR_START_HOLD_1MHZ              ( 28.6 )
#define SMBUS_CTLR_START_SETUP_100KHZ           ( 517  )
#define SMBUS_CTLR_START_SETUP_400KHZ           ( 66   )
#define SMBUS_CTLR_START_SETUP_1MHZ             ( 28.6 )
#define SMBUS_CTLR_STOP_SETUP_100KHZ            ( 440  )
#define SMBUS_CTLR_STOP_SETUP_400KHZ            ( 66   )
#define SMBUS_CTLR_STOP_SETUP_1MHZ              ( 28.6 )
#define SMBUS_CTLR_CLK_LOW_100KHZ               ( 517  )
#define SMBUS_CTLR_CLK_LOW_400KHZ               ( 143  )
#define SMBUS_CTLR_CLK_LOW_1MHZ                 ( 55   )
#define SMBUS_CTLR_CLK_HIGH_100KHZ              ( 440  )
#define SMBUS_CTLR_CLK_HIGH_400KHZ              ( 66   )
#define SMBUS_CTLR_CLK_HIGH_1MHZ                ( 28.6 )
#define SMBUS_GET_FREQ_VALUE( x, y )            ( ulSMBusCeil( ( float )x * ( ( ( float )y )/ ( float )SMBUS_TBUF_DIVISOR ) ) - 1 )
#define SMBUS_GET_FREQ_VALUE_MINUS_CONSTANT( x, y, z )   ( ulSMBusCeil( ( float )x * ( ( ( float )y )/ ( float )SMBUS_TBUF_DIVISOR ) ) - z )
#define SMBUS_GET_GLITCH_FILTER_DUR( x )        ( x + 1 )
#define SMBUS_GET_CONSTANT                      ( SMBUS_CTRL_FIXED_INPUT_LATENCY + 1 )
#define SMBUS_GET_CONSTANT_WITH_GLITCH( x )     ( x + SMBUS_GET_CONSTANT )
#define SMBUS_SINGLE_ELEMENT                    ( 1 )
#define SMBUS_ZERO_ELEMENTS                     ( 0 )
#define SMBUS_TRUE                              ( 1 )
#define SMBUS_FALSE                             ( 0 )
#define SMBUS_CONTROL_ENABLE                    ( 1 )
#define SMBUS_MAX_FIFO_EMPTY_WHILE_IN_DONE      ( 10 )
#define SMBUS_SMBCLK_LOW_TIMEOUT_DETECTED       ( 1 )
#define SMBUS_SMBDAT_LOW_TIMEOUT_DETECTED       ( 1 )
#define SMBUS_UNEXPECTED_READ_DESCRIPTOR_DATA   ( 0xFF )

/*
 * @enum SMBUS_STATE
 * @brief Enumeration of SMBUS FSM states
 */
typedef enum
{
    SMBUS_STATE_INITIAL                    = 0,
    SMBUS_STATE_AWAITING_COMMAND_BYTE      = 1,
    SMBUS_STATE_AWAITING_BLOCK_SIZE        = 2,
    SMBUS_STATE_AWAITING_DATA              = 3,
    SMBUS_STATE_AWAITING_READ              = 4,
    SMBUS_STATE_READY_TO_SEND_BYTE         = 5,
    SMBUS_STATE_CHECK_IF_PEC_REQUIRED      = 6,
    SMBUS_STATE_AWAITING_DONE              = 7,
    SMBUS_STATE_CONTROLLER_SEND_COMMAND    = 8,
    SMBUS_STATE_CONTROLLER_SEND_READ_START = 9,
    SMBUS_STATE_CONTROLLER_READ_BLOCK_SIZE = 10,
    SMBUS_STATE_CONTROLLER_READ_BYTE       = 11,
    SMBUS_STATE_CONTROLLER_READ_PEC        = 12,
    SMBUS_STATE_CONTROLLER_READ_DONE       = 13,
    SMBUS_STATE_CONTROLLER_WRITE_BYTE      = 14,

} SMBUS_STATE;

/*
 * @struct SMBus_LogBufRecord
 * @brief  Structure to hold SMBus debug logging information
 */
typedef struct
{
    uint32_t        ulIsOccupied;
    uint32_t        ulTicks;
    uint32_t        ulEntry1;
    uint32_t        ulInstance;
    uint32_t        ulEntry2;
    SMBUS_LOG_EVENT xEvent;

} SMBus_LogBufRecord;

/*
 * @struct SMBus_LogBuf
 * @brief  Structure to hold SMBus read/write logging information
 */
typedef struct
{
    uint32_t    ulWrite;
    uint32_t    ulRead;

} SMBus_LogBuf;

/*
 * @struct SMBus_EventBufElement
 * @brief  Structure to single SMBus State Machine Event
 */
typedef struct
{
    uint8_t     ucIsOccupied;
    uint8_t     ucOctet;

} SMBus_EventBufElement;

/*
 * @struct SMBus_EventBuf
 * @brief  Structure to hold SMBus State Machine Event queue
 */
typedef struct
{
    uint32_t    ulMaxElements;
    uint32_t    ulWrite;
    uint32_t    ulRead;
    SMBus_EventBufElement* pxEventBuf;

} SMBus_EventBuf;

/*
 * @struct SMBus_Instance
 * @brief  Structure to hold a single SMBus instance
 */
typedef struct
{
    uint32_t                         ulFirewall1;
    SMBUS_USER_ENV_GET_PROTOCOL_TYPE pFnGetProtocol;
    SMBUS_USER_ENV_GET_DATA_TYPE     pFnGetData;
    SMBUS_USER_ENV_WRITE_DATA_TYPE   pFnWriteData;
    SMBUS_USER_ENV_CMD_COMPLETE      pFnAnnounceResult;
    SMBUS_USER_ENV_ARP_ADDR_CHANGE   pFnArpAddrChange;
    SMBUS_USER_ENV_BUS_ERROR         pFnBusError;
    SMBUS_USER_ENV_BUS_WARNING       pFnBusWarning;
    I2C_USER_ENV_GET_DATA_TYPE       pFnI2CGetData;
    I2C_USER_ENV_WRITE_DATA_TYPE     pFnI2CWriteData;
    I2C_USER_ENV_COMMAND_COMPLETE    pFnI2CAnnounceResult;
    void*                            pxSMBusProfile;
    uint32_t                         ulI2CDevice;
    uint32_t                         ulI2CTransaction;
    uint32_t                         ulAction;
    uint32_t                         ulMessagesComplete[I2C_PROTOCOL_NONE];
    uint32_t                         ulMessagesInitiated[I2C_PROTOCOL_NONE];
    uint16_t                         usSendDataSize;
    uint16_t                         usSendIndex;
    uint16_t                         usReceiveIndex;
    uint16_t                         usDescriptorsSent;
    uint8_t                          ucSendData[SMBUS_DATA_SIZE_MAX];
    uint8_t                          ucReceivedData[SMBUS_DATA_SIZE_MAX];
    uint8_t                          ucUDID[SMBUS_UDID_LENGTH];
    uint32_t                         ulFirewall2;
    uint8_t                          ucInstanceInUse;
    uint8_t                          ucSMBusAddr;
    uint8_t                          ucPECRequired;
    uint8_t                          ucThisInstanceNum;
    uint8_t                          ucEvent;
    uint8_t                          ucCommand;
    uint8_t                          ucSimpleDevice;
    uint8_t                          ucExpectedByteCountPart;
    uint16_t                         usExpectedByteCount;
    uint8_t                          ucSMBusDestAddr;
    uint8_t                          ucSMBusSenderAddr;
    uint8_t                          ucPecRequiredForTransaction;
    uint8_t                          ucTriggerFSM;
    uint8_t                          ucNewDeviceSlaveAddr;
    uint8_t                          ucUDIDMatchedInstance;
    uint8_t                          ucARFlag;
    uint8_t                          ucAVFlag;
    uint8_t                          ucMatchedSMBusAddr;
    uint8_t                          ucNackSent;
    uint8_t                          ucPECSent;
    uint8_t                          ucFifoEmptyWhileInDoneCount;
    SMBUS_STATE                      xPreviousState;
    SMBUS_STATE                      xState;
    SMBUS_COMMAND_PROTOCOL           xProtocol;
    SMBUS_ARP_CAPABILITY             xARPCapability;
    SMBus_EventBuf                   xEventSrcCircularBuf;
    SMBus_EventBufElement            xCircularBuf[SMBUS_MAX_EVENT_ELEMENTS];
    uint32_t                         ulFirewall3;

} SMBus_Instance;

/*
 * @struct SMBus_Profile
 * @brief  Structure to hold a single SMBus profile
 */
typedef struct SMBus_Profile
{
    SMBUS_USER_ENV_READ_TICKS  pFnReadTicks;
    void*                      pvBaseAddr;
    uint32_t                   ulTransactionID;
    uint32_t                   ulInitialize;
    SMBus_LogBuf               xLogCircularBuf;
    SMBus_LogBufRecord         xCircularBuf[SMBUS_MAX_CIRCULAR_LOG_ENTRIES];
    SMBus_Instance             xSMBusInstance[SMBUS_NUM_INSTANCES];
    uint8_t                    ucInstanceInPlay;
    uint8_t                    ucActiveTargetInstance;
    SMBUS_LOG_LEVEL            xLogLevel;
    uint8_t                    ucUDIDMatch[SMBUS_NUM_NON_ARP_INSTANCES];

} SMBus_Profile;

/**
 * @brief    Does a ceiling conversion on a floating point number and returns the
 *           rounded up interger value
 *
 * @param    fNum is a floating point number
 * @return   An integer
 */
uint32_t ulSMBusCeil( float fNum );

/**
 * @brief    Converts a protocol enum value to a text string for logging
 *
 * @param    ucProtocol is any protocol enum value
 * @return   A character string of the protocol
 */
SMBUS_ERROR_TYPE xSMBusFirewallCheck(SMBus_Profile* pxSMBusProfile);

/**
 * @brief    Will add a log entry into the debug log
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    xLogLevel is the level of the log being added
 * @param    ulInstance is the numerical instance number
 * @param    xLogEvent is the type of log event
 * @param    ulEntry1 is a uint32_t value to log. Depending on xLogEvent this can be interpreted
 * @param    ulEntry2 is a 2nd uint32_t value to log. Depending on xLogEvent this can be interpreted
 *
 * @return   None
 */
void vLogAddEntry( SMBus_Profile* pxSMBusProfile, SMBUS_LOG_LEVEL xLogLevel, uint32_t ulInstance,
                   SMBUS_LOG_EVENT xLogEvent, uint32_t ulEntry1, uint32_t ulEntry2 );

/**
 * @brief    Converts a protocol enum value to a text string for logging
 *
 * @param    ucProtocol is any protocol enum value
 *
 * @return   A character string of the protocol
 */
char* pcProtocolToString( uint8_t ucProtocol );

/**
 * @brief    Will walk through all active instances, check if any events have been
 *           raised against that instance and call into the state machine for that
 *           instance with each event found
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @return   None
 */
void vSMBusEventQueueHandle( SMBus_Profile* pxSMBusProfile );
/**
 * @brief    Initializes the debug log. Setting its pointer to zero
 *
 * @param    pxSMBusProfile is the pointer to the SMBUS profile
 *
 * @return   None
 */
void vLogInitialize( SMBus_Profile* pxSMBusProfile );

/**
 * @brief    Will retreive the log as a character string
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    pcLogBuffer is the buffer to write the log in to.
 * @param    usLogSizeBytes is the number of byte being returned
 *
 * @return   None
 */
void vLogDisplayLog( SMBus_Profile* pxSMBusProfile, char* pcLogBuffer, uint32_t* usLogSizeBytes );

/******************************************************************************/
/* Driver Internal APIs                                                       */
/******************************************************************************/

/**
 * @brief    Enables logging
 *
 * @param    pxSMBusProfile is the pointer to the SMBUS profile
 *
 * @return   None
 */
void vSMBusLogEnable( SMBus_Profile* pxSMBusProfile );

/**
 * @brief    Disables logging
 *
 * @param    pxSMBusProfile is the pointer to the SMBUS profile
 *
 * @return   None
 */
void vSMBusLogDisable( SMBus_Profile* pxSMBusProfile );

/**
 * @brief    Resets the statistics log values for the specified instance
 *
 * @param    pxSMBusProfile is the pointer to the SMBUS profile
 * @param    ucSMBusInstanceID is the SMBus instance ID of the instance to use as the controller
 *
 * @return   None
 */
void vSMBusResetStatsLogInstance( SMBus_Profile* pxSMBusProfile, uint8_t ucSMBusInstance );

/**
 * @brief    Reads the statistics log values for the specified instance
 *
 * @param    pxSMBusProfile is the pointer to the SMBUS profile
 * @param    ucSMBusInstanceID is the SMBus instance ID of the instance to use as the controller
 * @param    pSMBusMessageLog is pointer to the log read
 *
 * @return   None
 */
void vSMBusReadStatsLogInstance( SMBus_Profile* pxSMBusProfile, uint8_t ucSMBusInstance,
    SMBus_Log* pSMBusMessageLog );

#ifdef __cplusplus
}
#endif

#endif /* _SMBUS_INTERNAL_H_ */
