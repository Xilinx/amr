/**
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the user API definitions for the sGCQ driver.
 *
 * @file gcq_driver.c
 */

#include "gcq_internal.h"
#include "standard.h"


/*****************************************************************************/
/* Defines                                                                   */
/*****************************************************************************/

#define GCQ_INSTANCE_UPPER_FIREWALL ( 0xBEEFCAFE )
#define GCQ_INSTANCE_LOWER_FIREWALL ( 0xDEADFACE )

#define CHECK_FIREWALLS( f )        \
    ( ( f->ulUpperFirewall != GCQ_INSTANCE_UPPER_FIREWALL ) && \
      ( f->ulLowerFirewall != GCQ_INSTANCE_LOWER_FIREWALL ) )

#define CHECK_32BIT_ALIGNMENT( x )  ( 0 == ( x & 0x3 ) )


/******************************************************************************/
/* Structs                                                                    */
/******************************************************************************/

/**
 * @struct  GCQPrivateData
 *
 * @brief   Structure to hold ths driver's private data
 *
 * @note    Initialisation status is checked per instance.
 */
typedef struct
{
    uint32_t        ulUpperFirewall;

    uint8_t         ucConsumerAttached;
    uint8_t         ucInstancesAllocated;
    GCQInstance     xGCQInstances[ GCQ_MAX_INSTANCES ];

    uint32_t        ulLowerFirewall;

} GCQPrivateData;


/*****************************************************************************/
/* Local Variables                                                           */
/*****************************************************************************/

static GCQPrivateData xLocalData =
{
    GCQ_INSTANCE_UPPER_FIREWALL,     /* ulUpperFirewall */

    FALSE,                           /* ucConsumerAttached */
    0,                               /* ucInstancesAllocated */
    { { 0 } },                       /* xGCQInstances */

    GCQ_INSTANCE_LOWER_FIREWALL      /* ulLowerFirewall */
};

static GCQPrivateData *pxThis = &xLocalData;


/*****************************************************************************/
/* Private Functions                                                          */
/*****************************************************************************/

/**
 * @brief   Calculate the number of slots that can be allocated based on
 *          the length of the ring and the SQ & CQ slot sizes
 *
 * @param   ullRingLen is the ring buffer length
 * @param   ulSQSlotSize is the SQ slot size
 * @param   ulCQSlotSize is the CQ slot size
 *
 * @return  the number of slots that can be allocated
 */
static inline uint32_t prvulGCQAllocNumSlots( uint64_t ullRingLen,
                                              const uint32_t ulSQSlotSize,
                                              const uint32_t ulCQSlotSize )
{
    uint32_t ulTotalLen = 0;
    uint32_t ulNumSlots = 1;

    while ( ulTotalLen <= ullRingLen )
    {
        /* Allocate an even number of slots starting at 2 */
        ulNumSlots <<= 1;

        /* The ring length is based on the ring header plus a number of
           SQ/CQ slots */
        ulTotalLen = prvulGCQRingLen( ulNumSlots, ulSQSlotSize, ulCQSlotSize );
    }
    return ( ulNumSlots >> 1 );
}

/**
 * @brief   Fast forward both the producer & consumer
 *
 * @param   pxGCQInstance the gcq driver instance
 * @param   pxRing the sq or cq ring buffer
 *
 * @return  N/A
 */
static inline void prvvGCQFastForward( const GCQInstance *pxGCQInstance,
                                       GCQRing *pxRing )
{
    prvvGCQRingReadProduced( pxGCQInstance->pxGCQIOAccess, pxRing );
    prvvGCQRingReadConsumed( pxGCQInstance->pxGCQIOAccess, pxRing );
}

/**
 * @brief   Check if the producer has any more free slots
 *
 * @param   pxGCQInstance the gcq driver instance
 * @param   pxRing the sq or cq ring buffer
 *
 * @return  returns true if can produce
 */
