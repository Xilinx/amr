/**
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the user API definitions for the sGCQ.
 *
 * @file ami_gcq.c
 */

#include "ami_gcq.h"
#include <linux/delay.h>
#include <asm/io.h>


/*****************************************************************************/
/* Defines                                                                   */
/*****************************************************************************/

#define GCQ_INSTANCE_UPPER_FIREWALL	(0xBEEFCAFE)
#define GCQ_INSTANCE_LOWER_FIREWALL	(0xDEADFACE)

#define CHECK_32BIT_ALIGNMENT(x)	(0 == (x & 0x3))


#define GCQ_NAME                       "GCQ_NAME"

#define GCQ_ATTACH_MAX_ATTEMPTS         (30)  /* Roughly 30 seconds */
#define GCQ_ATTACH_RETRY_TIMEOUT_MS     (1000)

#define GCQ_VER_MAJOR                   (1)
#define GCQ_VER_MINOR                   (0)
#define GCQ_VER_PATCH                   (0)

#define GET_GCQ_MAJOR(version)          (version >> 16)
#define GET_GCQ_MINOR(version)          (version & 0xFFFF)

#define GCQ_ALLOC_MAGIC                 (0x5847513F)
#define GCQ_MIN_NUM_SLOTS               (2)
#ifndef GCQ_MAX_INSTANCES
#define GCQ_MAX_INSTANCES               (4)   /* Default value, but can be overridden by build environmental variable  */
#endif

/* Producer address offsets */
#define GCQ_PRODUCER_SQ_TAIL_POINTER    (0x0000)  /* RW */
#define GCQ_PRODUCER_SQ_MEM_ADDR_LOW    (0x0008)  /* RW */
#define GCQ_PRODUCER_SQ_MEM_ADDR_HIGH   (0x0010)  /* RW */
#define GCQ_PRODUCER_CQ_TAIL_POINTER    (0x0100)  /* RO */
#define GCQ_PRODUCER_CQ_MEM_ADDR_LOW    (0x0108)  /* RO */
#define GCQ_PRODUCER_CQ_MEM_ADDR_HIGH   (0x0110)  /* RO */

/* Consumer address offsets */
#define GCQ_CONSUMER_CQ_TAIL_POINTER    (0x0100)  /* RW */
#define GCQ_CONSUMER_CQ_MEM_ADDR_LOW    (0x0108)  /* RW */
#define GCQ_CONSUMER_CQ_MEM_ADDR_HIGH   (0x0110)  /* RW */
#define GCQ_CONSUMER_SQ_TAIL_POINTER    (0x0000)  /* RO */
#define GCQ_CONSUMER_SQ_MEM_ADDR_LOW    (0x0108)  /* RO */
#define GCQ_CONSUMER_SQ_MEM_ADDR_HIGH   (0x0110)  /* RO */

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

typedef struct GCQInstance GCQInstance;

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


/**
 * @struct  GCQInstance
 *
 * @brief   Instance of an GCQInstance, used to model the sGCQ.
 */
typedef struct GCQInstance
{
	uint32_t ulUpperFirewall;
	bool     iInitialised;
	uint64_t ullBaseAddr;
	uint64_t ullRingAddr;
	uint32_t ulConsumerSlotSize;
	uint32_t ulProducerSlotSize;
	GCQRing  xGCQSq; ____cacheline_aligned_in_smp
	GCQRing  xGCQCq; ____cacheline_aligned_in_smp
	GCQRing  *pxGCQProducer;
	GCQRing  *pxGCQConsumer;
	GCQ_STATE xState;
	uint32_t ulLowerFirewall;

} GCQInstance;


/*****************************************************************************/
/* Local Variables                                                           */
/*****************************************************************************/

static bool iInitialised = false;

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
	GCQInstance xGCQInstance;

	uint32_t    ulLowerFirewall;

} GCQPrivateData;


/*****************************************************************************/
/* Local Variables                                                           */
/*****************************************************************************/

static GCQPrivateData xLocalData =
{
	GCQ_INSTANCE_UPPER_FIREWALL,	/* ulUpperFirewall */

	false,		                   	/* ucConsumerAttached */
	{ 0 },                  		/* xGCQInstances */

	GCQ_INSTANCE_LOWER_FIREWALL  	/* ulLowerFirewall */
};
static GCQPrivateData *pxThis = &xLocalData;


