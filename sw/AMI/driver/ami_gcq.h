/**
 * Copyright (C) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This header file contains structures, type definitions and function declarations
 * for the sGCQ driver.
 *
 * @file ami_gcq.h
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

#define GCQ_UDID_LEN        ( 16 )

#define gcq_assert(x)                                                           \
do { if (x) break;                                                              \
        printk(KERN_EMERG "### ASSERTION FAILED [sGCQ Driver] %s: %s: %d: %s\n",\
               __FILE__, __func__, __LINE__, #x); dump_stack(); BUG();          \
} while ( 0 )


/******************************************************************************/
/* Structs                                                                    */
/******************************************************************************/


typedef struct
{
	uint8_t ucVerMajor;
	uint8_t ucVerMinor;
	uint8_t ucVerPatch;

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
	GCQ_ERRORS_NONE                      = 0,
	GCQ_ERRORS_DRIVER_NOT_INITIALISED    = 1,
	GCQ_ERRORS_DRIVER_IN_USE             = 2,
	GCQ_ERRORS_NO_FREE_INSTANCES	     = 3,
	GCQ_ERRORS_INVALID_INSTANCE          = 4,
	GCQ_ERRORS_INVALID_ARG               = 5,
	GCQ_ERRORS_INVALID_HANDLE            = 6,
	GCQ_ERRORS_INVALID_SLOT_SIZE         = 7,
	GCQ_ERRORS_INVALID_VERSION           = 8,
	GCQ_ERRORS_INVALID_NUM_SLOTS         = 9,
	GCQ_ERRORS_INVALID_PARAMS            = 10,
	GCQ_ERRORS_CONSUMER_NOT_ATTACHED     = 11,
	GCQ_ERRORS_CONSUMER_NOT_AVAILABLE    = 12,
	GCQ_ERRORS_CONSUMER_NO_DATA_RECEIVED = 13,
	GCQ_ERRORS_PRODUCER_NO_FREE_SLOTS    = 14,
	GCQ_ERRORS_NOT_SUPPORTED             = 15,

	MAX_GCQ_ERRORS_TYPE

} GCQ_ERRORS_TYPE;

/**
 * @struct  GCQCfg
 * @brief   config options for sGCQ interfaces (generic across all sGCQ interfaces)
 */
typedef struct GCQCfg
{
	uint64_t    ullBaseAddr;
	uint64_t    ullRingAddr;
	uint32_t    ulRingLength;
	uint32_t    ulCQSlotSize;
	uint32_t    ulSQSlotSize;
	uint8_t     udid[GCQ_UDID_LEN];

    void        *pvGCQInstance;  /* opaque handle to store internal context */

} GCQCfg;

/*****************************************************************************/
/* Public Functions                                                          */
/*****************************************************************************/

/**
 * @brief    Gets version information from gcq_version.h
 *
 * @return   OK                  Version set successfully
 *           ERROR               Version not set successfully
 */
int gcq_get_version(GCQVersion *pxVersion);

/**
 * @brief   initialisation function for sGCQ interfaces (generic across all sGCQ interfaces)
 *
 * @param   None
 *
 * @return  See GCQ_ERRORS
 */
uint32_t gcq_init(void);

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
	uint8_t *pucData, uint32_t *pulSize, uint32_t ulTimeoutMs);

/**
 * @brief   Local implementation of gcq_write
 */
uint32_t gcq_write(void *pvFWIf,
	uint8_t *pucData, uint32_t ulSize, uint32_t ulTimeoutMs);


#endif /* _GCQ_H_ */
