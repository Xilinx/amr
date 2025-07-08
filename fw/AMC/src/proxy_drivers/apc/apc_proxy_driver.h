/**
 * Copyright (c) 2024 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the API for the Alveo Programming Control (APC) proxy driver
 *
 * @file apc_proxy_driver.h
 */

#ifndef _APC_PROXY_DRIVER_H_
#define _APC_PROXY_DRIVER_H_

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include "standard.h"
#include "evl.h"
#include "fw_if.h"
#include "xloader_client.h"


/******************************************************************************/
/* Enums                                                                      */
/******************************************************************************/

/**
 * @enum    APC_PROXY_DRIVER_EVENTS
 * @brief   Events raised by this proxy driver
 */
typedef enum APC_PROXY_DRIVER_EVENTS
{
    APC_PROXY_DRIVER_E_DOWNLOAD_STARTED = 0,
    APC_PROXY_DRIVER_E_DOWNLOAD_COMPLETE,
    APC_PROXY_DRIVER_E_DOWNLOAD_BUSY,
    APC_PROXY_DRIVER_E_DOWNLOAD_FAILED,
    APC_PROXY_DRIVER_E_FPT_UPDATE,
    APC_PROXY_DRIVER_E_COPY_STARTED,
    APC_PROXY_DRIVER_E_COPY_COMPLETE,
    APC_PROXY_DRIVER_E_COPY_BUSY,
    APC_PROXY_DRIVER_E_COPY_FAILED,
    APC_PROXY_DRIVER_E_PARTITION_SELECTED,
    APC_PROXY_DRIVER_E_PARTITION_SELECTION_FAILED,
    APC_PROXY_DRIVER_E_PROGRAM_STARTED,
    APC_PROXY_DRIVER_E_PROGRAM_COMPLETE,
    APC_PROXY_DRIVER_E_PROGRAM_BUSY,
    APC_PROXY_DRIVER_E_PROGRAM_FAILED,
    MAX_APC_PROXY_DRIVER_EVENTS

} APC_PROXY_DRIVER_EVENTS;

/**
 * @enum    APC_BOOT_DEVICES
 * @brief   Enumeration of boot devices available
 */
typedef enum
{
    APC_BOOT_DEVICE_PRIMARY = 0,
    APC_BOOT_DEVICE_SECONDARY,

    MAX_APC_BOOT_DEVICES

} APC_BOOT_DEVICES;


/******************************************************************************/
/* Structs                                                                    */
/******************************************************************************/

/**
 * @struct  APC_PROXY_DRIVER_FPT_HEADER
 * @brief   Structure to hold the FPT Header
 */
typedef struct APC_PROXY_DRIVER_FPT_HEADER
{
    uint32_t    ulMagicNum;
    uint8_t     ucFptVersion;
    uint8_t     ucFptHeaderSize;
    uint8_t     ucEntrySize;
    uint8_t     ucNumEntries;

} APC_PROXY_DRIVER_FPT_HEADER;

/**
 * @struct  APC_PROXY_DRIVER_FPT_PARTITION
 * @brief   Structure to hold a single FPT partition
 */
typedef struct APC_PROXY_DRIVER_FPT_PARTITION
{
    uint32_t    ulPartitionType;
    uint32_t    ulPartitionBaseAddr;
    uint32_t    ulPartitionSize;

} APC_PROXY_DRIVER_FPT_PARTITION;


/******************************************************************************/
/* Function declarations                                                      */
/******************************************************************************/

/**
 * iAPC_Initialise() - Main initialisation point for the APC Proxy Driver
 *
 * @ucProxyId:       Unique ID for this Proxy driver
 * @pxPrimaryFwIf:   Handle to the primary Firmware Interface to use
 * @pxSecondaryFwIf: Handle to the secondary Firmware Interface to use
 * @pXLoaderInst:    Handle to the xilloader client interface
 * @ulTaskPrio:      Priority of the Proxy driver task (if RR disabled)
 * @ulTaskStack:     Stack size of the Proxy driver task
 *
 * @return  OK       Proxy driver initialised correctly
 *          ERROR    Proxy driver not initialised, or was already initialised
 *
 * @note    A Primary Firmware Interface handle must be passed to iAPC_Initialise,
 *          the secondary Firmware Interface handle however is optional
 *          and can be set to NULL.
 */
int iAPC_Initialise( uint8_t ucProxyId,
					 FW_IF_CFG *pxPrimaryFwIf,
					 FW_IF_CFG *pxSecondaryFwIf,
					 XLoader_ClientInstance *pXLoaderInst,
                     uint32_t ulTaskPrio, uint32_t ulTaskStack );

/**
 * iAPC_BindCallback() - Bind into this proxy driver
 *
 * pxCallback:  Callback to bind into the proxy driver
 *
 * @return  OK       Callback successfully bound
 *          ERROR    Callback not bound
 */
int iAPC_BindCallback( EVL_CALLBACK *pxCallback );

/**
 * iAPC_DownloadImage() - Download an image to a location in NV memory
 *
 * @pxSignal:     Current event occurance (used for tracking)
 * @xBootDevice:  Target boot device
 * @iPartition:   The partition in the FPT to store this image in
 * @ulSrcAddr:    Address (in RAM) to read the image from
 * @ulImageSize:  Size of image (in bytes)
 * @usPacketNum:  Image packet number
 * @usPacketSize: Size of image packet (in KB)
 *
 * @return  OK    Image downloaded successfully
 *          ERROR Image not downloaded successfully
 */