static inline uint32_t prvucGCQCanProduce( const GCQInstance *pxGCQInstance,
                                           GCQRing *pxRing )
{
    uint32_t ulStatus = FALSE;

    if ( ( NULL != pxGCQInstance ) && ( NULL != pxRing ) )
    {
        if ( FALSE == likely( prvucGCQRingIsFull( pxRing ) ) )
        {
            ulStatus = TRUE;
        }
        else
        {
            prvvGCQRingReadConsumed( pxGCQInstance->pxGCQIOAccess, pxRing );
            ulStatus = ( FALSE == prvucGCQRingIsFull( pxRing ) );
        }
    }
    return ulStatus;
}

/**
 * @brief   Check if the consumer has data that can be consumed
 *
 * @param   pxGCQInstance the gcq driver instance
 * @param   pxRing the sq or cq ring buffer
 *
 * @return  returns TRUE if data can be consumed
 */
static inline uint32_t prvucGCQCanConsume( const GCQInstance *pxGCQInstance,
                                           GCQRing *pxRing )
{
    uint32_t ulStatus = FALSE;

    if ( ( NULL != pxGCQInstance ) && ( NULL != pxRing ) )
    {
        const GCQIOAccess *pxGCQIOAccess = pxGCQInstance->pxGCQIOAccess;

        /* Check for errors */
        uint32_t ulSqTailPointer = pxGCQIOAccess->xGCQReadMem32(
            pxGCQInstance->ullBaseAddr + GCQ_PRODUCER_SQ_TAIL_POINTER );

        uint32_t ulCqTailPointer = pxGCQIOAccess->xGCQReadMem32(
            pxGCQInstance->ullBaseAddr + GCQ_PRODUCER_CQ_TAIL_POINTER );

        if ( unlikely( ( ( uint32_t )-1 == ulSqTailPointer ) &&
           ( ( uint32_t )-1 == ulCqTailPointer ) ) )
        {
            ulStatus = FALSE;
        }
        else if ( FALSE == likely( prvucGCQRingIsEmpty( pxRing ) ) )
        {
            ulStatus = TRUE;
        }
        else
        {
            prvvGCQRingReadProduced( pxGCQIOAccess, pxRing );
            ulStatus = ( FALSE == prvucGCQRingIsEmpty( pxRing ) );
        }
    }
    return ulStatus;
}

/**
 * @brief   Set consumed to be the same as produced to ignore any existing
 *          commands.
 *
 * @param   pxGCQInstance the gcq driver instance
 * @param   pxRing the sq or cq ring buffer
 *
 * @return  N/A
 */
static inline void prvvGCQSoftReset( const GCQInstance *pxGCQInstance,
                                     GCQRing *pxRing )
{
    prvvGCQRingReadProduced( pxGCQInstance->pxGCQIOAccess, pxRing );
    pxRing->ulRingConsumed = pxRing->ulRingProduced;
    prvvGCQRingWriteConsumed( pxGCQInstance->pxGCQIOAccess, pxRing );
}

/**
 * @brief   Attempt to add data into the producer, can fail if no more
 *          free slots
 *
 * @param   pxGCQInstance the gcq driver instance
 * @param   ullSlotAddr is the slot address
 *
 * @return  returns TRUE if produced else error
 */
static inline GCQ_ERRORS_TYPE prvxGCQProduce( GCQInstance *pxGCQInstance,
                                              uint64_t *ullSlotAddr )
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_NONE;

    if ( ( NULL == pxGCQInstance ) || ( NULL == ullSlotAddr ) )
    {
        xStatus = GCQ_ERRORS_INVALID_ARG;
    }
    else
    {
        /* Bind into the correct ring buffer */
        GCQRing *pxRing = pxGCQInstance->pxGCQProducer;

        /* Check if there is a free slot */
        if ( TRUE == likely( prvucGCQCanProduce( pxGCQInstance, pxRing ) ) )
        {
            *ullSlotAddr = prvullGCQRingGetSlotPtrProduced( pxRing );
            pxRing->ulRingProduced++;
        }
        else
        {
            xStatus = GCQ_ERRORS_PRODUCER_NO_FREE_SLOTS;
        }
    }
    return xStatus;
}


