/**
 * Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This header file contains structures, type definitions and function declarations
 * for the sGCQ driver.
 *
 * @file gcq.h
 */

#ifndef _GCQ_H_
#define _GCQ_H_

#include <stdint.h>
#include <stdio.h>
#include <assert.h>


/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/

#ifndef GCQ_MAX_INSTANCES
#define GCQ_MAX_INSTANCES	( 4 )   /**< Default value, but can be overridden by build environmental variable  */
#endif

#define likely( x )     __builtin_expect( !!( x ), 1 )
#define unlikely( x )   __builtin_expect( !!( x ), 0 )


/******************************************************************************/
/* Typedefs                                                                   */
/******************************************************************************/

/**
 * @brief   Bound in function ptr for reading from a memory address
 *
 * @param   ullMemAddr is the memory address to be read
 *
 * @return  The 32-bit value read from memory
 */
typedef uint32_t ( *GCQ_READ_MEM_32 )( uint64_t ullMemAddr );

/**
 * @brief   Bound in function for writing to a memory address
 *
 * @param   ullMemAddr is the memory address to be written
 * @param   ulValue is the 32-bit value to write
 *
 * @return  N/A
 */
typedef void ( *GCQ_WRITE_MEM_32 )( uint64_t ullMemAddr, uint32_t ulValue );


/******************************************************************************/
/* Structs                                                                    */
/******************************************************************************/

/*
 * @struct GCQInstance
 * @brief  Forward declaration of structure to hold a sGCQ instance
 */
typedef struct GCQInstance GCQInstance;

/*
 * @struct GCQIOAccess
 * @brief  Bound in function pointers for memory & register access
 */
typedef struct
{
    GCQ_READ_MEM_32     xGCQReadMem32;
    GCQ_WRITE_MEM_32    xGCQWriteMem32;

} GCQIOAccess;

typedef struct
{
    uint8_t     ucVerMajor;
    uint8_t     ucVerMinor;
    uint8_t     ucVerPatch;
    uint8_t     ucDevCommits;

} GCQVersion;


/******************************************************************************/
/* Enums                                                                      */
/******************************************************************************/

/*
 * @enum GCQ_ERRORS_TYPE
 * @brief Enumeration of sGCQ driver return values
 */
typedef enum
{
    GCQ_ERRORS_NONE                      =  0,
    GCQ_ERRORS_DRIVER_NOT_INITIALISED    =  1,
    GCQ_ERRORS_NO_FREE_INSTANCES         =  2,
    GCQ_ERRORS_INVALID_INSTANCE          =  3,
    GCQ_ERRORS_INVALID_ARG               =  4,
    GCQ_ERRORS_INVALID_SLOT_SIZE         =  5,
    GCQ_ERRORS_INVALID_VERSION           =  6,
    GCQ_ERRORS_INVALID_NUM_SLOTS         =  7,
    GCQ_ERRORS_CONSUMER_NOT_ATTACHED     =  8,
    GCQ_ERRORS_CONSUMER_NOT_AVAILABLE    =  9,
    GCQ_ERRORS_CONSUMER_NO_DATA_RECEIVED = 10,
    GCQ_ERRORS_PRODUCER_NO_FREE_SLOTS    = 11,

    MAX_GCQ_ERRORS_TYPE

} GCQ_ERRORS_TYPE;

/*
 * @enum GCQ_MODE_TYPE
 * @brief Enumeration of sGCQ supported modes
 */
typedef enum
{
    GCQ_MODE_TYPE_CONSUMER_MODE = 0,
    GCQ_MODE_TYPE_PRODUCER_MODE = 1,

    MAX_GCQ_MODE_TYPE

} GCQ_MODE_TYPE;

/******************************************************************************/
/* Driver External APIs                                                       */
/******************************************************************************/

/**
 * @brief    Initialise the sGCQ standalone driver
 *           Internally the function will:
 *           - Allocate an internal instance if any are free
 *           - Calculate the number of slots required
 *           - Validate there is enough shared memory
 *           - Initialise the internal ring buffers for the producer
 *           - Write the magic metadata to the producer
 *           - Handle the extended driver functionality flags
 *
 * @param    ppxGCQInstance is the instance of the of the sGCQ returned by driver
 * @param    pxIOAccess is the bound in function pointers for memory & register access
 * @param    xMode is the supported mode, consumer or producer
 * @param    ullBaseAddr is the base address of the sGCQ
 * @param    ullRingAddr is the base address of the shared memory for allocating slots
 * @param    ullRingLen is the length of the shared memory provided
 * @param    ulSQSlotSize is the required submission queue (SQ) slot size
 * @param    ulCQSlotSize is the required completion queue (CQ) slot size
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
GCQ_ERRORS_TYPE xGCQInit( GCQInstance **ppxGCQInstance,
                          const GCQIOAccess *pxIOAccess,
                          GCQ_MODE_TYPE xMode,
                          uint64_t ullBaseAddr,
                          uint64_t ullRingAddr,
                          uint64_t ullRingLen,
                          uint32_t ulSQSlotSize,
                          uint32_t ulCQSlotSize );

/**
 * @brief    De-initialise a sGCQ driver instance
 *
 * @param    ppxGCQInstance is the instance to de-initialise
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
GCQ_ERRORS_TYPE xGCQDeinit( GCQInstance *pxGCQInstance );

/**
 * @brief    Attempt to attach to the consumer, needs to be called before
 *           data can be consumed.
 *           Internally the function will:
 *           - Check driver has been initilaised
 *           - Check for consumer ring buffer for the magic metadata
 *           - Check the internal version matches
 *           - Prepare the ring buffer to receive data
 *
 * @param    pxGCQInstance is the instance of the sGCQ
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
GCQ_ERRORS_TYPE xGCQAttachConsumer( GCQInstance *pxGCQInstance );

/**
 * @brief    Function to consume/read data from the sGCQ
 *           Internally the function will:
 *           - Check driver has been initilaised
 *           - Check driver has attached to the consumer
 *           - Attempt to read data if any is available
 *
 * @param    pxGCQInstance is the instance of the sGCQ
 * @param    pucData is the pointer to the data to be populated on receive
 * @param    ulDatalLen is the length of the data received
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
GCQ_ERRORS_TYPE xGCQConsumeData( GCQInstance *pxGCQInstance,
    uint8_t *pucData, uint32_t ulDatalLen );

/**
 * @brief    Function to produce/send data to the sGCQ
 *           Internally the function will:
 *           - Check driver has been initilaised
 *           - Attempt to send data
 *
 * @param    pxGCQInstance is the instance of the sGCQ
 * @param    pucData is the pointer to be data to be sent
 * @param    ulDataLen the length of the data being sent
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
GCQ_ERRORS_TYPE xGCQProduceData( GCQInstance *pxGCQInstance,
    uint8_t *pucData, uint32_t ulDataLen );

/**
 * @brief    Gets version information from gcq_version.h
 *
 * @return   OK                  Version set successfully
 *           ERROR               Version not set successfully
 */
int iGCQGetVersion( GCQVersion *pxVersion );

#endif /* _GCQ_H_ */
