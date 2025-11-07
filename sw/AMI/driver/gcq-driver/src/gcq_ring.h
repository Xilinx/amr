/**
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This header file contains definitions for the internal ring buffer implementation.
 *
 * @file gcq_ring.h
 */

#ifndef _GCQ_RING_H_
#define _GCQ_RING_H_


/******************************************************************************/
/* Structs                                                                    */
/******************************************************************************/

/**
 * @struct  GCQHeader
 *
 * @brief   The internal header format which is sent by the producer first
 *          to allow syncing with the consumer. Note the format is kept
 *          same as original implementation to allow backwards compatability.
 */
typedef struct
{
    uint32_t ulHdrMagic; /* Always the first member */
    uint32_t ulHdrVersion;

    /* SQ and CQ share the same num of slots. */
    uint32_t ulHdrNumSlots;
    uint32_t ulHdrSQOffset;
    uint32_t ulHdrSQSlotSize;
    uint32_t ulHdrCQOffset;
    /* CQ slot size and format is tied to sGCQ version. */

    /*
     * Consumed pointer for both SQ and CQ are here since they don't generate
     * interrupt,
     */
    uint32_t ulHdrSQConsumed;
    uint32_t ulHdrCQConsumed;
    uint32_t ulHdrFlags;

    /*
     * In-mem version to communicate b/w the peers.
     */
    uint32_t ulHdrSQProduced;
    uint32_t ulHdrCQProduced;
} GCQHeader;

/**
 * @struct  GCQRing
 *
 * @brief   Instance of an GCQRing, used to model the ring buffer
 */
typedef struct
{
    GCQInstance *pxGCQInstance; /* pointing back to parent q */
    uint32_t    ulRingNumSlots;
    uint32_t    ulRingSlotSize;
    uint32_t    ulRingProduced;
    uint32_t    ulRingConsumed;
    uint64_t    ullRingProducedAddr;
    uint64_t    ullRingConsumedAddr;
    uint64_t    ullRingSlotAddr;
} GCQRing;


/******************************************************************************/
/* Static Function Declarations                                               */
/******************************************************************************/

/**
 * @brief    Initialise an instance of the ring buffer
 *
 * @pxGCQInstance: pointer to the parent sGCQ instance
 * @pxRing: ring buffer instance to be populated
 * @ullConsumedAddr: consumer address
 * @ullProducedAddr: producer address
 * @ullSlotAddr: slot address
 * @ulNumSlots: number of slots
 * @ulSlotSize: slot size
 *
 * @return   N/A
 */
static inline void prvvGCQInitRing(const GCQInstance *pxGCQInstance,
                                   GCQRing *pxRing,
                                   uint64_t ullProducedAddr,
                                   uint64_t ullConsumedAddr,
                                   uint64_t ullSlotAddr,
                                   uint32_t ulNumSlots,
                                   uint32_t ulSlotSize)
{
    gcq_assert(pxGCQInstance);
    gcq_assert(pxRing);

    pxRing->pxGCQInstance = (GCQInstance*)&pxGCQInstance;
    pxRing->ullRingProducedAddr = ullProducedAddr;
    pxRing->ullRingConsumedAddr = ullConsumedAddr;
    pxRing->ullRingSlotAddr = ullSlotAddr;
    pxRing->ulRingSlotSize = ulSlotSize;
    pxRing->ulRingNumSlots = ulNumSlots;
    pxRing->ulRingProduced = pxRing->ulRingConsumed = 0;

    GCQ_DEBUG("Produced Tail:0x%llx\r\n", ullProducedAddr);
    GCQ_DEBUG("Hdr Consumed Tail:0x%llx\r\n", ullConsumedAddr);
    GCQ_DEBUG("Slot Addr:0x%llx\r\n", ullSlotAddr);
    GCQ_DEBUG("Slot Size:%ld\r\n", ulSlotSize);
    GCQ_DEBUG("Num Slots:%ld\r\n", ulNumSlots);
}