/**
 * @brief   Attempt to consume data from the consumer, can fail is no data
 *          is available to read
 *
 * @param   pxGCQInstance the gcq driver instance
 * @param   ullSlotAddr is the slot address
 *
 * @return  returns TRUE if consumed else error
 */
static inline GCQ_ERRORS_TYPE prvxGCQConsume( GCQInstance *pxGCQInstance,
                                              uint64_t *ullSlotAddr )
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_NONE;

    if ( ( NULL == pxGCQInstance ) || ( NULL == ullSlotAddr ) )
    {
        xStatus = GCQ_ERRORS_INVALID_ARG;
    }
    else
    {
        /* Bind into the correct ring */
        GCQRing *pxRing = pxGCQInstance->pxGCQConsumer;

        if ( TRUE == likely( prvucGCQCanConsume( pxGCQInstance, pxRing ) ) )
        {
            *ullSlotAddr = prvullGCQRingGetSlotPtrConsumed( pxRing );
            pxRing->ulRingConsumed++;
        }
        else
        {
            xStatus = GCQ_ERRORS_CONSUMER_NO_DATA_RECEIVED;
        }
    }
    return xStatus;
}

/**
 * @brief   Attempt to find an uninitialized sGCQ instance
 *
 * @param   ppxGCQInstance variable to store the gcq driver instance
 *
 * @return  returns GCQ_ERRORS_NONE if instance found, error otherwise
 */
static inline GCQ_ERRORS_TYPE prviGCQFindNextFreeInstance( GCQInstance **ppxGCQInstance )
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_NO_FREE_INSTANCES;

    if ( NULL == ppxGCQInstance || NULL != *ppxGCQInstance )
    {
        xStatus = GCQ_ERRORS_INVALID_ARG;
    }
    else if ( GCQ_MAX_INSTANCES > pxThis->ucInstancesAllocated )
    {
        for ( int iIndex = 0; GCQ_MAX_INSTANCES > iIndex; iIndex++ )
        {
            if ( FALSE == pxThis->xGCQInstances[ iIndex ].iInitialised )
            {
                *ppxGCQInstance = &pxThis->xGCQInstances[ iIndex ];
                xStatus = GCQ_ERRORS_NONE;
                break;
            }
        }
    }
    return xStatus;
}

/**
 * @brief    Initial sGCQ mode configuration
 */
static inline GCQ_ERRORS_TYPE xGCQHWInit( GCQ_MODE_TYPE xMode,
                            uint64_t ullBaseAddr,
                            uint64_t ullRingAddr,
                            const GCQIOAccess *pxGCQIOAccess )
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_NONE;
    uint32_t ulValue = 0;

    /*
     * The high/low memory address are meant to be used to store the base address of
     * where the ring buffer is located in memory, in the current HW implementation this
     * doesn't seen to be used, setting anyway as will maybe be required in the future.
     */
    switch ( xMode )
    {
        case GCQ_MODE_TYPE_CONSUMER_MODE:
            /*
             * In consumer mode we don't perform a soft reset and the HW is owned by the producer
             */
            ulValue = ( uint32_t )( ullRingAddr );
            pxGCQIOAccess->xGCQWriteMem32( ( ullBaseAddr + GCQ_CONSUMER_CQ_MEM_ADDR_LOW ), ulValue );
            ulValue = ( uint32_t ) ( ullRingAddr >> 32 );
            pxGCQIOAccess->xGCQWriteMem32( ( ullBaseAddr + GCQ_CONSUMER_CQ_MEM_ADDR_HIGH ), ulValue );
            break;

        case GCQ_MODE_TYPE_PRODUCER_MODE:
            /*
             * Performs a soft reset of all submission queue and completion queues.
             * The reset field is self-clearing once set.
             */
            ulValue = ( uint32_t ) ( ullRingAddr );
            pxGCQIOAccess->xGCQWriteMem32( ( ullBaseAddr + GCQ_PRODUCER_SQ_MEM_ADDR_LOW ), ulValue );
            ulValue = ( uint32_t ) ( ullRingAddr >> 32 );
            pxGCQIOAccess->xGCQWriteMem32( ( ullBaseAddr + GCQ_PRODUCER_SQ_MEM_ADDR_HIGH ), ulValue );
            break;

        default:
            xStatus = GCQ_ERRORS_INVALID_ARG;
            break;
    }

    if ( GCQ_ERRORS_NONE == xStatus)
    {
        GCQ_DEBUG( "sGCQ Init Complete: (%s) (0x%llx)\r\n",
            (xMode == GCQ_MODE_TYPE_PRODUCER_MODE) ? "PRODUCER MODE" : "CONSUMER_MODE",
            ullBaseAddr );
    }

    return xStatus;
}

