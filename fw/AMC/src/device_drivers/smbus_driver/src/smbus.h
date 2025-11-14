/**
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This header file contains structures, type definitions and function declarations
 * for the SMBus driver.
 *
 * @file smbus.h
 */

#ifndef _SMBUS_H_
#define _SMBUS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/
#define SMBUS_DATA_SIZE_MIN             ( 0 )
#define SMBUS_DATA_SIZE_MAX             ( 256 ) /* 255 bytes of data + 1 byte block size */
#define SMBUS_UDID_LENGTH               ( 16 )
#define SMBUS_MAX_CIRCULAR_LOG_ENTRIES  ( 5000 )
#define SMBUS_NUM_INSTANCES             ( 8 )
#define SMBUS_NUM_NON_ARP_INSTANCES     ( 7 )
#define SMBUS_INVALID_INSTANCE          ( 99 )
#define SMBUS_MAX_EVENT_ELEMENTS        ( 300 )

/******************************************************************************/
/* Enums                                                                      */
/******************************************************************************/

/*
 * @enum SMBUS_FREQ_CLASS
 * @brief Enumeration of SMBUS frequency values
 */
typedef enum
{
    SMBUS_FREQ_100KHZ = 0,
    SMBUS_FREQ_400KHZ = 1,
    SMBUS_FREQ_1MHZ   = 2,
    SMBUS_FREQ_MAX    = 3,

} SMBUS_FREQ_CLASS;

/*
 * @enum SMBUS_ERROR_TYPE
 * @brief Enumeration of SMBUS return values
 */
typedef enum
{
    SMBUS_SUCCESS = 0,
    SMBUS_ERROR   = 1,

} SMBUS_ERROR_TYPE;

/*
 * @enum SMBUS_ARP_CAPABILITY
 * @brief Enumeration of SMBUS ARP settings
 */
typedef enum
{
    SMBUS_ARP_CAPABILITY_UNKNOWN     = 0,
    SMBUS_ARP_CAPABLE                = 1,
    SMBUS_ARP_FIXED_AND_DISCOVERABLE = 2,
    SMBUS_ARP_FIXED_NOT_DISCOVERABLE = 3,
    SMBUS_ARP_NON_ARP_CAPABLE        = 4,

} SMBUS_ARP_CAPABILITY;


/*
 * @enum SMBUS_COMMAND_PROTOCOL
 * @brief Enumeration of SMBUS command protocols
 */
typedef enum
{
    SMBUS_PROTOCOL_QUICK_CMD_LO       = 0,
    SMBUS_PROTOCOL_QUICK_CMD_HI       = 1,
    SMBUS_PROTOCOL_SEND_BYTE          = 2,
    SMBUS_PROTOCOL_RECEIVE_BYTE       = 3,
    SMBUS_PROTOCOL_WRITE_BYTE         = 4,
    SMBUS_PROTOCOL_WRITE_WORD         = 5,
    SMBUS_PROTOCOL_READ_BYTE          = 6,
    SMBUS_PROTOCOL_READ_WORD          = 7,
    SMBUS_PROTOCOL_PROCESS_CALL       = 8,
    SMBUS_PROTOCOL_BLOCK_WRITE        = 9,
    SMBUS_PROTOCOL_BLOCK_READ         = 10,
    SMBUS_PROTOCOL_BLK_WRITE_BLK_RD_PROCESS_CALL = 11,
    SMBUS_PROTOCOL_HOST_NOTIFY        = 12,
    SMBUS_PROTOCOL_WRITE_32           = 13,
    SMBUS_PROTOCOL_READ_32            = 14,
    SMBUS_PROTOCOL_WRITE_64           = 15,
    SMBUS_PROTOCOL_READ_64            = 16,
    SMBUS_ARP_PROTOCOL_PREPARE_TO_ARP = 17,
    SMBUS_ARP_PROTOCOL_RESET_DEVICE   = 18,
    SMBUS_ARP_PROTOCOL_GET_UDID       = 19,
    SMBUS_ARP_PROTOCOL_ASSIGN_ADDRESS = 20,
    SMBUS_ARP_PROTOCOL_GET_UDID_DIRECTED = 21,
    SMBUS_ARP_PROTO_RESET_DEVICE_DIRECTED = 22,
    SMBUS_PROTOCOL_NONE               = 23,
    I2C_PROTOCOL_WRITE                = 24,
    I2C_PROTOCOL_READ                 = 25,
    I2C_PROTOCOL_WRITE_READ           = 26,
    I2C_PROTOCOL_NONE                 = 27,

} SMBUS_COMMAND_PROTOCOL;