/**
 * @brief    Calculate and return the size of the ring buffer being used
 *
 * @ulNumSlots: number of slots
 * @ulSQSlotSize: SQ slot size
 * @ulCQSlotSize: CQ slot size
 *
 * @return   The calculated ring length based on number of slots & CQ/SQ slot size
 */
static inline uint32_t prvulGCQRingLen(uint32_t ulNumSlots,
            uint32_t ulSQSlotSize, uint32_t ulCQSlotSize)
{
    return (sizeof(GCQHeader) + (ulNumSlots * (ulSQSlotSize + ulCQSlotSize)));
}

/**
 * @brief    Copy data to the ring buffer
 *
 * @pxGCQIOAccess: memory access functions
 * @pucBuffer: byte buffer containing the data to write
 * @ullDestAddr: destination address within the ring buffer
 * @ulLen: length of data to write
 *
 * @return   N/A
 *
 * @note     Ring buffer accesses are memory only
 */
static inline void prvvGCQCopyToRing(const GCQIOAccess *pxGCQIOAccess,
    uint32_t *pucBuffer, uint64_t ullDestAddr, uint32_t ulLen)
{
    uint32_t i;

    gcq_assert(pxGCQIOAccess);
    gcq_assert(pucBuffer);

    ulLen /= 4;

    for(i = 0; i < ulLen; i++, ullDestAddr += 4) {
        pxGCQIOAccess->xGCQWriteMem32(ullDestAddr, pucBuffer[i]);
    }
}

/**
 * @brief    Copy data from the ring buffer
 *
 * @pxGCQIOAccess: memory access functions
 * @pucBuffer: byte buffer to be populated
 * @ullSrcAddr: src address within the ring buffer
 * @ulLen is the length of data to be read
 *
 * @return   N/A
 *
 * @note     Ring buffer accesses are memory only
 */
static inline void prvvGCQCopyFromRing(const GCQIOAccess *pxGCQIOAccess,
	uint32_t *pucBuffer, uint64_t ullSrcAddr, uint32_t ulLen)
{
    uint32_t i;

    gcq_assert(pxGCQIOAccess);
    gcq_assert(pucBuffer);

    ulLen /= 4;
    for (i = 0; i < ulLen; i++, ullSrcAddr += 4) {
        pucBuffer[i] = pxGCQIOAccess->xGCQReadMem32(ullSrcAddr);
    }
}

/**
 * @brief    Checks if the ring buffer is full
 *
 * @pxRing: ring buffer instance
 *
 * @return   Returns 0 is buffer is not full
 */
static inline bool prvucGCQRingIsFull(const GCQRing *pxRing)
{
    gcq_assert(pxRing);
    return ((pxRing->ulRingProduced - pxRing->ulRingConsumed) >= pxRing->ulRingNumSlots);
}

/**
 * @brief    Checks if the ring buffer is empty
 *
 * @pxRing: ring buffer instance
 *
 * @return   Returns 0 is buffer is empty
 */
static inline bool prvucGCQRingIsEmpty(const GCQRing *pxRing)
{
    gcq_assert(pxRing);
    return (pxRing->ulRingProduced == pxRing->ulRingConsumed);
}

/**
 * @brief    Returns the current producer slot address
 *
 * @pxRing: ring buffer instance
 *
 * @return   The slot address
 */
static inline uint64_t prvullGCQRingGetSlotPtrProduced(const GCQRing *pxRing)
{
    gcq_assert(pxRing);
    return (pxRing->ullRingSlotAddr +
            (uint64_t)pxRing->ulRingSlotSize *
			(pxRing->ulRingProduced & (pxRing->ulRingNumSlots - 1)));
}

/**
 * @brief    Returns the current consumer slot address
 *
 * @pxRing: ring buffer instance
 *
 * @return   The slot address
 */
static inline uint64_t prvullGCQRingGetSlotPtrConsumed(const GCQRing *pxRing)
{
    gcq_assert(pxRing);
    return (pxRing->ullRingSlotAddr +
           (uint64_t)pxRing->ulRingSlotSize *
		   (pxRing->ulRingConsumed & (pxRing->ulRingNumSlots - 1)));
}

#endif /* _GCQ_RING_H_ */
