/**
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the user API definitions for the sGCQ driver.
 *
 * @file gcq_driver.c
 */

#include "gcq_internal.h"


/*****************************************************************************/
/* Defines                                                                   */
/*****************************************************************************/

#define GCQ_INSTANCE_UPPER_FIREWALL    (0xBEEFCAFE)
#define GCQ_INSTANCE_LOWER_FIREWALL    (0xDEADFACE)

#define CHECK_FIREWALLS(f)  ((f->ulUpperFirewall != GCQ_INSTANCE_UPPER_FIREWALL) && \
                             (f->ulLowerFirewall != GCQ_INSTANCE_LOWER_FIREWALL))

#define CHECK_32BIT_ALIGNMENT(x)    (0 == (x & 0x3))


/******************************************************************************/
/* Structs                                                                    */
/******************************************************************************/

/**
 * @struct  GCQPrivateData
 *
 * @brief   Structure to hold the driver's private data
 *
 * @note    Initialisation status is checked per instance.
 */
typedef struct
{
    uint32_t    ulUpperFirewall;

    bool        ucConsumerAttached;
    uint8_t     ucInstancesAllocated;
    GCQInstance xGCQInstances[GCQ_MAX_INSTANCES];

    uint32_t    ulLowerFirewall;

} GCQPrivateData;


/*****************************************************************************/
/* Local Variables                                                           */
/*****************************************************************************/

static GCQPrivateData xLocalData =
{
    GCQ_INSTANCE_UPPER_FIREWALL,    /* ulUpperFirewall */

    false,                          /* ucConsumerAttached */
    0,                              /* ucInstancesAllocated */
    { { 0 } },                      /* xGCQInstances */

    GCQ_INSTANCE_LOWER_FIREWALL     /* ulLowerFirewall */
};

static GCQPrivateData *pxThis = &xLocalData;


/*****************************************************************************/
/* Private Functions                                                          */
/*****************************************************************************/

/**
 * @brief   Calculate the number of slots that can be allocated based on
 *          the length of the ring and the SQ & CQ slot sizes
 *
 * @ullRingLen: ring buffer length
 * @ulSQSlotSize: SQ slot size
 * @ulCQSlotSize: CQ slot size
 *
 * @return  the number of slots that can be allocated
 */
static inline uint32_t prvulGCQAllocNumSlots(uint64_t ullRingLen,
    const uint32_t ulSQSlotSize, const uint32_t ulCQSlotSize)
{
    uint32_t ulTotalLen = 0;
    uint32_t ulNumSlots = 1;

    while (ulTotalLen <= ullRingLen) {
        /* Allocate an even number of slots starting at 2 */
        ulNumSlots <<= 1;

        /* The ring length is based on the ring header plus a number of SQ/CQ slots */
        ulTotalLen = prvulGCQRingLen(ulNumSlots, ulSQSlotSize, ulCQSlotSize);
    }

    return (ulNumSlots >> 1);
}

/**
 * @brief   Fast forward both the producer & consumer
 *
 * @param   pxGCQInstance the gcq driver instance
 * @param   pxRing the sq or cq ring buffer
 *
 * @return  N/A
 */
static inline void prvvGCQFastForward(const GCQInstance *pxGCQInstance,
    GCQRing *pxRing)
{
    gcq_assert(pxGCQInstance);
    gcq_assert(pxRing);

    pxRing->ulRingProduced =  pxGCQInstance->pxGCQIOAccess->xGCQReadMem32(
        pxRing->ullRingProducedAddr);
    pxRing->ulRingConsumed =  pxGCQInstance->pxGCQIOAccess->xGCQReadMem32(
        pxRing->ullRingConsumedAddr);
}

/**
 * @brief   Check if the producer has any more free slots
 *
 * @pxGCQInstance: gcq driver instance
 * @pxRing: the sq or cq ring buffer
 *
 * @return  returns true if can produce
 */
static inline bool prvucGCQCanProduce(const GCQInstance *pxGCQInstance,
    GCQRing *pxRing)
{
    bool ulStatus = false;

    if ((NULL != pxGCQInstance) ||
        (NULL != pxRing)) {
        if (false == likely(prvucGCQRingIsFull(pxRing))) {
            ulStatus = true;
        } else {
            pxRing->ulRingConsumed = pxGCQInstance->pxGCQIOAccess->xGCQReadMem32(
                pxRing->ullRingConsumedAddr);
            ulStatus = (false == prvucGCQRingIsFull(pxRing));
        }
    }

    return ulStatus;
}

