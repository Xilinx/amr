/**
 * Copyright (C) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This header file contains structures, type definitions and function declarations
 * for the sGCQ driver.
 *
 * @file gcq.h
 */

#ifndef _GCQ_H_
#define _GCQ_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/idr.h>


/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/
#ifndef GCQ_DEBUG_ENABLE
#define GCQ_DEBUG_ENABLE    (0)   /**< Debug logging disabled by default */
#endif
#if GCQ_DEBUG_ENABLE
#define GCQ_DEBUG(x...)     { printk("[sGCQ Driver] " x); }
#else
#define GCQ_DEBUG(x...)     (void) (0)
#endif


#ifndef GCQ_MAX_INSTANCES
#define GCQ_MAX_INSTANCES	(4)   /**< Default value, but can be overridden by build environmental variable  */
#endif

#define gcq_assert(x)                                                           \
do {    if (x) break;                                                           \
        printk(KERN_EMERG "### ASSERTION FAILED [sGCQ Driver] %s: %s: %d: %s\n",\
               __FILE__, __func__, __LINE__, #x); dump_stack(); BUG();          \
} while ( 0 )

#define GCQ_UDID_LEN                  ( 16 )

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
    GCQ_ERRORS_NONE = 0,
    GCQ_ERRORS_DRIVER_NOT_INITIALISED 	= 1,
    GCQ_ERRORS_DRIVER_IN_USE		= 2,
    GCQ_ERRORS_NO_FREE_INSTANCES	= 3,
    GCQ_ERRORS_NO_FREE_PROFILES		= 4,
    GCQ_ERRORS_INVALID_INSTANCE		= 5,
    GCQ_ERRORS_INVALID_ARG		= 6,
    GCQ_ERRORS_INVALID_HANDLE		= 7,
    GCQ_ERRORS_INVALID_SLOT_SIZE	= 8,
    GCQ_ERRORS_INVALID_VERSION		= 9,
    GCQ_ERRORS_INVALID_NUM_SLOTS	= 10,
    GCQ_ERRORS_INVALID_PROFILE		= 11,
    GCQ_ERRORS_INVALID_PARAMS		= 12,
    GCQ_ERRORS_CONSUMER_NOT_ATTACHED	= 13,
    GCQ_ERRORS_CONSUMER_NOT_AVAILABLE	= 14,
    GCQ_ERRORS_CONSUMER_NO_DATA_RECEIVED = 15,
    GCQ_ERRORS_PRODUCER_NO_FREE_SLOTS	= 16,
    GCQ_ERRORS_NOT_SUPPORTED		= 17,

    MAX_GCQ_ERRORS_TYPE

} GCQ_ERRORS_TYPE;


/**
 * @enum    COMMON_IOCTRL_OPTIONS
 *
 * @brief   IO ctrl options common
 */
typedef enum
{
    COMMON_IOCTRL_FLUSH_TX = 0,
    COMMON_IOCTRL_FLUSH_RX,
    COMMON_IOCTRL_GET_RX_MODE,
    COMMON_IOCTRL_ENABLE_DEBUG_PRINT,
    COMMON_IOCTRL_DISABLE_DEBUG_PRINT,

    MAX_COMMON_IOCTRL_OPTION

} COMMON_IOCTRL_OPTIONS;

/**
 * @struct  GCQ_STATE
 * @brief   The internal sGCQ IF state
 */
typedef enum
{
    GCQ_STATE_CLOSED      = 0,
    GCQ_STATE_INIT        = 1,
    GCQ_STATE_OPENED      = 2,
    GCQ_STATE_ATTACHED    = 3,

    GCQ_STATE_MAX

} GCQ_STATE;

/**
 * @struct  GCQInitCfg
 * @brief   config options for sGCQ initialisation (generic across all sGCQ interfaces)
 */
typedef struct GCQInitCfg
{
    void       *pvIOAccess;

} GCQInitCfg;


/**
 * @struct  GCQProfile
 * @brief   The definition of a profile, used to store internal
 *          state & ptr to driver instance
 */
typedef struct
{
    uint32_t ulIOHandle;
    GCQ_STATE xState;
    GCQInstance *pxGCQInstance;

} GCQProfile;


/**
 * @struct  GCQCfg
 * @brief   config options for sGCQ interfaces (generic across all sGCQ interfaces)
 */
typedef struct GCQCfg
{
    uint64_t    ullBaseAddress;
    uint64_t    ullRingAddress;
    uint32_t    ulRingLength;
    uint32_t    ulCompletionQueueSlotSize;
    uint32_t    ulSubmissionQueueSlotSize;
    uint8_t     udid[ GCQ_UDID_LEN ];

    void        *pvProfile;      /* opaque handle to store internal context */

} GCQCfg;

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
 * @param    ullBaseAddr is the base address of the sGCQ
 * @param    ullRingAddr is the base address of the shared memory for allocating slots
 * @param    ullRingLen is the length of the shared memory provided
 * @param    ulSQSlotSize is the required submission queue (SQ) slot size
 * @param    ulCQSlotSize is the required completion queue (CQ) slot size
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
GCQ_ERRORS_TYPE xGCQInit(GCQInstance **ppxGCQInstance,
                         const GCQIOAccess *pxIOAccess,
                         uint64_t ullBaseAddr,
                         uint64_t ullRingAddr,
                         uint64_t ullRingLen,
                         uint32_t ulSQSlotSize,
                         uint32_t ulCQSlotSize);

/**
 * @brief    De-initialise a sGCQ driver instance
 *
 * @param    ppxGCQInstance is the instance to de-initialise
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
GCQ_ERRORS_TYPE xGCQDeinit(GCQInstance *pxGCQInstance);

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
GCQ_ERRORS_TYPE xGCQAttachConsumer(GCQInstance *pxGCQInstance);

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
GCQ_ERRORS_TYPE xGCQConsumeData(GCQInstance *pxGCQInstance,
    uint8_t *pucData, uint32_t ulDatalLen);

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
GCQ_ERRORS_TYPE xGCQProduceData(GCQInstance *pxGCQInstance,
    uint8_t *pucData, uint32_t ulDataLen);

/**
 * @brief    Gets version information from gcq_version.h
 *
 * @return   OK                  Version set successfully
 *           ERROR               Version not set successfully
 */
int iGCQGetVersion( GCQVersion *pxVersion );

/*****************************************************************************/
/* Public Functions                                                          */
/*****************************************************************************/

/**
 * @brief   initialisation function for sGCQ interfaces (generic across all sGCQ interfaces)
 *
 * @param   xInitCfg    pointer to the config to initialise the driver with
 *
 * @return  See GCQ_ERRORS
 */
uint32_t GCQ_Init(GCQInitCfg *pxInitCfg);

/**
 * @brief   Local implementation of gcq_open
 */
uint32_t gcq_open(void *pvFWIf);


/**
 * @brief   Local implementation of gcq_close
 */
uint32_t gcq_close(void *pvFWIf);

/**
 * @brief   Local implementation of gcq_read
 */
uint32_t gcq_read(void *pvFWIf,
        uint64_t ullSrcPort, uint8_t *pucData, uint32_t *pulSize, uint32_t ulTimeoutMs);

/**
 * @brief   Local implementation of gcq_write
 */
uint32_t gcq_write(void *pvFWIf,
        uint64_t ullDstPort, uint8_t *pucData, uint32_t ulSize, uint32_t ulTimeoutMs);


#endif /* _GCQ_H_ */