/*
 * @enum SMBUS_LOG_EVENT
 * @brief Enumeration of SMBus logging types
 */
typedef enum
{
    SMBUS_LOG_EVENT_INTERRUPT_EVENT = 1,
    SMBUS_LOG_EVENT_FSM_EVENT       = 2,
    SMBUS_LOG_EVENT_ERROR           = 3,
    SMBUS_LOG_EVENT_HW_READ         = 4,
    SMBUS_LOG_EVENT_HW_WRITE        = 5,
    SMBUS_LOG_EVENT_PROTOCOL        = 6,
    SMBUS_LOG_EVENT_DEBUG           = 7,
    SMBUS_LOG_EVENT_TRYREAD         = 8,
    SMBUS_LOG_EVENT_TRYWRITE        = 9,

} SMBUS_LOG_EVENT;

/*
 * @enum SMBUS_LOG_LEVEL
 * @brief Enumeration of SMBUS logging levels
 */
typedef enum
{
    SMBUS_LOG_LEVEL_NONE    = 0,
    SMBUS_LOG_LEVEL_ERROR   = 1,
    SMBUS_LOG_LEVEL_WARNING = 2,
    SMBUS_LOG_LEVEL_INFO    = 3,
    SMBUS_LOG_LEVEL_DEBUG   = 4,
    SMBUS_LOG_LEVEL_MAX     = 5,

} SMBUS_LOG_LEVEL;

/******************************************************************************/
/* Typedefs                                                                   */
/******************************************************************************/

/*
 * @typedef SMBUS_USER_ENV_GET_PROTOCOL_TYPE
 *
 * @brief   This callback retrieves the protocol for a command
 * @param   pxUserContext is the pointer to SMBusProfile
 * @param   ucCommand is the command under inspection
 * @param   xProtocol is a pointer for the protocol to be stored in
 *
 * @return  void
 */
typedef void ( *SMBUS_USER_ENV_GET_PROTOCOL_TYPE )( uint8_t ucCommand,
                SMBUS_COMMAND_PROTOCOL* xProtocol );

/*
 * @typedef SMBUS_USER_ENV_GET_DATA_TYPE
 *
 * @brief   This callback updates the initialiser with new data
 * @param   pxUserContext is the pointer to SMBusProfile
 * @param   ucCommand is the SMBus command
 * @param   pucData is a pointer to the new data read
 * @param   Data_Size is the number of bytes in pucData
 *
 * @return  void
 */
typedef void ( *SMBUS_USER_ENV_GET_DATA_TYPE )( uint8_t ucCommand, uint8_t* pucData,
                uint16_t* Data_Size );

/*
 * @typedef SMBUS_USER_ENV_WRITE_DATA_TYPE
 *
 * @brief   This callback retrieves data from the initialiser to write
 * @param   pxUserContext is the pointer to SMBusProfile
 * @param   ucCommand is the SMBus command
 * @param   pucData is a pointer to the new data to write
 * @param   Data_Size is the number of bytes in pucData
 * @param   ulTransactonID is the transaction ID of the message
 *
 * @return  void
 */
typedef void ( *SMBUS_USER_ENV_WRITE_DATA_TYPE )( uint8_t ucCommand, uint8_t* pucData,
                uint16_t Data_Size, uint32_t ulTransactionID );