/**
 * @brief   Check if the consumer has data that can be consumed
 *
 * @pxGCQInstance: gcq driver instance
 * @pxRing: sq or cq ring buffer
 *
 * @return  returns true if data can be consumed
 */
static inline bool prvucGCQCanConsume(const GCQInstance *pxGCQInstance,
    GCQRing *pxRing)
{
    bool ulStatus = false;

    if ((NULL != pxGCQInstance) ||
        (NULL != pxRing))
    {
        const GCQIOAccess *pxGCQIOAccess = pxGCQInstance->pxGCQIOAccess;

        /* Check for errors */
        uint32_t ulSqTailPointer = pxGCQIOAccess->xGCQReadMem32(
            pxGCQInstance->ullBaseAddr + GCQ_PRODUCER_SQ_TAIL_POINTER);

        uint32_t ulCqTailPointer = pxGCQIOAccess->xGCQReadMem32(
            pxGCQInstance->ullBaseAddr + GCQ_PRODUCER_CQ_TAIL_POINTER );

        if (unlikely(((uint32_t)-1 == ulSqTailPointer) &&
            ((uint32_t)-1 == ulCqTailPointer))) {
            ulStatus = false;
        } else if (false == likely(prvucGCQRingIsEmpty(pxRing))) {
            ulStatus = true;
        } else {
            pxRing->ulRingProduced = pxGCQInstance->pxGCQIOAccess->xGCQReadMem32(
                pxRing->ullRingProducedAddr);
            ulStatus = (false == prvucGCQRingIsEmpty(pxRing));
        }
    }

    return ulStatus;
}

/**
 * @brief   Set consumed to be the same as produced to ignore any existing
 *          commands.
 *
 * @pxGCQInstance: gcq driver instance
 * @pxRing: sq or cq ring buffer
 *
 * @return  N/A
 */
static inline void prvvGCQSoftReset(const GCQInstance *pxGCQInstance,
        GCQRing *pxRing)
{
    const GCQIOAccess *pxGCQIOAccess;
    gcq_assert(pxGCQInstance);
    gcq_assert(pxRing);

    pxGCQIOAccess = pxGCQInstance->pxGCQIOAccess;

    pxRing->ulRingProduced = pxGCQIOAccess->xGCQReadMem32(
        pxRing->ullRingProducedAddr);

    pxRing->ulRingConsumed = pxRing->ulRingProduced;
    pxGCQIOAccess->xGCQWriteMem32(pxRing->ullRingConsumedAddr,
        pxRing->ulRingConsumed);
}

/**
 * @brief   Attempt to add data into the producer, can fail if no more
 *          free slots
 *
 * @pxGCQInstance: gcq driver instance
 * @ullSlotAddr: slot address
 *
 * @return  returns true if produced else error
 */
static inline GCQ_ERRORS_TYPE prvxGCQProduce(GCQInstance *pxGCQInstance,
    uint64_t *ullSlotAddr)
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_NONE;

    if ((NULL == pxGCQInstance) ||
        (NULL == ullSlotAddr)) {
        xStatus = GCQ_ERRORS_INVALID_ARG;
    } else {
        /* Bind into the correct ring buffer */
        GCQRing *pxRing = pxGCQInstance->pxGCQProducer;

        /* Check if there is a free slot */
        if (true == likely(prvucGCQCanProduce(pxGCQInstance, pxRing))) {
            *ullSlotAddr = prvullGCQRingGetSlotPtrProduced(pxRing);
            pxRing->ulRingProduced++;
        } else {
            xStatus = GCQ_ERRORS_PRODUCER_NO_FREE_SLOTS;
        }
    }

    return xStatus;
}

/**
 * @brief   Attempt to consume data from the consumer, can fail is no data
 *          is available to read
 *
 * @pxGCQInstance: gcq driver instance
 * @ullSlotAddr: slot address
 *
 * @return  returns true if consumed else error
 */