/*****************************************************************************/
/* Private Functions                                                          */
/*****************************************************************************/

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
static inline void gcq_init_ring(const GCQInstance *pxGCQInstance,
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
	GCQ_DEBUG("Slot Size:%u\r\n", ulSlotSize);
	GCQ_DEBUG("Num Slots:%u\r\n", ulNumSlots);
}

/**
 * @brief    Checks if the ring buffer is full
 *
 * @pxRing: ring buffer instance
 *
 * @return   Returns 0 is buffer is not full
 */
static inline bool gcq_is_ring_full(const GCQRing *pxRing)
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
static inline bool gcq_is_ring_empty(const GCQRing *pxRing)
{
	gcq_assert(pxRing);
	return (pxRing->ulRingProduced == pxRing->ulRingConsumed);
}

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
static inline uint32_t gcq_get_max_slots(uint64_t ullRingLen,
	const uint32_t ulSQSlotSize, const uint32_t ulCQSlotSize)
{
	uint32_t ulTotalLen = 0;
	uint32_t ulNumSlots = 1;

	while (ulTotalLen <= ullRingLen) {
		/* Allocate an even number of slots starting at 2 */
		ulNumSlots <<= 1;

		/* The ring length is based on the ring header plus a number of SQ/CQ slots */
		ulTotalLen = sizeof(GCQHeader) + (ulNumSlots * (ulSQSlotSize + ulCQSlotSize));
	}

	return (ulNumSlots >> 1);
}

/**
 * @brief   Fast forward both the producer & consumer
 *
 * @param   pxRing the sq or cq ring buffer
 *
 * @return  N/A
 */
static inline void gcq_fast_forward(GCQRing *pxRing)
{
	gcq_assert(pxRing);

	pxRing->ulRingProduced = ioread32((void __iomem *)pxRing->ullRingProducedAddr);
	pxRing->ulRingConsumed = ioread32((void __iomem *)pxRing->ullRingConsumedAddr);
}

/**
 * @brief   Check if the producer has any more free slots
 *
 * @pxRing: the sq or cq ring buffer
 *
 * @return  returns true if can produce
 */