/*
 * @typedef SMBUS_USER_ENV_CMD_COMPLETE
 *
 * @brief   This callback updates the initialiser when a command is complete
 * @param   pxUserContext is the pointer to SMBusProfile
 * @param   ucCommand is the SMBus command
 * @param   ulTransactionID is the transaction ID of the completed command
 * @param   Status is the status of the command
 *
 * @return  void
 */
typedef void ( *SMBUS_USER_ENV_CMD_COMPLETE )( uint8_t ucCommand, uint32_t ulTransactionID,
                uint32_t Status );

/*
 * @typedef SMBUS_USER_ENV_ARP_ADDR_CHANGE
 *
 * @brief   This callback updates the initialiser when an address is changed
 * @param   pxUserContext is the pointer to SMBusProfile
 * @param   ucNewAddr is the next SMBus address
 *
 * @return  void
 */
typedef void ( *SMBUS_USER_ENV_ARP_ADDR_CHANGE )( uint8_t ucNewAddr );

/*
 * @typedef SMBUS_USER_ENV_BUS_ERROR
 *
 * @brief   This callback updates the initialiser when there is an SMBus Error
 * @param   pxUserContext is the pointer to SMBusProfile
 * @param   ucError is the error that was raised
 *
 * @return  void
 */
typedef void ( *SMBUS_USER_ENV_BUS_ERROR )( uint8_t ucError );

/*
 * @typedef SMBUS_USER_ENV_BUS_WARNING
 *
 * @brief   This callback updates the initialiser when there is an SMBus Warning
 * @param   pxUserContext is the pointer to SMBusProfile
 * @param   ucWarning is the warning that was raised
 *
 * @return  void
 */
typedef void ( *SMBUS_USER_ENV_BUS_WARNING )( uint8_t ucWarning );

/*
 * @typedef SMBUS_USER_ENV_READ_TICKS
 *
 * @brief   This callback retrieves the current tick count from the initialiser
 * @param   pxUserContext is the pointer to SMBusProfile
 * @param   pulTicks is a a pointer to store the tick count
 *
 * @return  void
 */
typedef void ( *SMBUS_USER_ENV_READ_TICKS )( uint32_t* pulTicks );

/******************************************************************************/
/* Structs                                                                    */
/******************************************************************************/


/*
 * @struct SMBus_Log
 * @brief  Structure to hold SMBus message logging information
 */
typedef struct
{
    uint32_t   ulMessagesComplete[SMBUS_PROTOCOL_NONE];
    uint32_t   ulMessagesInitiated[SMBUS_PROTOCOL_NONE];

} SMBus_Log;


/*
 * @struct SMBus_Version
 * @brief  Structure to hold the SMBus driver version informatin
 */
typedef struct
{
    uint16_t    usIpVerMajor;
    uint16_t    usIpVerMinor;
    uint8_t     ucSwVerMajor;
    uint8_t     ucSwVerMinor;
    uint8_t     ucSwVerPatch;
    uint8_t     ucSwDevBuild;
    uint8_t     ucSwTestBuild;

} SMBus_Version;


/*
 * @struct SMBus_Profile
 * @brief  Forward Declaration of structure to hold a single SMBus profile
 */
typedef struct SMBus_Profile SMBus_Profile;

/******************************************************************************/
/* Driver External APIs                                                       */
/******************************************************************************/

/**
 * @brief    Checks hardware is present at the supplied base address
 *           Sets up hardware registers for the frequency class supplied
 *           initializes software structures, sets up log and event queues
 *
 * @param    ppxSMBusProfile is a pointer to the SMBus profile structure handle.
 * @param    xFrequencyClass is an enum of type Freq_Class_Type which can be
 *           100KHz, 400KHz or 1MHz.
 * @param    pvBaseAddr is the address of the SMBus IP block
 * @param    xLogLevel is the level of logging detail required
 * @param    pFnReadTicks is an optional function pointer that supplies a tick count for logging purposes
 *
 * @return   - SMBUS_ERROR if error
 *           - SMBUS_SUCCESS if successful
 */