static inline GCQ_ERRORS_TYPE prvxGCQConsume(GCQInstance *pxGCQInstance,
    uint64_t *ullSlotAddr)
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_NONE;

    if ((NULL == pxGCQInstance) ||
        (NULL == ullSlotAddr)) {
        xStatus = GCQ_ERRORS_INVALID_ARG;
    } else {
        /* Bind into the correct ring */
        GCQRing *pxRing = pxGCQInstance->pxGCQConsumer;

        if (true == likely(prvucGCQCanConsume(pxGCQInstance, pxRing))) {
            *ullSlotAddr = prvullGCQRingGetSlotPtrConsumed(pxRing);
            pxRing->ulRingConsumed++;
        } else {
            xStatus = GCQ_ERRORS_CONSUMER_NO_DATA_RECEIVED;
        }
    }

    return xStatus;
}

/**
 * @brief   Attempt to find an uninitialized sGCQ instance
 *
 * @ppxGCQInstance: pointer to pointer to gcq driver instance
 *
 * @return  returns GCQ_ERRORS_NONE if instance found, error otherwise
 */
static inline GCQ_ERRORS_TYPE prviGCQFindNextFreeInstance(
    GCQInstance **ppxGCQInstance)
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_NO_FREE_INSTANCES;

    if ((NULL == ppxGCQInstance) || (NULL != *ppxGCQInstance)) {
        xStatus = GCQ_ERRORS_INVALID_ARG;
    } else if (GCQ_MAX_INSTANCES > pxThis->ucInstancesAllocated) {
        int iIndex = 0;
        for(iIndex = 0; iIndex < GCQ_MAX_INSTANCES; iIndex++) {
            if (false == pxThis->xGCQInstances[iIndex].iInitialised) {
                *ppxGCQInstance = &pxThis->xGCQInstances[iIndex];
                xStatus = GCQ_ERRORS_NONE;
                break;
            }
        }
    }

    return xStatus;
}


/*****************************************************************************/
/* Public Functions                                                          */
/*****************************************************************************/

/**
 * @brief    Initialise the sGCQ standalone driver
 */