/*****************************************************************************/
/* Public Functions                                                          */
/*****************************************************************************/

/**
 * @brief    Initialise the sGCQ standalone driver
 */
GCQ_ERRORS_TYPE xGCQInit( GCQInstance **ppxGCQInstance,
                          const GCQIOAccess *pxGCQIOAccess,
                          GCQ_MODE_TYPE xMode,
                          uint64_t ullBaseAddr,
                          uint64_t ullRingAddr,
                          uint64_t ullRingLen,
                          uint32_t ulSQSlotSize,
                          uint32_t ulCQSlotSize )
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_INVALID_ARG;

    GCQHeader xGCQHeader = { };
    uint32_t ullNumSlots = 0;

    if ( ( GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
         ( GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall ) )
    {
        if ( NULL != *ppxGCQInstance )
        {
            xStatus = GCQ_ERRORS_INVALID_ARG;
        }
        /* Find the next free instance */
        else
        {
            xStatus = prviGCQFindNextFreeInstance( ppxGCQInstance );
        }

        /* Slot Validation */
        if ( GCQ_ERRORS_NONE == xStatus )
        {
            if ( ( ulSQSlotSize % sizeof( uint32_t ) ) ||
                 ( ulCQSlotSize % sizeof( uint32_t ) ) )
            {
                GCQ_DEBUG( "Error: Invalid SQ/CQ slot size specified, needs to be 32-bit aligned!\r\n" );
                xStatus = GCQ_ERRORS_INVALID_SLOT_SIZE;
            }
            else if ( ( 0 == ulSQSlotSize ) ||
                      ( 0 == ulCQSlotSize ) )
            {
                GCQ_DEBUG( "Error: Invalid SQ/CQ slot size specified, zero is not a valid value\r\n" );
                xStatus = GCQ_ERRORS_INVALID_SLOT_SIZE;
            }

            if ( GCQ_ERRORS_NONE == xStatus )
            {
                ullNumSlots = prvulGCQAllocNumSlots( ullRingLen, ulSQSlotSize, ulCQSlotSize );
                if ( GCQ_MIN_NUM_SLOTS > ullNumSlots )
                {
                    GCQ_DEBUG( "Error: Number of slots calculated less than minimum supported value of %d\r\n",
                               GCQ_MIN_NUM_SLOTS );
                    xStatus = GCQ_ERRORS_INVALID_NUM_SLOTS;
                }
            }
        }

        /* Populate the Instance data */
        if ( GCQ_ERRORS_NONE == xStatus )
        {
            uint64_t ullCQProduced = 0;
            uint64_t ullSQProduced = 0;
            GCQInstance *pxGCQInstance = *ppxGCQInstance;

            /* Init IP block and configure interrupt mode */
            xGCQHWInit(xMode, ullBaseAddr, ullRingAddr, pxGCQIOAccess );

            if ( GCQ_MODE_TYPE_PRODUCER_MODE == xMode )
            {
                pxGCQInstance->ulProducerSlotSize = ulCQSlotSize;
                pxGCQInstance->ulConsumerSlotSize = ulSQSlotSize;
                /* In producer mode we produce onto the CQ and consume from the SQ */
                pxGCQInstance->pxGCQProducer = &( pxGCQInstance )->xGCQCq;
                pxGCQInstance->pxGCQConsumer = &( pxGCQInstance )->xGCQSq;
                /* Set the producer address */
                ullSQProduced = ullBaseAddr + GCQ_PRODUCER_SQ_TAIL_POINTER;
                ullCQProduced = ullBaseAddr + GCQ_PRODUCER_CQ_TAIL_POINTER;
            }
            else
            {
                pxGCQInstance->ulProducerSlotSize = ulSQSlotSize;
                pxGCQInstance->ulConsumerSlotSize = ulCQSlotSize;
                /* In consumer mode we produce onto the SQ and consume from the CQ */
                pxGCQInstance->pxGCQProducer = &( pxGCQInstance )->xGCQSq;
                pxGCQInstance->pxGCQConsumer = &( pxGCQInstance )->xGCQCq;
                /* Set the producer address */
                ullSQProduced = ( ullBaseAddr + GCQ_CONSUMER_SQ_TAIL_POINTER );
                ullCQProduced = ( ullBaseAddr + GCQ_CONSUMER_CQ_TAIL_POINTER );
            }

            /* Init SQ & CQ rings */
            GCQ_DEBUG( "sGCQ Init Ring SQ\r\n" );
            prvvGCQInitRing( pxGCQInstance,
                             &( pxGCQInstance )->xGCQSq,
                             ullSQProduced,
                             ullRingAddr + offsetof( GCQHeader, ulHdrSQConsumed ),
                             ullRingAddr + sizeof( GCQHeader ),
                             ullNumSlots,
                             ulSQSlotSize);
            GCQ_DEBUG( "sGCQ Init Ring CQ\r\n" );
            prvvGCQInitRing( pxGCQInstance,
                             &( pxGCQInstance )->xGCQCq,
                             ullCQProduced,
                             ullRingAddr + offsetof( GCQHeader, ulHdrCQConsumed ),
                             ullRingAddr + sizeof( GCQHeader ) + ullNumSlots * ulSQSlotSize,
                             ullNumSlots,
                             ulCQSlotSize );

            pxGCQInstance->iInitialised = FALSE;
            pxGCQInstance->xMode            = xMode;
            pxGCQInstance->ullGCQHeaderAddr = ullRingAddr;
            pxGCQInstance->ullBaseAddr      = ullBaseAddr;
            pxGCQInstance->ullRingAddr      = ullRingAddr;
            pxGCQInstance->pxGCQIOAccess    = pxGCQIOAccess;
            pxGCQInstance->ulUpperFirewall  = GCQ_INSTANCE_UPPER_FIREWALL;
            pxGCQInstance->ulLowerFirewall  = GCQ_INSTANCE_LOWER_FIREWALL;

            /* If producer mode then populate the header onto the ring */
            if ( GCQ_MODE_TYPE_PRODUCER_MODE == xMode )
            {
                xGCQHeader.ulHdrMagic      = 0;
                xGCQHeader.ulHdrVersion    = ( GCQ_VER_MAJOR << 16 ) + GCQ_VER_MINOR;
                xGCQHeader.ulHdrNumSlots   = ullNumSlots;
                xGCQHeader.ulHdrSQOffset   = pxGCQInstance->xGCQSq.ullRingSlotAddr - ullRingAddr;
                xGCQHeader.ulHdrSQSlotSize = ulSQSlotSize;
                xGCQHeader.ulHdrCQOffset   = pxGCQInstance->xGCQCq.ullRingSlotAddr - ullRingAddr;
                xGCQHeader.ulHdrSQConsumed = 0;
                xGCQHeader.ulHdrCQConsumed = 0;

                /* Write the header to the ring buffer */
                prvvGCQCopyToRing( pxGCQIOAccess, ( uint32_t* )&xGCQHeader, ullRingAddr,
                                   sizeof( GCQHeader ) );
                prvvGCQSoftReset( pxGCQInstance, &( pxGCQInstance )->xGCQSq );
                prvvGCQSoftReset( pxGCQInstance, &( pxGCQInstance )->xGCQCq );

                /* Write the magic number to confirm the header is fully initialized */
                xGCQHeader.ulHdrMagic = GCQ_ALLOC_MAGIC;
                prvvGCQCopyToRing( pxGCQIOAccess, ( uint32_t* )&xGCQHeader, ullRingAddr, sizeof( uint32_t ) );
            }

            /* Set flag to show initialisation complete */
            pxGCQInstance->iInitialised = TRUE;
            pxThis->ucInstancesAllocated++;
        }
    }
    return xStatus;
}