SMBUS_ERROR_TYPE xInitSMBus( SMBus_Profile** ppxSMBusProfile,
                             SMBUS_FREQ_CLASS xFrequencyClass,
                             void * pvBaseAddr,
                             SMBUS_LOG_LEVEL xLogLevel,
                             SMBUS_USER_ENV_READ_TICKS pFnReadTicks );


/**
 * @brief    Checks all instances have already been removed
 *           If so sets Profile structure to default values
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure handle.
 *
 * @return   - SMBUS_ERROR if error
 *           - SMBUS_SUCCESS if successful
 */
SMBUS_ERROR_TYPE xDeinitSMBus( SMBus_Profile** ppxSMBusProfile );


/**
 * @brief    Checks that a free instance slot is available and if so stores the
 *           supplied data associated with the instance and enables the hardware
 *           to send or receive SMBus messages for the supplied instance
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucSMBusAddr is the 7-bit SMBUs address to asscociate with this instance
 * @param    ucUDID is the 16 byte UDID to asscociate with this instance
 * @param    xARPCapability is the ARP capabaility of the instance
 * @param    pFnGetProtocol ia a pointer to a function to convert coammand byte to an SMBus protocol
 * @param    pFnGetData ia a pointer to a function to get data to be returned
 * @param    pFnWriteData ia a pointer to a function to write data received
 * @param    pFnAnnounceResult ia a pointer to a function to announce the result of a transaction
 * @param    pFnArpAddrChange ia a pointer to a function to call if the address is changed
 * @param    pFnBusError ia a pointer to a function to report SMBus errors
 * @param    pFnBusWarning ia a pointer to a function to report SMBus warnings
 * @param    ucSimpleDevice is a flag to allow instance to understand only simple
 *                          send and receive byte commands
 *
 * @return   - SMBUS_INVALID_INSTANCE ID ( 99 ) if error
 *           - Instance ID ( 0 - 6 ) if successful
 */
uint8_t ucCreateSMBusInstance( SMBus_Profile* pxSMBusProfile,
                               uint8_t ucSMBusAddr,
                               uint8_t ucUDID[SMBUS_UDID_LENGTH],
                               SMBUS_ARP_CAPABILITY xARPCapability,
                               SMBUS_USER_ENV_GET_PROTOCOL_TYPE pFnGetProtocol,
                               SMBUS_USER_ENV_GET_DATA_TYPE pFnGetData,
                               SMBUS_USER_ENV_WRITE_DATA_TYPE pFnWriteData,
                               SMBUS_USER_ENV_CMD_COMPLETE pFnAnnounceResult,
                               SMBUS_USER_ENV_ARP_ADDR_CHANGE pFnArpAddrChange,
                               SMBUS_USER_ENV_BUS_ERROR pFnBusError,
                               SMBUS_USER_ENV_BUS_WARNING pFnBusWarning,
                               uint8_t  ucSimpleDevice );


/**
 * @brief    Checks that the supplied instance is present and attempts to remove
 *           it. If the instance being removed is the only instance then the ARP
 *           instance is also removed
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucSMBusInstanceID is the SMBus instance ID of the instance to be
 *           removed
 *
 * @return   - SMBUS_ERROR if error
 *           - SMBUS_SUCCESS if successful
 */
SMBUS_ERROR_TYPE xDestroySMBusInstance( SMBus_Profile* pxSMBusProfile,
                                         uint8_t ucSMBusInstanceID );


