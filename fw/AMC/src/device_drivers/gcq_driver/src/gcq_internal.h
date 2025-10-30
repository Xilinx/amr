/**
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This header file contains private definitions for the sGCQ driver.
 *
 * @file gcq_internal.h
 */

#ifndef _GCQ_INTERNAL_H_
#define _GCQ_INTERNAL_H_

#include "gcq.h"
#include "gcq_debug.h"
#include "gcq_ring.h"


/*****************************************************************************/
/* Defines                                                                   */
/*****************************************************************************/

#define GCQ_VER_MAJOR                   (1)
#define GCQ_VER_MINOR                   (0)
#define GCQ_VER_PATCH                   (0)
#define GCQ_VER_DEV_COMMITS             (0)

#define GET_GCQ_MAJOR( version )        ( version >> 16 )
#define GET_GCQ_MINOR( version )        ( version & 0xFFFF )

#define GCQ_ALLOC_MAGIC                 ( 0x5847513F )
#define GCQ_MIN_NUM_SLOTS               ( 2 )

/* Producer address offsets */
#define GCQ_PRODUCER_SQ_TAIL_POINTER    ( 0x0000 )  /* RW */
#define GCQ_PRODUCER_SQ_MEM_ADDR_LOW    ( 0x0008 )  /* RW */
#define GCQ_PRODUCER_SQ_MEM_ADDR_HIGH   ( 0x0010 )  /* RW */
#define GCQ_PRODUCER_CQ_TAIL_POINTER    ( 0x0100 )  /* RO */


/* Consumer address offsets */
#define GCQ_CONSUMER_CQ_TAIL_POINTER    ( 0x0000 )  /* RW */
#define GCQ_CONSUMER_CQ_MEM_ADDR_LOW    ( 0x0008 )  /* RW */
#define GCQ_CONSUMER_CQ_MEM_ADDR_HIGH   ( 0x0010 )  /* RW */
#define GCQ_CONSUMER_SQ_TAIL_POINTER    ( 0x0100 )  /* RO */


/******************************************************************************/
/* Structs                                                                    */
/******************************************************************************/

/**
 * @struct  GCQInstance
 *
 * @brief   Instance of an GCQInstance, used to model the sGCQ.
 */
typedef struct GCQInstance
{
    uint32_t        ulUpperFirewall;
    int             iInitialised;
    uint64_t        ullBaseAddr;
    GCQ_MODE_TYPE   xMode;
    const GCQIOAccess *pxGCQIOAccess;
    uint64_t        ullRingAddr;
    uint32_t        ulConsumerSlotSize;
    uint32_t        ulProducerSlotSize;
    uint64_t        ullGCQHeaderAddr;
    GCQRing         xGCQSq;
    GCQRing         xGCQCq;
    GCQRing         *pxGCQProducer;
    GCQRing         *pxGCQConsumer;
    uint32_t        ulLowerFirewall;

} GCQInstance;


#endif /* _GCQ_INTERNAL_H_ */