static inline bool gcq_can_produce(GCQRing *pxRing)
{
	bool ulStatus = false;

	if (NULL != pxRing) {
		if (false == likely(gcq_is_ring_full(pxRing))) {
			ulStatus = true;
		} else {
			pxRing->ulRingConsumed = ioread32((void __iomem *)pxRing->ullRingConsumedAddr);
			ulStatus = (false == gcq_is_ring_full(pxRing));
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
static inline bool gcq_can_consume(const GCQInstance *pxGCQInstance,
	GCQRing *pxRing)
{
	bool ulStatus = false;

	if ((NULL != pxGCQInstance) ||
		(NULL != pxRing))
	{
		/* Check for errors */
		uint32_t ulSqTailPointer = ioread32((void __iomem *)
			pxGCQInstance->ullBaseAddr + GCQ_PRODUCER_SQ_TAIL_POINTER);

		uint32_t ulCqTailPointer = ioread32((void __iomem *)
			pxGCQInstance->ullBaseAddr + GCQ_PRODUCER_CQ_TAIL_POINTER);

		if (unlikely(((uint32_t)-1 == ulSqTailPointer) &&
			((uint32_t)-1 == ulCqTailPointer))) {
			ulStatus = false;
		} else if (false == likely(gcq_is_ring_empty(pxRing))) {
			ulStatus = true;
		} else {
			pxRing->ulRingProduced = ioread32((void __iomem *)pxRing->ullRingProducedAddr);
			ulStatus = (false == gcq_is_ring_empty(pxRing));
		}
	}

	return ulStatus;
}

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
 * @param    ullBaseAddr is the base address of the sGCQ
 * @param    ullRingAddr is the base address of the shared memory for allocating slots
 * @param    ullRingLen is the length of the shared memory provided
 * @param    ulSQSlotSize is the required submission queue (SQ) slot size
 * @param    ulCQSlotSize is the required completion queue (CQ) slot size
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
static GCQ_ERRORS_TYPE gcq_initialise(GCQInstance **ppxGCQInstance,
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

		/* Find the next free instance */
		if (GCQ_ERRORS_NONE == xStatus) {
			xStatus = GCQ_ERRORS_NO_FREE_INSTANCES;

			/* Attempt to find an uninitialized sGCQ instance to use*/
			if (false == pxThis->xGCQInstance.iInitialised) {
				*ppxGCQInstance = &pxThis->xGCQInstance;
				xStatus = GCQ_ERRORS_NONE;
			}
		}

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
				ullNumSlots = gcq_get_max_slots(ullRingLen, ulSQSlotSize, ulCQSlotSize);
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
			gcq_init_ring(pxGCQInstance,
				&pxGCQInstance->xGCQSq,
				ullSQProduced,
				ullRingAddr + offsetof(GCQHeader, ulHdrSQConsumed),
				ullRingAddr + sizeof(GCQHeader),
				ullNumSlots,
				ulSQSlotSize);
			GCQ_DEBUG("sGCQ Init Ring CQ\r\n");
			gcq_init_ring(pxGCQInstance,
				&pxGCQInstance->xGCQCq,
				ullCQProduced,
				ullRingAddr + offsetof(GCQHeader, ulHdrCQConsumed),
				ullRingAddr + sizeof(GCQHeader) + ullNumSlots * ulSQSlotSize,
				ullNumSlots,
				ulCQSlotSize);

			pxGCQInstance->iInitialised    = false;
			pxGCQInstance->ullBaseAddr     = ullBaseAddr;
			pxGCQInstance->ullRingAddr     = ullRingAddr;
			pxGCQInstance->ulUpperFirewall = GCQ_INSTANCE_UPPER_FIREWALL;
			pxGCQInstance->ulLowerFirewall = GCQ_INSTANCE_LOWER_FIREWALL;

			/* Set flag to show initialisation complete */
			pxGCQInstance->iInitialised = true;
			pxGCQInstance->xState = GCQ_STATE_INIT;
		}
	}

	return xStatus;
}

/**
 * @brief    De-initialise a sGCQ driver instance
 *
 * @param    ppxGCQInstance is the instance to de-initialise
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
static GCQ_ERRORS_TYPE gcq_deinitialise(GCQInstance *pxGCQInstance)
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
			pxGCQInstance->xState = GCQ_STATE_CLOSED;
		}
	}

	return xStatus;
}

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
static GCQ_ERRORS_TYPE gcq_attach_consumer(GCQInstance *pxGCQInstance)
{
	GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_INVALID_ARG;
	GCQHeader xGCQHeader = {0};

	if ((GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall) &&
		(GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall))  {
		xStatus = GCQ_ERRORS_NONE;

		if ((NULL == pxGCQInstance) ||
			(false == pxGCQInstance->iInitialised)) {
			xStatus = GCQ_ERRORS_INVALID_INSTANCE;

		} else {
			/* Copy header from the ring buffer */
			xGCQHeader.ulHdrMagic = ioread32((void __iomem *)pxGCQInstance->ullRingAddr);
			/* Magic number must show up to confirm the header is fully initialized */
			if (xGCQHeader.ulHdrMagic != GCQ_ALLOC_MAGIC)
				xStatus = GCQ_ERRORS_CONSUMER_NOT_AVAILABLE;
		}

		/* Check the version within the header is as expected */
		if (GCQ_ERRORS_NONE == xStatus) {
			memcpy_fromio(&xGCQHeader, (void __iomem *)pxGCQInstance->ullRingAddr, sizeof(GCQHeader));

			if (GCQ_VER_MAJOR != GET_GCQ_MAJOR(xGCQHeader.ulHdrVersion)) {
				GCQ_DEBUG("Error: Unexpected version:0x%x in magic header!\r\n",
					xGCQHeader.ulHdrVersion);
				xStatus = GCQ_ERRORS_INVALID_VERSION;
			} else {
				GCQ_DEBUG("Version: 0x%x 0x%x\n", GET_GCQ_MAJOR(xGCQHeader.ulHdrVersion),
					GET_GCQ_MINOR(xGCQHeader.ulHdrVersion));
			}
		}

		/* Validate the number of slots matches */
		if (GCQ_ERRORS_NONE == xStatus) {
			uint32_t ullHdrNumSlots = xGCQHeader.ulHdrNumSlots;
			uint32_t ullNumSlots = pxGCQInstance->xGCQSq.ulRingNumSlots;

			if (ullNumSlots != ullHdrNumSlots) {
				GCQ_DEBUG("Error: Invalid number of slots:%u found in magic header, expecting: %u!!\r\n",
					ullHdrNumSlots, ullNumSlots);
				xStatus = GCQ_ERRORS_INVALID_NUM_SLOTS;
			}
		}

		/* Validate the slot size matches */
		if (GCQ_ERRORS_NONE == xStatus) {
			uint32_t ulHdrSlotSize = xGCQHeader.ulHdrSQSlotSize;
			uint32_t ulRingSlotSize = pxGCQInstance->xGCQSq.ulRingSlotSize;
			if (ulRingSlotSize != ulHdrSlotSize) {
				GCQ_DEBUG("Error: Invalid slot size:%u found in magic header, expecting: %u!\r\n",
					ulHdrSlotSize, ulRingSlotSize);
				xStatus = GCQ_ERRORS_INVALID_SLOT_SIZE;
			}
		}

		if (GCQ_ERRORS_NONE == xStatus) {
			gcq_fast_forward(&pxGCQInstance->xGCQSq);
			gcq_fast_forward(&pxGCQInstance->xGCQCq);

			/* Set flag to show now attached */
			pxThis->ucConsumerAttached = true;
			pxGCQInstance->xState = GCQ_STATE_ATTACHED;
			GCQ_DEBUG("Attached ok!\r\n");
		}
	}

	return xStatus;
}

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
static GCQ_ERRORS_TYPE xGCQConsumeData(GCQInstance *pxGCQInstance,
	uint8_t *pucData, uint32_t ulDataLen)
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
			GCQ_DEBUG(" Error: length 0x%x is not 32bit aligned\r\n", ulDataLen);
			xStatus = GCQ_ERRORS_INVALID_ARG;
		}

		if (ulDataLen > pxGCQInstance->ulConsumerSlotSize) {
			GCQ_DEBUG(" Error: length 0x%x specified is larger than slot configured\r\n", ulDataLen);
			xStatus = GCQ_ERRORS_INVALID_ARG;
		}

		if (GCQ_ERRORS_NONE == xStatus) {
			/* Bind into the correct ring */
			GCQRing *pxRing = pxGCQInstance->pxGCQConsumer;

			/* Attempt to consume data if any is available */
			if (true == likely(gcq_can_consume(pxGCQInstance, pxRing))) {
				/* Get consumer slot address */
				ullSlotAddr = pxRing->ullRingSlotAddr +
					(uint64_t)pxRing->ulRingSlotSize *
					(pxRing->ulRingConsumed & (pxRing->ulRingNumSlots - 1));
				pxRing->ulRingConsumed++;
			} else {
				xStatus = GCQ_ERRORS_CONSUMER_NO_DATA_RECEIVED;
			}
		}

		if (GCQ_ERRORS_NONE == xStatus) {
			GCQRing *pxRing = pxGCQInstance->pxGCQConsumer;
			GCQ_DEBUG("Read data from slot addr:0x%llx len:%u\r\n", ullSlotAddr, ulDataLen);
			/* Process the data & populate the return buffer */
			memcpy_fromio(pucData, (void __iomem *)ullSlotAddr, ulDataLen);

			/* Notify the peer the data has been consumed */
			iowrite32(pxRing->ulRingConsumed, (void __iomem *)pxRing->ullRingConsumedAddr);
		}
	}

	return xStatus;
}

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
static GCQ_ERRORS_TYPE gcq_produce_data(GCQInstance *pxGCQInstance,
	uint8_t * pucData, uint32_t ulDataLen)
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
			GCQ_DEBUG(" Error: length 0x%x is not 32bit aligned\r\n", ulDataLen);
			xStatus = GCQ_ERRORS_INVALID_ARG;
		}

		if (ulDataLen > pxGCQInstance->ulProducerSlotSize) {
			GCQ_DEBUG(" Error: length 0x%x specified is larger than slot configured\r\n", ulDataLen);
			xStatus = GCQ_ERRORS_INVALID_ARG;
		}

		if (GCQ_ERRORS_NONE == xStatus) {
			/* Bind into the correct ring buffer */
			GCQRing *pxRing = pxGCQInstance->pxGCQProducer;

			/* Check if there is a free slot */
			if (true == likely(gcq_can_produce(pxRing))) {
				/* Get producer slot address */
				ullSlotAddr = pxRing->ullRingSlotAddr +
							(uint64_t)pxRing->ulRingSlotSize *
							(pxRing->ulRingProduced & (pxRing->ulRingNumSlots - 1));
				pxRing->ulRingProduced++;
			} else {
				xStatus = GCQ_ERRORS_PRODUCER_NO_FREE_SLOTS;
			}
		}

		if (GCQ_ERRORS_NONE == xStatus) {
			GCQRing *pxRing = pxGCQInstance->pxGCQProducer;
			GCQ_DEBUG("Write data to slot addr:0x%llx len:%u\r\n", ullSlotAddr, ulDataLen);

			memcpy_toio((void __iomem *)ullSlotAddr, pucData, ulDataLen);
			iowrite32(pxRing->ulRingProduced, (void __iomem *)pxRing->ullRingProducedAddr);
		} else {
			GCQ_DEBUG("Error: Failed to add data into slot: %d\r\n", xStatus);
		}
	}

	return xStatus;
}