GCQ_ERRORS_TYPE xGCQInit(GCQInstance **ppxGCQInstance,
                          const GCQIOAccess *pxGCQIOAccess,
                          uint64_t ullBaseAddr,
                          uint64_t ullRingAddr,
                          uint64_t ullRingLen,
                          uint32_t ulSQSlotSize,
                          uint32_t ulCQSlotSize)
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_INVALID_ARG;
    uint32_t ullNumSlots = 0;

    if ((GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall) &&
        (GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall)) {
        xStatus = GCQ_ERRORS_NONE;

        if (NULL != *ppxGCQInstance)
            xStatus = GCQ_ERRORS_INVALID_ARG;
        else if (NULL == pxGCQIOAccess)
            xStatus = GCQ_ERRORS_INVALID_ARG;
        else if ((NULL == pxGCQIOAccess->xGCQReadMem32)  ||
                 (NULL == pxGCQIOAccess->xGCQWriteMem32))
            xStatus = GCQ_ERRORS_INVALID_ARG;

        /* Find the next free instance */
        if (GCQ_ERRORS_NONE == xStatus)
            xStatus = prviGCQFindNextFreeInstance(ppxGCQInstance);

        /* Slot Validation */
        if (GCQ_ERRORS_NONE == xStatus) {
            if ((ulSQSlotSize % sizeof(uint32_t)) ||
                (ulCQSlotSize % sizeof(uint32_t))) {
                GCQ_DEBUG("Error: Invalid SQ/CQ slot size specified, needs to be 32-bit aligned!\r\n");
                xStatus = GCQ_ERRORS_INVALID_SLOT_SIZE;
            } else if ((0 == ulSQSlotSize) ||
                       (0 == ulCQSlotSize)) {
                GCQ_DEBUG("Error: Invalid SQ/CQ slot size specified, zero is not a valid value\r\n");
                xStatus = GCQ_ERRORS_INVALID_SLOT_SIZE;
            }

            if (GCQ_ERRORS_NONE == xStatus) {
                ullNumSlots = prvulGCQAllocNumSlots(ullRingLen, ulSQSlotSize, ulCQSlotSize);
                if (ullNumSlots < GCQ_MIN_NUM_SLOTS) {
                    GCQ_DEBUG("Error: Number of slots calculated less than minimum supported value of %d\r\n",
                        GCQ_MIN_NUM_SLOTS);
                    xStatus = GCQ_ERRORS_INVALID_NUM_SLOTS;
                }
            }
        }

        /* Populate the Instance data */
        if (GCQ_ERRORS_NONE == xStatus) {
            uint64_t ullCQProduced;
            uint64_t ullSQProduced;
            GCQInstance *pxGCQInstance = *ppxGCQInstance;

            pxGCQInstance->ulProducerSlotSize = ulSQSlotSize;
            pxGCQInstance->ulConsumerSlotSize = ulCQSlotSize;

            /* In consumer mode we produce onto the SQ and consume from the CQ */
            pxGCQInstance->pxGCQProducer = &pxGCQInstance->xGCQSq;
            pxGCQInstance->pxGCQConsumer = &pxGCQInstance->xGCQCq;

            /* Set the producer address based on consumer GCQ memory map */
            ullSQProduced = ullBaseAddr + GCQ_CONSUMER_SQ_TAIL_POINTER;
            ullCQProduced = ullBaseAddr + GCQ_CONSUMER_CQ_TAIL_POINTER;

            /* Init SQ & CQ rings */
            GCQ_DEBUG("sGCQ Init Ring SQ\r\n");
            prvvGCQInitRing(pxGCQInstance,
                &pxGCQInstance->xGCQSq,
                ullSQProduced,
                ullRingAddr + offsetof(GCQHeader, ulHdrSQConsumed),
                ullRingAddr + sizeof(GCQHeader),
                ullNumSlots,
                ulSQSlotSize);
            GCQ_DEBUG("sGCQ Init Ring CQ\r\n");
            prvvGCQInitRing(pxGCQInstance,
                &pxGCQInstance->xGCQCq,
                ullCQProduced,
                ullRingAddr + offsetof(GCQHeader, ulHdrCQConsumed),
                ullRingAddr + sizeof(GCQHeader) + ullNumSlots * ulSQSlotSize,
                ullNumSlots,
                ulCQSlotSize);

            pxGCQInstance->iInitialised     = false;
            pxGCQInstance->ullGCQHeaderAddr = ullRingAddr;
            pxGCQInstance->ullBaseAddr      = ullBaseAddr;
            pxGCQInstance->ullRingAddr      = ullRingAddr;
            pxGCQInstance->pxGCQIOAccess    = pxGCQIOAccess;
            pxGCQInstance->ulUpperFirewall  = GCQ_INSTANCE_UPPER_FIREWALL;
            pxGCQInstance->ulLowerFirewall  = GCQ_INSTANCE_LOWER_FIREWALL;

            /* Set flag to show initialisation complete */
            pxGCQInstance->iInitialised = true;
            pxThis->ucInstancesAllocated++;
        }
    }

    return xStatus;
}

/**
 * @brief    De-initialise a sGCQ driver instance
 */
GCQ_ERRORS_TYPE xGCQDeinit(GCQInstance *pxGCQInstance)
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_INVALID_ARG;

    if ((GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall) &&
        (GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall)) {
        xStatus = GCQ_ERRORS_NONE;

        if ((NULL == pxGCQInstance) ||
            (false == pxGCQInstance->iInitialised)) {
            xStatus = GCQ_ERRORS_INVALID_INSTANCE;
        } else {
            pxGCQInstance->iInitialised = false;
            pxThis->ucInstancesAllocated--;
        }
    }

    return xStatus;
}

/**
 * @brief    Attempt to attach to the consumer, needs to be called before
 *           data can be consumed.
 */