/**
 * @brief    De-initialise a sGCQ driver instance
 */
GCQ_ERRORS_TYPE xGCQDeinit( GCQInstance *pxGCQInstance )
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_INVALID_ARG;

    if ( ( GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
         ( GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall ) )
    {
        xStatus = GCQ_ERRORS_NONE;

        if ( ( NULL == pxGCQInstance ) || ( FALSE == pxGCQInstance->iInitialised ) )
        {
            xStatus = GCQ_ERRORS_INVALID_INSTANCE;
        }
        else
        {
            pxGCQInstance->iInitialised = FALSE;
            pxThis->ucInstancesAllocated--;
        }
    }
    return xStatus;
}

/**
 * @brief    Attempt to attach to the consumer, needs to be called before
 *           data can be consumed.
 */
GCQ_ERRORS_TYPE xGCQAttachConsumer( GCQInstance *pxGCQInstance )
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_INVALID_ARG;
    GCQHeader xGCQHeader = { };

    if ( ( GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
         ( GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall ) )
    {
        xStatus = GCQ_ERRORS_NONE;

        if ( ( NULL == pxGCQInstance ) || ( FALSE == pxGCQInstance->iInitialised ) )
        {
            xStatus = GCQ_ERRORS_INVALID_INSTANCE;
        }
        else
        {
            /* Copy header from the ring buffer */
            prvvGCQCopyFromRing( pxGCQInstance->pxGCQIOAccess,
                                 ( uint32_t* )&xGCQHeader,
                                 pxGCQInstance->ullRingAddr,
                                 sizeof( uint32_t ) );

            /* Magic number is to confirm the header is fully initialized */
            if ( xGCQHeader.ulHdrMagic != GCQ_ALLOC_MAGIC )
            {
                xStatus = GCQ_ERRORS_CONSUMER_NOT_AVAILABLE;
            }
        }

        /* Check the version within the header is as expected */
        if ( GCQ_ERRORS_NONE == xStatus )
        {
            prvvGCQCopyFromRing( pxGCQInstance->pxGCQIOAccess,
                                 ( uint32_t* )&xGCQHeader,
                                 pxGCQInstance->ullRingAddr,
                                 sizeof( GCQHeader ) );

            if ( GCQ_VER_MAJOR != GET_GCQ_MAJOR( xGCQHeader.ulHdrVersion ) )
            {
                GCQ_DEBUG( "Error: Unexpected version:0x%lx in magic header!\r\n",
                           xGCQHeader.ulHdrVersion );
                xStatus = GCQ_ERRORS_INVALID_VERSION;
            }
            else
            {
                GCQ_DEBUG( "Version: 0x%lx 0x%lx\n", GET_GCQ_MAJOR( xGCQHeader.ulHdrVersion ),
                           GET_GCQ_MINOR( xGCQHeader.ulHdrVersion ) );
            }
        }

        /* Validate the number of slots matches */
        if ( GCQ_ERRORS_NONE == xStatus )
        {
            uint32_t ullHdrNumSlots = xGCQHeader.ulHdrNumSlots;
            uint32_t ullNumSlots    = pxGCQInstance->xGCQSq.ulRingNumSlots;

            if ( ullNumSlots != ullHdrNumSlots )
            {
                GCQ_DEBUG( "Error: Invalid number of slots:%ld found in magic header, expecting: %ld!!\r\n",
                           ullHdrNumSlots, ullNumSlots );
                xStatus = GCQ_ERRORS_INVALID_NUM_SLOTS;
            }
        }

        /* Validate the slot size matches */
        if ( GCQ_ERRORS_NONE == xStatus )
        {
            uint32_t ulHdrSlotSize  = xGCQHeader.ulHdrSQSlotSize;
            uint32_t ulRingSlotSize = pxGCQInstance->xGCQSq.ulRingSlotSize;
            if ( ulRingSlotSize != ulHdrSlotSize )
            {
                GCQ_DEBUG( "Error: Invalid slot size:%ld found in magic header, expecting: %ld!\r\n",
                           ulHdrSlotSize, ulRingSlotSize );
                xStatus = GCQ_ERRORS_INVALID_SLOT_SIZE;
            }
            else
            {
                prvvGCQFastForward( pxGCQInstance, &pxGCQInstance->xGCQSq );
                prvvGCQFastForward( pxGCQInstance, &pxGCQInstance->xGCQCq );

                /* Set flag to show now attached */
                pxThis->ucConsumerAttached = TRUE;
            }
        }
    }
    return xStatus;
}

