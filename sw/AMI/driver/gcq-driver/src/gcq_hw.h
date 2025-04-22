/**
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains API definitions for HW accesses to sGCQ IP.
 *
 * @file gcq_hw.h
 */

#ifndef _GCQ_HW_H_
#define _GCQ_HW_H_


/******************************************************************************/
/* Function Declarations                                                      */
/******************************************************************************/

/**
 * @brief    Initial sGCQ mode configuration
 *
 * @xMode: sGCQ supported mode, consumer or producer
 * @ullBaseAddr: sGCQ is the base address of the
 * @ ullRingAddr: the queue memory base address
 * @pxGCQIOAccess: memory/register access functions
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
GCQ_ERRORS_TYPE xGCQHWInit(GCQ_MODE_TYPE xMode,
                           uint64_t ullBaseAddr,
                           uint64_t ullRingAddr,
                           const GCQ_IO_ACCESS_TYPE *pxGCQIOAccess);

/**
 * @brief    Configure the sGCQ interrupt mode
 *
 * @xMode: sGCQ supported mode, consumer or producer
 * @xIntMode: sGCQ is the supported interrupt mode
 * @ullBaseAddr: sGCQ is the base address of the
 * @pxGCQIOAccess: memory/register access functions
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
GCQ_ERRORS_TYPE xGCQHWConfigureInterruptMode(GCQ_MODE_TYPE xMode,
                                             GCQ_INTERRUPT_MODE_TYPE xIntMode,
                                             uint64_t ullBaseAddr,
                                             const GCQ_IO_ACCESS_TYPE *pxGCQIOAccess);

/**
 * @brief    Trigger an interrupt via the interrupt register
 *
 * @xMode: sGCQ supported mode, consumer or producer
 * @ullBaseAddr: sGCQ is the base address of the
 * @pxGCQIOAccess: memory/register access functions
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
GCQ_ERRORS_TYPE xGCQHWTriggerInterrupt(GCQ_MODE_TYPE xMode,
                                       uint64_t ullBaseAddr,
                                       const GCQ_IO_ACCESS_TYPE *pxGCQIOAccess);

/**
 * @brief    Clear the interrupt via the interrupt status register
 *
 * @xMode: sGCQ supported mode, consumer or producer
 * @ullBaseAddr: sGCQ is the base address of the
 * @pxGCQIOAccess: memory/register access functions
 *
 * @return   See GCQ_ERRORS_TYPE for possible return values
 */
GCQ_ERRORS_TYPE xGCQHWClearInterrupt(GCQ_MODE_TYPE xMode,
                                     uint64_t ullBaseAddr,
                                     const GCQ_IO_ACCESS_TYPE *pxGCQIOAccess);

#endif /* _GCQ_HW_H_ */