GCQ_ERRORS_TYPE xGCQAttachConsumer(GCQInstance *pxGCQInstance)
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_INVALID_ARG;

    GCQHeader xGCQHeader = { };

    if ((GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall) &&
        (GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall))  {
        xStatus = GCQ_ERRORS_NONE;

        if ((NULL == pxGCQInstance) ||
            (false == pxGCQInstance->iInitialised)) {
            xStatus = GCQ_ERRORS_INVALID_INSTANCE;

        } else {
            /* Copy header from the ring buffer */
            prvvGCQCopyFromRing(pxGCQInstance->pxGCQIOAccess,
                (uint32_t*)&xGCQHeader,
                pxGCQInstance->ullRingAddr,
                sizeof(uint32_t));

            /* Magic number must show up to confirm the header is fully initialized */
            if (xGCQHeader.ulHdrMagic != GCQ_ALLOC_MAGIC)
                xStatus = GCQ_ERRORS_CONSUMER_NOT_AVAILABLE;
        }

        /* Check the version within the header is as expected */
        if (GCQ_ERRORS_NONE == xStatus) {
            prvvGCQCopyFromRing(pxGCQInstance->pxGCQIOAccess,
                (uint32_t*)&xGCQHeader,
                pxGCQInstance->ullRingAddr,
                sizeof(GCQHeader));

            if (GCQ_VER_MAJOR != GET_GCQ_MAJOR(xGCQHeader.ulHdrVersion)) {
                GCQ_DEBUG("Error: Unexpected version:0x%lx in magic header!\r\n",
                    xGCQHeader.ulHdrVersion);
                xStatus = GCQ_ERRORS_INVALID_VERSION;
            } else {
                GCQ_DEBUG("Version: 0x%lx 0x%lx\n", GET_GCQ_MAJOR(xGCQHeader.ulHdrVersion),
                    GET_GCQ_MINOR(xGCQHeader.ulHdrVersion));
            }
        }

        /* Validate the number of slots matches */
        if (GCQ_ERRORS_NONE == xStatus) {
            uint32_t ullHdrNumSlots = xGCQHeader.ulHdrNumSlots;
            uint32_t ullNumSlots = pxGCQInstance->xGCQSq.ulRingNumSlots;

            if (ullNumSlots != ullHdrNumSlots) {
                GCQ_DEBUG("Error: Invalid number of slots:%ld found in magic header, expecting: %ld!!\r\n",
                    ullHdrNumSlots, ullNumSlots);
                xStatus = GCQ_ERRORS_INVALID_NUM_SLOTS;
            }
        }

        /* Validate the slot size matches */
        if (GCQ_ERRORS_NONE == xStatus) {
            uint32_t ulHdrSlotSize = xGCQHeader.ulHdrSQSlotSize;
            uint32_t ulRingSlotSize = pxGCQInstance->xGCQSq.ulRingSlotSize;
            if (ulRingSlotSize != ulHdrSlotSize) {
                GCQ_DEBUG("Error: Invalid slot size:%ld found in magic header, expecting: %ld!\r\n",
                    ulHdrSlotSize, ulRingSlotSize);
                xStatus = GCQ_ERRORS_INVALID_SLOT_SIZE;
            }
        }

        if (GCQ_ERRORS_NONE == xStatus) {
            prvvGCQFastForward(pxGCQInstance, &pxGCQInstance->xGCQSq);
            prvvGCQFastForward(pxGCQInstance, &pxGCQInstance->xGCQCq);

            /* Set flag to show now attached */
            pxThis->ucConsumerAttached = true;
        }
    }

    return xStatus;
}

/**
 * @brief    Function to consume data from the ring buffer
 */
GCQ_ERRORS_TYPE xGCQConsumeData(GCQInstance *pxGCQInstance, uint8_t *pucData, uint32_t ulDataLen)
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_INVALID_ARG;

    uint64_t ullSlotAddr = 0;

    if ((GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall) &&
        (GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall)) {
        xStatus = GCQ_ERRORS_NONE;

        if ((NULL == pxGCQInstance) ||
            (false == pxGCQInstance->iInitialised))
            xStatus = GCQ_ERRORS_INVALID_INSTANCE;

        if (false == pxThis->ucConsumerAttached)
            xStatus = GCQ_ERRORS_CONSUMER_NOT_ATTACHED;

        if (NULL == pucData)
            xStatus = GCQ_ERRORS_INVALID_ARG;

        if (!CHECK_32BIT_ALIGNMENT(ulDataLen)) {
            GCQ_DEBUG(" Error: length 0x%lx is not 32bit aligned\r\n", ulDataLen);
            xStatus = GCQ_ERRORS_INVALID_ARG;
        }

        if (ulDataLen > pxGCQInstance->ulConsumerSlotSize) {
            GCQ_DEBUG(" Error: length 0x%lx specified is larger than slot configured\r\n", ulDataLen);
            xStatus = GCQ_ERRORS_INVALID_ARG;
        }

        if (GCQ_ERRORS_NONE == xStatus) {
            /* Attempt to consume data if any is available */
            xStatus = prvxGCQConsume(pxGCQInstance, &ullSlotAddr);
        }

        if (GCQ_ERRORS_NONE == xStatus) {
            uint32_t offset;
            GCQRing *pxRing = pxGCQInstance->pxGCQConsumer;

            GCQ_DEBUG("Read data from slot addr:0x%llx len:%ld\r\n", ullSlotAddr, ulDataLen);

            /* Process the data & populate the return buffer */
            for(offset = 0; offset < ulDataLen; offset += 4) {
                *(uint32_t *)(pucData + offset) = pxGCQInstance->pxGCQIOAccess->xGCQReadMem32(ullSlotAddr + offset);
                GCQ_DEBUG("Read addr:0x%llx val:0x%lx\r\n", ullSlotAddr + offset, *(uint32_t*)(pucData + offset));
            }

            /* Notify the peer the data has been consumed */
            pxGCQInstance->pxGCQIOAccess->xGCQWriteMem32(pxRing->ullRingConsumedAddr, pxRing->ulRingConsumed);
        }
    }

    return xStatus;
}