/**
 * @brief    Function to consume data from the ring buffer
 */
GCQ_ERRORS_TYPE xGCQConsumeData( GCQInstance *pxGCQInstance,
                                 uint8_t *pucData,
                                 uint32_t ulDataLen )
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_INVALID_ARG;
    uint64_t ullSlotAddr = 0;

    if ( ( GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
         ( GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall ) )
    {
        xStatus = GCQ_ERRORS_NONE;

        if ( ( NULL == pxGCQInstance ) ||
             ( FALSE == pxGCQInstance->iInitialised ) )
        {
            xStatus = GCQ_ERRORS_INVALID_INSTANCE;
        }
        /* Check if it is attached */
        else if ( ( GCQ_MODE_TYPE_CONSUMER_MODE == pxGCQInstance->xMode ) &&
                  ( FALSE == pxThis->ucConsumerAttached ) )
        {
            xStatus = GCQ_ERRORS_CONSUMER_NOT_ATTACHED;
        }
        else if ( ulDataLen > pxGCQInstance->ulConsumerSlotSize )
        {
            GCQ_DEBUG( " Error: length 0x%lx specified is larger than slot configured\r\n",
                       ulDataLen );
            xStatus = GCQ_ERRORS_INVALID_ARG;
        }
        else if ( NULL == pucData )
        {
            xStatus = GCQ_ERRORS_INVALID_ARG;
        }
        else if ( !CHECK_32BIT_ALIGNMENT( ulDataLen ) )
        {
            GCQ_DEBUG( " Error: length 0x%lx is not 32bit aligned\r\n", ulDataLen );
            xStatus = GCQ_ERRORS_INVALID_ARG;
        }

        if ( GCQ_ERRORS_NONE == xStatus )
        {
            /* Attempt to consume data if any is available */
            xStatus = prvxGCQConsume( pxGCQInstance, &ullSlotAddr );
        }

        if ( GCQ_ERRORS_NONE == xStatus )
        {
            GCQ_DEBUG( "Read data from slot addr:0x%llx len:%ld\r\n", ullSlotAddr, ulDataLen );

            /* Process the data & populate the return buffer */
            for ( uint32_t offset = 0; offset < ulDataLen; offset += 4 )
            {
                *( uint32_t *)( pucData + offset ) =
                    pxGCQInstance->pxGCQIOAccess->xGCQReadMem32( ullSlotAddr + offset );
                GCQ_DEBUG( "Read addr:0x%llx val:0x%lx\r\n",
                           ullSlotAddr + offset, *( uint32_t* )( pucData + offset ) );
            }

            /* Notify the peer the data has been consumed */
            prvvGCQRingWriteConsumed( pxGCQInstance->pxGCQIOAccess, pxGCQInstance->pxGCQConsumer );
        }
    }
    return xStatus;
}

