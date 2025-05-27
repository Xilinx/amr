/**
 * Copyright (c) 2024 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the fal profile for the Rave
 *
 * @file profile_fal.c
 */

/*****************************************************************************/
/* Includes                                                                  */
/*****************************************************************************/

/* core libs */
#include "pll.h"

/* fal */
#include "fw_if_muxed_device.h"
#include "fw_if_gcq.h"
#include "fw_if_ospi.h"
#include "fw_if_smbus.h"
#include "fw_if_test.h"
#include "fw_if_smbus.h"

/* device drivers */
#include "i2c.h"

/* proxy drivers*/
#include "apc_proxy_driver.h"
#include "ami_proxy_driver.h"

/* hardware definitions */
#include "profile_fal.h"
#include "profile_hal.h"
#include "profile_muxed_device.h"

#ifdef DEBUG_BUILD
#include "fw_if_gcq_debug.h"
#include "fw_if_ospi_debug.h"
#include "fw_if_muxed_device_debug.h"
#endif


/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/

#define FAL_PROFILE_NAME      "PROFILE_FAL"
#define OSPI_PAGE_SIZE        ( 256 )
#define OSPI_RPU_BASE_ADDRESS ( 0x0 )
#define OSPI_RPU_LENGTH       ( 0x08000000 )    /* 1Gb (128MB) */


/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/

/* FAL objects */
FW_IF_CFG xGcqIf =
{
    0
};
FW_IF_CFG xOspiIf =
{
    0
};
FW_IF_CFG xQsfpIf1 =
{
    0
};
FW_IF_CFG xQsfpIf2 =
{
    0
};
FW_IF_CFG xQsfpIf3 =
{
    0
};
FW_IF_CFG xQsfpIf4 =
{
    0
};
FW_IF_CFG xDimmIf =
{
    0
};


FW_IF_CFG *pxEmmcIf  = NULL;
FW_IF_CFG *pxOspiIf  = &xOspiIf;
FW_IF_CFG *pxSMBusIf = NULL;

/*****************************************************************************/
/* Local variables                                                           */
/*****************************************************************************/

static FW_IF_GCQ_CFG xGcqCfg =
{
    ( uint64_t )HAL_BASE_LOGIC_GCQ_M2R_S01_AXI_BASEADDR,
    FW_IF_GCQ_MODE_PRODUCER,
    FW_IF_GCQ_INTERRUPT_MODE_NONE,
    ( uint64_t )HAL_RPU_RING_BUFFER_BASE,
    HAL_RPU_RING_BUFFER_LEN,
    AMI_PROXY_RESPONSE_SIZE,
    AMI_PROXY_REQUEST_SIZE,
    ""
};

static FW_IF_GCQ_INIT_CFG myGcqIf =
{
    NULL
};

static FW_IF_OSPI_CFG xOspiCfg =
{
    OSPI_RPU_BASE_ADDRESS,
    OSPI_RPU_LENGTH,
    TRUE,                                                                      /* Enable erase before write */
    FW_IF_OSPI_STATE_INIT
};

static FW_IF_OSPI_INIT_CFG myOspiIf =
{
    HAL_OSPI_0_DEVICE_ID,
    OSPI_PAGE_SIZE
};



/******************************************************************************/
/* Public Function implementations                                            */
/******************************************************************************/

/**
 * @brief   Initialise FAL layer
 */
