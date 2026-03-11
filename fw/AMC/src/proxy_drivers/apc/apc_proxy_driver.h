/**
 * Copyright (c) 2024 - 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the API for the AMR Programming Control (APC) proxy driver
 *
 * @file apc_proxy_driver.h
 */

#ifndef _APC_PROXY_DRIVER_H_
#define _APC_PROXY_DRIVER_H_

#include "standard.h"
#include "evl.h"
#include "fw_if.h"
#include "xloader_client.h"

/* PDI Load on Power-up Configuration */
#define USER_PDI_POWERUP_BOOT_DEVICE   ( 0 ) /* 0 = Primary (OSPI), 1 = Secondary */
#define USER_PDI_POWERUP_PARTITION     ( 2 )
#define USER_PDI_POWERUP_FLAG          ( 1 << 0 )    /* User PDI power on load flag */
#define USER_PDI_POWERUP_NOT_LOADED    ( 0x0 << 16 ) /* Power on load user partition not loaded */
#define USER_PDI_POWERUP_LOADED        ( 0x1 << 16 ) /* Power on load user partition loaded */
#define USER_PDI_POWERUP_ERROR         ( 0x2 << 16 ) /* Power on load user partition error */

/******************************************************************************/
/* Enums                                                                      */
/******************************************************************************/

/**
 * @enum    APC_PROXY_DRIVER_EVENTS
 * @brief   Events raised by this proxy driver
 */