/*****************************************************************************/
/* Public Functions                                                          */
/*****************************************************************************/

/**
 * @brief   Local implementation of gcq_open
 */
uint32_t gcq_open(void *pvFWIf)
{
	GCQCfg *pxCfg = (GCQCfg*)pvFWIf;
	GCQ_ERRORS_TYPE xRet = MAX_GCQ_ERRORS_TYPE;
    GCQInstance *pxGCQInstance = NULL;

	if (NULL == pxCfg)
		return GCQ_ERRORS_INVALID_HANDLE;

	if (false == iInitialised)
		return GCQ_ERRORS_DRIVER_NOT_INITIALISED;

	/* Initially only interrupt polling mode supported */
	xRet = gcq_initialise(&pxGCQInstance,
			pxCfg->ullBaseAddr,
			pxCfg->ullRingAddr,
			pxCfg->ulRingLength,
			pxCfg->ulSQSlotSize,
			pxCfg->ulCQSlotSize);

	/* Update state & attach if in consumer mode */
	if (GCQ_ERRORS_NONE == xRet) {
		int iAttempts = 0;
		pxGCQInstance->xState = GCQ_STATE_OPENED;
		pxCfg->pvGCQInstance = pxGCQInstance;
		GCQ_DEBUG("GCQ_open (%d)\r\n", pxGCQInstance->xState);

		/* Consumer Mode:
		 * Sometimes (very rarely) the consumer is not yet ready when we reach this point.
		 * This can happen if a hot reset was performed and not enough time was given
		 * on the host before attempting to perform sGCQ setup. To mitigate this
		 * we need a retry mechanism here.
		 */
		while (1) {
			xRet = gcq_attach_consumer(pxGCQInstance);
			iAttempts++;
			if ((GCQ_ATTACH_MAX_ATTEMPTS <= iAttempts) ||
				(GCQ_ERRORS_NONE == xRet)) {
				break;
			}
			msleep(GCQ_ATTACH_RETRY_TIMEOUT_MS);
		}
		if (GCQ_ERRORS_NONE == xRet) {
			GCQ_DEBUG("Attached ok!\r\n");
		}
	}

	return xRet;
}