int iAPC_DownloadImage( EVL_SIGNAL *pxSignal,
						APC_BOOT_DEVICES xBootDevice,
						int iPartition,
						uint32_t ulSrcAddr,
						uint32_t ulImageSize,
						uint16_t usPacketNum,
						uint16_t usPacketSize );

/**
 * iAPC_UpdateFpt() - Download an image with an FPT to a location in NV memory
 *
 * @pxSignal:     Current event occurance (used for tracking)
 * @xBootDevice:  Target boot device
 * @ulSrcAddr:    Address (in RAM) to read the image from
 * @ulImageSize:  Size of image (in bytes)
 * @usPacketNum:  Image packet number
 * @usPacketSize: Size of image packet (in KB)
 * @iLastPacket:  Boolean indicating if this is the last data packet
 *
 * @return  OK    Image downloaded successfully
 *          ERROR Image not downloaded successfully
 */
int iAPC_UpdateFpt( EVL_SIGNAL *pxSignal,
					APC_BOOT_DEVICES xBootDevice,
					uint32_t ulSrcAddr,
					uint32_t ulImageSize,
					uint16_t usPacketNum,
					uint16_t usPacketSize,
					int iLastPacket );

/**
 * iAPC_CopyImage() - Copy an image from one partition to another
 *
 * @pxSignal:        Current event occurance (used for tracking)
 * @xSrcBootDevice:  Target boot device to copy from
 * @iSrcPartition:   The partition in the FPT to copy this image from
 * @xDestBootDevice: Target boot device to copy to
 * @iDestPartition:  The partition in the FPT to copy this image to
 * @ulCpyAddr:       Address (in RAM) to copy the source partition to before writing it
 * @ulAllocatedSize: Maximum size available to copy
 *
 * @return  OK       Image copied successfully
 *          ERROR    Image not copied successfully
 */
int iAPC_CopyImage( EVL_SIGNAL *pxSignal,
					APC_BOOT_DEVICES xSrcBootDevice,
					int iSrcPartition,
					APC_BOOT_DEVICES xDestBootDevice,
					int iDestPartition,
					uint32_t ulCpyAddr,
					uint32_t ulAllocatedSize );

/**
 * iAPC_PdiProgram() - Program PDI image to a location in memory
 *
 * @pxSignal:     Current event occurance (used for tracking)
 * @xBootDevice:  Target boot device
 * @iPartition:   The partition in the FPT to store this image in
 * @ulSrcAddr:    Address (in RAM) to read the image from
 * @ulImageSize:  Size of image (in bytes)
 * @ulLastPacket: Last packet
 * @usPacketNum:  Image packet number
 * @usPacketSize: Size of image packet (in KB)
 *
 * @return  OK    Image copied successfully
 *          ERROR Image not copied successfully
 */
int iAPC_PdiProgram( EVL_SIGNAL *pxSignal,
					APC_BOOT_DEVICES xBootDevice,
					int iPartition,
					uint32_t ulSrcAddr,
					uint32_t ulImageSize,
					 uint32_t ulLastPacket,
					uint16_t usPacketNum,
					uint16_t usPacketSize );
/**
 * iAPC_SetNextPartition() - Select which partition (from primary boot device) to boot from
 *
 * @pxSignal    Current event occurance (used for tracking)
 * @iPartition  The partition to boot from on the next reset
 *
 * @return  OK    Partition successfully selected
 *          ERROR Partition not selected
 */
int iAPC_SetNextPartition( EVL_SIGNAL *pxSignal, int iPartition );

/**
 * iAPC_EnableHotReset() - Enable the hot reset capability (from primary boot device)
 *
 * @pxSignal: Current event occurance (used for tracking)
 *
 * @return  OK    Hot reset successfully enabled
 *          ERROR Hot reset not enabled
 */
int iAPC_EnableHotReset( EVL_SIGNAL *pxSignal );

/**
 * iAPC_GetFptHeader() - Get the Flash Partition Table (FPT) Header
 *
 * @xBootDevice: Target boot device
 * @pxFptHeader: Pointer to the FPT header data
 *
 * @return  OK    FPT header retrieved successfully
 *          ERROR FPT header not retrieved successfully
 *
 */
int iAPC_GetFptHeader( APC_BOOT_DEVICES xBootDevice,
					APC_PROXY_DRIVER_FPT_HEADER *pxFptHeader );

/**
 * iAPC_GetFptPartition() - Get a Flash Partition Table (FPT) Partition
 *
 * @xBootDevice:    Target boot device
 * @iPartition:     Index of partition to retrieve (0 is the 1st partition)
 * @pxFptPartition: Pointer to the FPT partition data
 *
 * @return  OK      FPT partition retrieved successfully
 *          ERROR   FPT partition not retrieved successfully
 */
int iAPC_GetFptPartition( APC_BOOT_DEVICES xBootDevice, int iPartition,
						APC_PROXY_DRIVER_FPT_PARTITION *pxFptPartition );

/**
 * iAPC_PrintStatistics() - Print all the stats gathered by the proxy driver
 *
 * @return  OK    Stats retrieved from proxy driver successfully
 *          ERROR Stats not retrieved successfully
 *
 */
int iAPC_PrintStatistics( void );

/**
 * iAPC_ClearStatistics() - Clear all the stats in the proxy driver
 *
 * @return  OK    Stats cleared successfully
 *          ERROR Stats not cleared successfully
 */
int iAPC_ClearStatistics( void );

/**
 * iAPC_GetState() - Gets the current state of the proxy driver
 *
 * @pxState: Pointer to the state
 *
 * @return  OK    If successful
 *          ERROR If not successful
 */
int iAPC_GetState( MODULE_STATE *pxState );

#endif /* _APC_PROXY_DRIVER_H_ */