typedef enum
{
    APC_PROXY_DRIVER_E_DOWNLOAD_STARTED   = 0,
    APC_PROXY_DRIVER_E_DOWNLOAD_COMPLETE  = 1,
    APC_PROXY_DRIVER_E_DOWNLOAD_BUSY      = 2,
    APC_PROXY_DRIVER_E_DOWNLOAD_FAILED    = 3,
    APC_PROXY_DRIVER_E_FPT_UPDATE         = 4,
    APC_PROXY_DRIVER_E_COPY_STARTED       = 5,
    APC_PROXY_DRIVER_E_COPY_COMPLETE      = 6,
    APC_PROXY_DRIVER_E_COPY_BUSY          = 7,
    APC_PROXY_DRIVER_E_COPY_FAILED        = 8,
    APC_PROXY_DRIVER_E_PARTITION_SELECTED = 9,
    APC_PROXY_DRIVER_E_PARTITION_SELECTION_FAILED = 10,
    APC_PROXY_DRIVER_E_FPT_FLAGS_UPDATED  = 11,
    APC_PROXY_DRIVER_E_PROGRAM_STARTED    = 12,
    APC_PROXY_DRIVER_E_PROGRAM_COMPLETE   = 13,
    APC_PROXY_DRIVER_E_PROGRAM_BUSY       = 14,
    APC_PROXY_DRIVER_E_PROGRAM_FAILED     = 15,
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

/**
 * @enum    APC_FPT_TYPE
 * @brief   FPT partition types
 */
typedef enum
{
	APC_FPT_TYPE_FPT             = 0xFFFF,
	APC_FPT_TYPE_RECOVERY_FPT    = 0xFFFE,
	APC_FPT_TYPE_EXTENSION_FPT   = 0xFFFD,
	APC_FPT_TYPE_PDI_BOOT        = 0x0E00,
	APC_FPT_TYPE_PDI_BOOT_BACKUP = 0x0E01,
	APC_FPT_TYPE_PDI_XSABIN_META = 0x0E02,
	APC_FPT_TYPE_PDI_GOLDEN      = 0x0E03,
	APC_FPT_TYPE_PDI_SYS_DTB     = 0x0E04,
	APC_FPT_TYPE_PDI_META        = 0x0E05,
	APC_FPT_TYPE_PDI_META_BACKUP = 0x0E06,
	APC_FPT_TYPE_PDI_APU         = 0x0E07,
	APC_FPT_TYPE_PDI_RPU         = 0x0E08,
	APC_FPT_TYPE_PDI_USER        = 0x0F00,
	APC_FPT_TYPE_SC_FW           = 0x0C00,
} APC_FPT_TYPE;

/******************************************************************************/
/* Structs                                                                    */
/******************************************************************************/

/**
 * @struct  APCProxyDriverFptHeader
 * @brief   Structure to hold the FPT Header
 */
typedef struct
{
    uint32_t    ulMagicNum;
    uint8_t     ucFptVersion;
    uint8_t     ucFptHeaderSize;
    uint8_t     ucEntrySize;
    uint8_t     ucNumEntries;

} APCProxyDriverFptHeader;

/**
 * @struct  APCProxyDriverFptPartition
 * @brief   Structure to hold a single FPT partition
 */
typedef struct
{
    uint32_t    ulPartitionType;
    uint32_t    ulPartitionBaseAddr;
    uint32_t    ulPartitionSize;
    uint8_t     pdi_md5[MD5_SIZE];
    uint32_t    ulPdiSize;
    union {
        uint32_t ulPartitionFlags;     /* User defined flags */
        struct {
            uint32_t powerup_flag:1;    /* Power on load user partition flag
                                           0: Disabled, 1: Enabled */
            uint32_t reserved1:15;      /* Reserved for future use */
            uint32_t powerup_error:2;   /* Power on load user partition error flag
                                           0: Not Loaded, 1: Loaded, 2: Error */
            uint32_t reserved2:14;      /* Reserved for future use */
        } user;
    };

} APCProxyDriverFptPartition;

typedef struct AMIProxyPdiDownloadRequest AMIProxyPdiDownloadRequest;

extern uint32_t ulUserPdiLoadStatus;    /* User PDI power on load status 0: Success */

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
                     FWIfCfg *pxPrimaryFwIf,
                     FWIfCfg *pxSecondaryFwIf,
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
 * @ulPacketSize: Size of image packet (in KB)
 * @pucPdiMd5:    Pointer to 16-byte PDI MD5 checksum
 * @ulPdiSize:    Size of full PDI file in bytes
 * @iLastPacket:  Boolean indicating if this is the last data packet
 *
 * @return  OK    Image downloaded successfully
 *          ERROR Image not downloaded successfully
 */
int iAPC_DownloadImage( EVLSignal *pxSignal,
                        APC_BOOT_DEVICES xBootDevice,
                        int iPartition,
                        uint32_t ulSrcAddr,
                        uint32_t ulImageSize,
                        uint16_t usPacketNum,
                        uint32_t ulPacketSize,
                        uint8_t *pucPdiMd5,
                        uint32_t ulPdiSize,
                        int iLastPacket );

/**
 * iAPC_UpdateFpt() - Download an image with an FPT to a location in NV memory
 *
 * @pxSignal:     Current event occurance (used for tracking)
 * @xBootDevice:  Target boot device
 * @ulSrcAddr:    Address (in RAM) to read the image from
 * @ulImageSize:  Size of image (in bytes)
 * @usPacketNum:  Image packet number
 * @ulPacketSize: Size of image packet (in KB)
 * @iLastPacket:  Boolean indicating if this is the last data packet
 *
 * @return  OK    Image downloaded successfully
 *          ERROR Image not downloaded successfully
 */
int iAPC_UpdateFpt( EVLSignal *pxSignal,
                    APC_BOOT_DEVICES xBootDevice,
                    uint32_t ulSrcAddr,
                    uint32_t ulImageSize,
                    uint16_t usPacketNum,
                    uint32_t ulPacketSize,
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
int iAPC_CopyImage( EVLSignal *pxSignal,
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
 * @pxDownloadRequest:  Target boot device
 * @ulSrcAddr:    Address (in RAM) to read the image from
 *
 * @return  OK    PDI programmed successfully
 *          ERROR PDI programming failed
 */
int iAPC_PdiProgram( EVLSignal *pxSignal,
                     const AMIProxyPdiDownloadRequest *pxDownloadRequest,
                     uint32_t ulSrcAddr );

/**
 * iAPC_SetNextPartition() - Select which partition (from primary boot device) to boot from
 *
 * @pxSignal    Current event occurance (used for tracking)
 * @iPartition  The partition to boot from on the next reset
 *
 * @return  OK    Partition successfully selected
 *          ERROR Partition not selected
 */
int iAPC_SetNextPartition( EVLSignal *pxSignal, int iPartition );

/**
 * iAPC_EnableHotReset() - Enable the hot reset capability (from primary boot device)
 *
 * @pxSignal: Current event occurance (used for tracking)
 *
 * @return  OK    Hot reset successfully enabled
 *          ERROR Hot reset not enabled
 */
int iAPC_EnableHotReset( EVLSignal *pxSignal );

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
    APCProxyDriverFptHeader *pxFptHeader );

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
    APCProxyDriverFptPartition *pxFptPartition );

/**
 * iAPC_SetFptPartitionFlags() - Set/Update the flags of a Flash Partition Table (FPT) Partition
 *
 * @pxSignal:       Current event occurance (used for tracking)
 * @xBootDevice:    Target boot device
 * @iPartition:     Index of partition to update (0 is the 1st partition)
 * @ulFlags:        New flags value to set
 *
 * @return  OK      FPT partition flags updated successfully
 *          ERROR   FPT partition flags not updated successfully
 *
 * @note    This function updates the partition flags in both the cached copy
 *          and the flash storage
 */
int iAPC_SetFptPartitionFlags( EVLSignal *pxSignal,
                               APC_BOOT_DEVICES xBootDevice,
                               int iPartition,
                               uint32_t ulFlags );

/**
 * iAPC_SetFptPartition() - Set/Update an FPT Partition entry
 *
 * @pxSignal:       Current event occurance (used for tracking)
 * @xBootDevice:    Target boot device
 * @iPartition:     Index of partition to update (0 is the 1st partition)
 * @pucPdiMd5:      Pointer to 16-byte PDI MD5 checksum (NULL to skip update)
 * @ulPdiSize:      Size of the PDI file in bytes
 * @ulFlags:        New flags value to set
 *
 * @return  OK      FPT partition updated successfully
 *          ERROR   FPT partition not updated successfully
 *
 * @note    This function updates the partition in both the cached copy
 *          and the flash storage
 */
int iAPC_SetFptPartition( EVLSignal *pxSignal,
                          APC_BOOT_DEVICES xBootDevice,
                          int iPartition,
                          uint8_t *pucPdiMd5,
                          uint32_t ulPdiSize,
                          uint32_t ulFlags );

/**
 * iAPC_ReadFlashRaw() - Read raw bytes from flash
 *
 * @xBootDevice:    Target boot device
 * @ulOffset:       Offset in flash to read from
 * @pucBuffer:      Buffer to store the read data
 * @ulLength:       Number of bytes to read
 *
 * @return  OK      Data read successfully
 *          ERROR   Data not read successfully
 */
int iAPC_ReadFlashRaw( APC_BOOT_DEVICES xBootDevice,
                       uint32_t ulOffset,
                       uint8_t *pucBuffer,
                       uint32_t ulLength );

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