/**
 * @brief    Function to provide data and populate the ring buffer
 */
GCQ_ERRORS_TYPE xGCQProduceData(GCQInstance *pxGCQInstance, uint8_t * pucData, uint32_t ulDataLen)
{
    GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_INVALID_ARG;

    uint64_t ullSlotAddr = 0;

    if ((GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall) &&
        (GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall)) {
        xStatus = GCQ_ERRORS_NONE;

        if ((NULL == pxGCQInstance) ||
            (false == pxGCQInstance->iInitialised))
            xStatus = GCQ_ERRORS_INVALID_INSTANCE;

        if (NULL == pucData)
            xStatus = GCQ_ERRORS_INVALID_ARG;

        if (!CHECK_32BIT_ALIGNMENT(ulDataLen)) {
            GCQ_DEBUG(" Error: length 0x%lx is not 32bit aligned\r\n", ulDataLen);
            xStatus = GCQ_ERRORS_INVALID_ARG;
        }

        if (ulDataLen > pxGCQInstance->ulProducerSlotSize) {
            GCQ_DEBUG(" Error: length 0x%lx specified is larger than slot configured\r\n", ulDataLen);
            xStatus = GCQ_ERRORS_INVALID_ARG;
        }

        if (GCQ_ERRORS_NONE == xStatus)
            xStatus = prvxGCQProduce(pxGCQInstance, &ullSlotAddr);

        if (GCQ_ERRORS_NONE == xStatus) {
            uint32_t offset;
            GCQRing *pxRing = pxGCQInstance->pxGCQProducer;
            GCQ_DEBUG("Write data to slot addr:0x%llx len:%ld\r\n", ullSlotAddr, ulDataLen);

            for(offset = 0; offset < ulDataLen; offset += 4) {
                pxGCQInstance->pxGCQIOAccess->xGCQWriteMem32(ullSlotAddr + offset, *(uint32_t *)(pucData + offset));
                GCQ_DEBUG("Write addr:0x%llx val:0x%lx\r\n", ullSlotAddr + offset, *(uint32_t *)(pucData + offset));
            }

            pxGCQInstance->pxGCQIOAccess->xGCQWriteMem32(pxRing->ullRingProducedAddr, pxRing->ulRingProduced);
        } else {
            GCQ_DEBUG("Error: Failed to add data into slot: %d\r\n", xStatus);
        }
    }

    return xStatus;
}

/**
 * @brief    Sets this modules version information
 */
int iGCQGetVersion(GCQVersion *pxVersion)
{
    int iStatus = GCQ_ERRORS_INVALID_ARG;

    if ((GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall) &&
        (GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall) &&
        (NULL != pxVersion)) {

        pxVersion->ucVerMajor   = GCQ_VER_MAJOR;
        pxVersion->ucVerMinor   = GCQ_VER_MINOR;
        pxVersion->ucVerPatch   = GCQ_VER_PATCH;
        pxVersion->ucDevCommits = GCQ_VER_DEV_COMMITS;

        iStatus = GCQ_ERRORS_NONE;
    }

    return iStatus;
}
