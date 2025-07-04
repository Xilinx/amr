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
 * @struct  GCQ_HEADER_TYPE
 *
 * @brief   The internal header format which is sent by the producer first
 *          to allow syncing with the consumer. Note the format is kept the
 *          same as original implementation to allow backwards compatability.
 */
struct GCQ_HEADER_TYPE
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
     * Consumed pointer for both SQ and CQ are here since they don't generate interrupt,
     * so no need to be a register.
     */
    uint32_t ulHdrSQConsumed;
    uint32_t ulHdrCQConsumed;
    uint32_t ulHdrFlags;

    /*
     * On some platforms, there is no dedicated producer pointer register. We can use
     * below in-mem version to communicate b/w the peers.
     */
    uint32_t ulHdrSQProduced;
    uint32_t ulHdrCQProduced;
};

/**
 * @struct  GCQ_RING_TYPE
 *
 * @brief   Instance of an GCQ_RING_TYPE, used to model the ring buffer
 */
struct GCQ_RING_TYPE
{
    struct GCQ_INSTANCE_TYPE *pxGCQInstance; /* pointing back to parent q */
    uint32_t ulRingNumSlots;
    uint32_t ulRingSlotSize;
    uint32_t ulRingProduced;
    uint32_t ulRingConsumed;
    uint64_t ullRingProducedAddr;
    uint64_t ullRingConsumedAddr;
    uint64_t ullRingSlotAddr;
};


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
static inline void prvvGCQInitRing(const struct GCQ_INSTANCE_TYPE *pxGCQInstance,
                                   struct GCQ_RING_TYPE *pxRing,
                                   uint64_t ullProducedAddr,
                                   uint64_t ullConsumedAddr,
                                   uint64_t ullSlotAddr,
                                   uint32_t ulNumSlots,
                                   uint32_t ulSlotSize)
{
    gcq_assert(pxGCQInstance);
    gcq_assert(pxRing);

    pxRing->pxGCQInstance = (struct GCQ_INSTANCE_TYPE*)&pxGCQInstance;
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
			uint32_t ulSQSlotSize,
			uint32_t ulCQSlotSize)
{
    return (sizeof(struct GCQ_HEADER_TYPE) + (ulNumSlots * (ulSQSlotSize + ulCQSlotSize)));
}

/**
 * @brief    Copy data to the ring buffer
 *
 * @pxGCQIOAccess: memory/register access functions
 * @pucBuffer: byte buffer containing the data to write
 * @ullDestAddr: destination address within the ring buffer
 * @ulLen: length of data to write
 *
 * @return   N/A
 *
 * @note     Ring buffer accesses are memory only
 */
static inline void prvvGCQCopyToRing(const GCQ_IO_ACCESS_TYPE *pxGCQIOAccess,
	uint8_t *pucBuffer,
	uint64_t ullDestAddr,
	uint32_t ulLen)
{
    int i = 0;
    uint32_t *ulSrcAddr = (uint32_t *)pucBuffer;

	gcq_assert(pxGCQIOAccess);
    gcq_assert(pucBuffer);

    for(i = 0; i < (ulLen / 4); i++, (ullDestAddr += 4)) {
        pxGCQIOAccess->xGCQWriteMem32(ullDestAddr, ulSrcAddr[ i ]);
    }
}

/**
 * @brief    Copy data from the ring buffer
 *
 * @pxGCQIOAccess: memory/register access functions
 * @pucBuffer: byte buffer to be populated
 * @ullSrcAddr: src address within the ring buffer
 * @ulLen is the length of data to be read
 *
 * @return   N/A
 *
 * @note     Ring buffer accesses are memory only
 */
static inline void prvvGCQCopyFromRing(const GCQ_IO_ACCESS_TYPE *pxGCQIOAccess,
	uint8_t *pucBuffer,
	uint64_t ullSrcAddr, uint32_t ulLen)
{
    uint32_t i = 0;
    uint32_t *ullDestAddr = (uint32_t *)pucBuffer;

    gcq_assert(pxGCQIOAccess);
    gcq_assert(pucBuffer);

    for (i = 0; i < (ulLen / 4); i++, ullSrcAddr += 4) {
        ullDestAddr[ i ] = pxGCQIOAccess->xGCQReadMem32(ullSrcAddr);
    }
}

/**
 * @brief    Read a value from the producer tail pointer register
 *
 * @pxGCQIOAccess: memory/register access functions
 * @xGCQFlags: supported driver flags
 * @pxRing: ring buffer instance
 *
 * @return   N/A
 *
 * @note     Supports double read and in memory feature flags
 */
static inline void prvvGCQRingReadProduced(const GCQ_IO_ACCESS_TYPE *pxGCQIOAccess,
										GCQ_FLAGS_TYPE xGCQFlags,
										struct GCQ_RING_TYPE *pxRing)
{
    gcq_assert(pxGCQIOAccess);