/**
 * @brief   Local implementation of gcq_close
 */
uint32_t gcq_close(void *pvFWIf)
{
	GCQCfg *pxCfg = (GCQCfg*)pvFWIf;
	GCQInstance *pxGCQInstance = NULL;
	GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_NONE;

	if (NULL == pxCfg)
		return GCQ_ERRORS_INVALID_HANDLE;

	if (false == iInitialised)
		return GCQ_ERRORS_DRIVER_NOT_INITIALISED;

	if (NULL == pxCfg->pvGCQInstance)
		return GCQ_ERRORS_INVALID_INSTANCE;

	pxGCQInstance = (GCQInstance*)pxCfg->pvGCQInstance;

	if ((GCQ_STATE_OPENED   != pxGCQInstance->xState) &&
		(GCQ_STATE_ATTACHED != pxGCQInstance->xState))
		return GCQ_ERRORS_NOT_SUPPORTED;

	xStatus = gcq_deinitialise(pxGCQInstance);

	return xStatus;
}

/**
 * @brief   Local implementation of gcq_read
 */
uint32_t gcq_read(void *pvFWIf,
	uint8_t *pucData, uint32_t *pulSize, uint32_t ulTimeoutMs)
{
	GCQCfg *pxCfg = (GCQCfg*)pvFWIf;
	GCQInstance *pxGCQInstance = NULL;
	GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_NONE;

	if (NULL == pxCfg)
		return GCQ_ERRORS_INVALID_HANDLE;

	if (false == iInitialised)
		return GCQ_ERRORS_DRIVER_NOT_INITIALISED;

	if (NULL == pulSize)
		return GCQ_ERRORS_INVALID_PARAMS;

	if (NULL == pucData)
		return GCQ_ERRORS_INVALID_PARAMS;

	if (NULL == pxCfg->pvGCQInstance)
		return GCQ_ERRORS_INVALID_HANDLE;

	pxGCQInstance = (GCQInstance*)pxCfg->pvGCQInstance;
	if ((GCQ_STATE_OPENED   != pxGCQInstance->xState) &&
		(GCQ_STATE_ATTACHED != pxGCQInstance->xState))
		return GCQ_ERRORS_INVALID_HANDLE;

	xStatus = xGCQConsumeData(pxGCQInstance, pucData, *pulSize);

	return xStatus;
}