/**
 * @brief    Will initiate an SMBus message from the supplied intance as a controller
 *
 * @param    pxSMBusProfile is a pointer to the SMBus profile structure.
 * @param    ucSMBusInstanceID is the SMBus instance ID of the instance to use as the controller
 * @param    ucSMBusDestAddr is the address of the target SMBUs device
 * @param    ucCommand is the Command byte of the message
 * @param    xProtocol is the SMBUs Protocol type of the message being sent
 * @param    usDataSize is the Size of data being sent ( if tranaction protocol is a send type )
 * @param    pucData is a pointer to the data to send ( if tranaction protocol is a send type )
 * @param    ucPecRequiredForTransaction is an integer to set if a PEC is required to be sent or not
 * @param    ulTransactionID is a pointer to the unique ID assigned to this transaction
 *
 * @return   - SMBUS_ERROR if error
 *           - SMBUS_SUCCESS if successful
 */
SMBUS_ERROR_TYPE xSMBusControllerInitiateCommand( SMBus_Profile* pxSMBusProfile,
                                                   uint8_t ucSMBusInstance,
                                                   uint8_t ucSMBusDestAddr,
                                                   uint8_t ucCommand,
                                                   SMBUS_COMMAND_PROTOCOL xProtocol,
                                                   uint16_t usDataSize,
                                                   uint8_t* pucData,
                                                   uint8_t ucPecRequiredForTransaction,
                                                   uint32_t* pulTransactionID );


/**
 * @brief    Retrieves SMBus log that is stored as a circular buffer in profile struct
 *           as ASCII char array
 *
 * @param    SMBus_Profile is the context to poll log on
 * @param    pcLogBuffer is the array to put log data must be more than TBD driver events string
 *           separated by '\n'
 * @param    pusLogSizeBytes is a pointer to the number of bytes that are in the log
 *           (NOTE NOT NULL TERMINATED. THIS SHOULD BE USED FOR MEMCPY ETC)
 *
 * @return   - SMBUS_ERROR if error
 *           - SMBUS_SUCCESS if successful
 */
SMBUS_ERROR_TYPE xSMBusGetLog( SMBus_Profile* pxSMBusProfile,
                               char* pcLogBuffer,
                               uint32_t* pulLogSizeBytes );


/**
 * @brief    Resets SMBus Driver Log
 *
 * @param    SMBus_Profile is the context with log to clear
 *
 * @return   - SMBUS_ERROR if error
 *           - SMBUS_SUCCESS if successful
 */
SMBUS_ERROR_TYPE xSMBusLogReset( SMBus_Profile* pxSMBusProfile );


/**
 * @brief    Disables and then clears all SMBUs interrupts
 *
 * @param    pxSMBusProfile is the pointer to the SMBus profile.
 *
 * @return   - SMBUS_ERROR if error
 *           - SMBUS_SUCCESS if successful
 */
SMBUS_ERROR_TYPE xSMBusInterruptDisableAndClearInterrupts( SMBus_Profile* pxSMBusProfile );


/**
 * @brief    enables all necessary SMBUs interrupts
 *
 * @param    pxSMBusProfile is the pointer to the SMBus profile.
 *
 * @return   - SMBUS_ERROR if error
 *           - SMBUS_SUCCESS if successful
 */
SMBUS_ERROR_TYPE xSMBusInterruptEnableInterrupts( SMBus_Profile* pxSMBusProfile );


/**
 * @brief    Function will be a callback called from the interrupt handler
 *           It will determine what interrupts are present from those add
 *           events on the event queue and then trigger the handling of the
 *           events by the state machine
 *
 * @param    pvCallBackRef is the reference to the SMBus profile.
 *
 * @return   void
 */
void vSMBusInterruptHandler( void* pvCallBackRef );


/**
 * @brief    Retrieves the SMBus driver version
 *
 * @param    pxSMBusProfile is the pointer to the SMBUS profile
 *
 * @param    pxSMBusVersion is the pointer to the SMBus version (to be filled).
 *
 * @return   - SMBUS_ERROR if error
 *           - SMBUS_SUCCESS if successful
 */
SMBUS_ERROR_TYPE xSMBusGetVersion( SMBus_Profile* pxSMBusProfile,
    SMBus_Version* pxSMBusVersion );

#ifdef __cplusplus
}
#endif

#endif /* _SMBUS_H_ */