    if(unlikely(GCQ_FEATURES_NEED_DOUBLE_MEM_READ(xGCQFlags))) {
        pxRing->ulRingProduced =
			ulGCQFeaturesHandleDoubleRead32(pxGCQIOAccess, pxRing->ullRingProducedAddr);
    } else {
        pxRing->ulRingProduced =
			(GCQ_FEATURES_IS_IN_MEM_PTR(xGCQFlags) ?
            pxGCQIOAccess->xGCQReadMem32(pxRing->ullRingProducedAddr) :
            pxGCQIOAccess->xGCQReadReg32(pxRing->ullRingProducedAddr));
    }
}

/**
 * @brief    Write a value to the producer tail pointer register
 *
 * @pxGCQIOAccess: memory/register access functions
 * @xGCQFlags: supported driver flags
 * @pxRing: ring buffer instance
 *
 * @return   N/A
 *
 * @note     Supports in memory feature flags
 */
static inline void prvvGCQRingWriteProduced(const GCQ_IO_ACCESS_TYPE *pxGCQIOAccess,
											GCQ_FLAGS_TYPE xGCQFlags,
											const struct GCQ_RING_TYPE *pxRing)
{
    gcq_assert(pxGCQIOAccess);
    gcq_assert(pxRing);

    (GCQ_FEATURES_IS_IN_MEM_PTR(xGCQFlags) ?
		pxGCQIOAccess->xGCQWriteMem32(pxRing->ullRingProducedAddr, pxRing->ulRingProduced) :
        pxGCQIOAccess->xGCQWriteReg32(pxRing->ullRingProducedAddr, pxRing->ulRingProduced));
}

/**
 * @brief    Read a value from the consumer tail pointer register
 *
 * @pxGCQIOAccess: memory/register access functions
 * @pxRing: ring buffer instance
 *
 * @return   N/A
 *
 * @note     Supports double read and in memory feature flags
 */
static inline void prvvGCQRingReadConsumed(const GCQ_IO_ACCESS_TYPE *pxGCQIOAccess,
										GCQ_FLAGS_TYPE xGCQFlags,
										struct GCQ_RING_TYPE *pxRing)
{
    gcq_assert(pxGCQIOAccess);
    gcq_assert(pxRing);

    if(unlikely(GCQ_FEATURES_NEED_DOUBLE_MEM_READ(xGCQFlags))) {
        pxRing->ulRingConsumed =
			ulGCQFeaturesHandleDoubleRead32(pxGCQIOAccess, pxRing->ullRingConsumedAddr);
    } else {
        pxRing->ulRingConsumed =
			(GCQ_FEATURES_IS_IN_MEM_PTR(xGCQFlags) ?
            	pxGCQIOAccess->xGCQReadMem32(pxRing->ullRingConsumedAddr) :
                pxGCQIOAccess->xGCQReadReg32(pxRing->ullRingConsumedAddr));
    }
}

/**
 * @brief    Write a value from the consumer tail pointer register
 *
 * @pxGCQIOAccess: memory/register access functions
 * @xGCQFlags: supported driver flags
 * @pxRing: ring buffer instance
 *
 * @return   N/A
 *
 * @note     Supports in memory feature flags
 */
static inline void prvvGCQRingWriteConsumed(const GCQ_IO_ACCESS_TYPE *pxGCQIOAccess, GCQ_FLAGS_TYPE xGCQFlags, const struct GCQ_RING_TYPE *pxRing)
{
    gcq_assert(pxGCQIOAccess);
    gcq_assert(pxRing);

    (GCQ_FEATURES_IS_IN_MEM_PTR(xGCQFlags) ? pxGCQIOAccess->xGCQWriteMem32(pxRing->ullRingConsumedAddr, pxRing->ulRingConsumed) :
                                                pxGCQIOAccess->xGCQWriteReg32(pxRing->ullRingConsumedAddr, pxRing->ulRingConsumed));
}

/**
 * @brief    Checks if the ring buffer is full
 *
 * @pxRing: ring buffer instance
 *
 * @return   Returns 0 is buffer is not full
 */
static inline bool prvucGCQRingIsFull(const struct GCQ_RING_TYPE *pxRing)
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
static inline bool prvucGCQRingIsEmpty(const struct GCQ_RING_TYPE *pxRing)
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
static inline uint64_t prvullGCQRingGetSlotPtrProduced(const struct GCQ_RING_TYPE *pxRing)
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
static inline uint64_t prvullGCQRingGetSlotPtrConsumed(const struct GCQ_RING_TYPE *pxRing)
{
    gcq_assert(pxRing);
    return (pxRing->ullRingSlotAddr +
           (uint64_t)pxRing->ulRingSlotSize *
		   (pxRing->ulRingConsumed & (pxRing->ulRingNumSlots - 1)));
}

#endif /* _GCQ_RING_H_ */