/**
 * @brief   Local implementation of gcq_write
 */
uint32_t gcq_write(void *pvFWIf,
	uint8_t *pucData, uint32_t ulSize, uint32_t ulTimeoutMs)
{
	GCQCfg *pxCfg = (GCQCfg*)pvFWIf;
	GCQInstance *pxGCQInstance = NULL;
	GCQ_ERRORS_TYPE xStatus = GCQ_ERRORS_NONE;

	if (NULL == pxCfg)
	{
		GCQ_DEBUG("GCQ CFG is null\n");
		return GCQ_ERRORS_INVALID_HANDLE;
	}

	if (false == iInitialised)
	{
		GCQ_DEBUG("initialization null\n");
		return GCQ_ERRORS_DRIVER_NOT_INITIALISED;
	}

	if (NULL == pucData)
	{
		GCQ_DEBUG("pucData null\n");
		return GCQ_ERRORS_INVALID_PARAMS;
	}

	if (NULL == pxCfg->pvGCQInstance)
	{
		GCQ_DEBUG("GCQInstance null\n");
		return GCQ_ERRORS_INVALID_INSTANCE;
	}

	pxGCQInstance = (GCQInstance*)pxCfg->pvGCQInstance;
	if ((GCQ_STATE_OPENED   != pxGCQInstance->xState) &&
		(GCQ_STATE_ATTACHED != pxGCQInstance->xState))
	{
		GCQ_DEBUG("invalid state\n");
		return GCQ_ERRORS_NOT_SUPPORTED;
	}

	xStatus = gcq_produce_data(pxGCQInstance, pucData, ulSize);

	return xStatus;
}

/**
 * @brief   initialisation function for sGCQ interfaces (generic across all sGCQ interfaces)
 */
uint32_t gcq_init(void)
{
	GCQ_ERRORS_TYPE xRet = GCQ_ERRORS_NONE;

	if (true == iInitialised) {
		xRet = GCQ_ERRORS_DRIVER_IN_USE;
	} else {
		/*
		 * Bind in register and memory R/W function pointers
		 * and assign to the local instance to be used by all
		 * sGCQ instances
		 */
		iInitialised = true;

		GCQ_DEBUG("gcq_init\r\n");
	}
	return xRet;
}

/**
 * @brief    Sets this modules version information
 */
int gcq_get_version(GCQVersion *pxVersion)
{
	int iStatus = GCQ_ERRORS_INVALID_ARG;

	if ((GCQ_INSTANCE_UPPER_FIREWALL == pxThis->ulUpperFirewall) &&
		(GCQ_INSTANCE_LOWER_FIREWALL == pxThis->ulLowerFirewall) &&
		(NULL != pxVersion)) {

		pxVersion->ucVerMajor   = GCQ_VER_MAJOR;
		pxVersion->ucVerMinor   = GCQ_VER_MINOR;
		pxVersion->ucVerPatch   = GCQ_VER_PATCH;

		iStatus = GCQ_ERRORS_NONE;
	}

	return iStatus;
}