/**
 * @brief    Function to provide data and populate the ring buffer
 */
GCQ_ERRORS_TYPE xGCQProduceData( GCQInstance *pxGCQInstance,
                                 uint8_t * pucData,
                                 uint32_t ulDataLen )
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_INVALID_ARG;

    uint64_t ullSlotAddr = 0;

    if ( ( GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
         ( GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall ) )
    {
        xStatus = GCQ_ERRORS_NONE;

        if ( ( NULL == pxGCQInstance ) ||
             ( FALSE == pxGCQInstance->iInitialised ) )
        {
            xStatus = GCQ_ERRORS_INVALID_INSTANCE;
        }
        else if ( ulDataLen > pxGCQInstance->ulProducerSlotSize )
        {
            GCQ_DEBUG( " Error: length 0x%lx specified is larger than slot configured\r\n", ulDataLen );
            xStatus = GCQ_ERRORS_INVALID_ARG;
        }
        else if ( NULL == pucData )
        {
            xStatus = GCQ_ERRORS_INVALID_ARG;
        }
        else if ( !CHECK_32BIT_ALIGNMENT( ulDataLen ) )
        {
            GCQ_DEBUG( " Error: length 0x%lx is not 32bit aligned\r\n", ulDataLen );
            xStatus = GCQ_ERRORS_INVALID_ARG;
        }

        if ( GCQ_ERRORS_NONE == xStatus )
        {
            xStatus = prvxGCQProduce( pxGCQInstance, &ullSlotAddr );
        }

        if ( GCQ_ERRORS_NONE == xStatus )
        {
            GCQ_DEBUG( "Write data to slot addr:0x%llx len:%ld\r\n", ullSlotAddr, ulDataLen );

            for ( uint32_t offset = 0; offset < ulDataLen; offset += 4 )
            {
                pxGCQInstance->pxGCQIOAccess->xGCQWriteMem32(
                    ullSlotAddr + offset,
                    *( uint32_t * )( pucData + offset ) );
                GCQ_DEBUG( "Write addr:0x%llx val:0x%lx\r\n",
                           ullSlotAddr + offset,
                           *( uint32_t * )( pucData + offset ) );
            }

            prvvGCQRingWriteProduced( pxGCQInstance->pxGCQIOAccess, pxGCQInstance->pxGCQProducer );
        }
        else
        {
            GCQ_DEBUG( "Error: Failed to add data into slot: %d\r\n", xStatus );
        }
    }
    return xStatus;
}

/**
 * @brief    Sets this modules version information
 */
int iGCQGetVersion( GCQVersion *pxVersion )
{
    int iStatus = GCQ_ERRORS_INVALID_ARG;

    if ( ( GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
         ( GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
         ( NULL != pxVersion ) )
    {

        pxVersion->ucVerMajor   = GCQ_VER_MAJOR;
        pxVersion->ucVerMinor   = GCQ_VER_MINOR;
        pxVersion->ucVerPatch   = GCQ_VER_PATCH;
        pxVersion->ucDevCommits = GCQ_VER_DEV_COMMITS;

        iStatus = GCQ_ERRORS_NONE;
    }
    return iStatus;
}