int iFAL_Initialise( uint64_t *pullAmcInitStatus )
{
    int     iStatus                         = OK;
    uint8_t ucUuidSize                      = 0;
    uint8_t pucUuid[ FW_IF_SMBUS_UDID_LEN ] =
    {
        0
    };

    if( NULL != pullAmcInitStatus )
    {
        /* Init the sGCQ FAL */
        PLL_LOG( FAL_PROFILE_NAME, "sGCQ service: starting\r\n" );
        if( FW_IF_ERRORS_NONE == ulFW_IF_GCQ_Init( &myGcqIf ) )
        {
            PLL_DBG( FAL_PROFILE_NAME, "sGCQ FAL initialised OK\r\n" );
            PLL_LOG( FAL_PROFILE_NAME, "sGCQ service: ready\r\n" );
            *pullAmcInitStatus |= AMC_CFG_GCQ_FAL_INITIALISED;
        }
        else
        {
            PLL_ERR( FAL_PROFILE_NAME, "Error initialising sGCQ FAL\r\n" );
            PLL_LOG( FAL_PROFILE_NAME, "sGCQ service: error initialising\r\n" );
            iStatus = ERROR;
        }

        /* Init the OSPI FAL */
        PLL_LOG( FAL_PROFILE_NAME, "OSPI driver: starting\r\n" );
        if( FW_IF_ERRORS_NONE == ulFW_IF_OSPI_Init( &myOspiIf ) )
        {
            PLL_DBG( FAL_PROFILE_NAME, "OSPI FAL initialised OK\r\n" );
            PLL_LOG( FAL_PROFILE_NAME, "OSPI driver: ready\r\n" );
            *pullAmcInitStatus |= AMC_CFG_OSPI_FAL_INITIALISED;
        }
        else
        {
            PLL_ERR( FAL_PROFILE_NAME, "Error initialising OSPI FAL\r\n" );
            PLL_LOG( FAL_PROFILE_NAME, "OSPI driver: error initialising\r\n" );
            iStatus = ERROR;
        }

        /* Create instance of the sGCQ based on the global configuration */
        if( AMC_CFG_GCQ_FAL_INITIALISED == ( *pullAmcInitStatus & AMC_CFG_GCQ_FAL_INITIALISED ) )
        {
            if( FW_IF_ERRORS_NONE == ulFW_IF_GCQ_Create( &xGcqIf, &xGcqCfg ) )
            {
                PLL_DBG( FAL_PROFILE_NAME, "sGCQ created OK\r\n" );
                *pullAmcInitStatus |= AMC_CFG_GCQ_FAL_CREATED;
            }
            else
            {
                PLL_ERR( FAL_PROFILE_NAME, "Error creating sGCQ\r\n" );
                iStatus = ERROR;
            }
        }

        /* Create instance of the OSPI based on the global configuration */
        if( AMC_CFG_OSPI_FAL_INITIALISED == ( *pullAmcInitStatus & AMC_CFG_OSPI_FAL_INITIALISED ) )
        {
            if( FW_IF_ERRORS_NONE == ulFW_IF_OSPI_Create( &xOspiIf, &xOspiCfg ) )
            {
                PLL_DBG( FAL_PROFILE_NAME, "OSPI created OK\r\n" );
                *pullAmcInitStatus |= AMC_CFG_OSPI_FAL_CREATED;
            }
            else
            {
                PLL_ERR( FAL_PROFILE_NAME, "Error creating OSPI\r\n" );
                iStatus = ERROR;
            }
        }

        /* Get UUID */
        if( AMC_CFG_EEPROM_INITIALISED == ( *pullAmcInitStatus & AMC_CFG_EEPROM_INITIALISED ) )
        {
            /* If EEPROM is ok use the UUID from it */
            if( OK == iEEPROM_GetUuid( pucUuid, &ucUuidSize ) )
            {
                if( FW_IF_SMBUS_UDID_LEN != ucUuidSize )
                {
                    PLL_ERR( FAL_PROFILE_NAME, "Error UUID size incorrect\n\r" );
                    iStatus = ERROR;
                }
            }
            else
            {
                PLL_ERR( FAL_PROFILE_NAME, "Error getting UUID\r\n" );
                iStatus = ERROR;
            }
        }
    }
    else
    {
        iStatus = ERROR;
    }

    return iStatus;

}

/**
 * @brief   Initialise FAL Debug monitoring
 */
void vFAL_DebugInitialise( void )
{
#ifdef DEBUG_BUILD
    /* FALs */
    static DAL_HDL pxFwIfTop = NULL;
    pxFwIfTop = pxDAL_NewDirectory( "fw_if" );

    vFW_IF_GCQ_DebugInit( pxFwIfTop );
    vFW_IF_OSPI_DebugInit( pxFwIfTop );

#endif
}
