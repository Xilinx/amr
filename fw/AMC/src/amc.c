/**
 * Copyright (c) 2024 - 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the main entry point for the AMR Managment Controller
 *
 * @file amc.c
 */

/* common includes */
#include "standard.h"
#include "util.h"
#include "amc_cfg.h"
#include "amc_version.h"
#include "xil_util.h"
#include "xil_cache.h"
#include "xloader_client.h"
#include "xloader_bsp_config.h"

/* osal */
#include "osal.h"

/* core_libs */
#include "pll.h"
#include "evl.h"
#include "dal.h"

/* device drivers */
#include "i2c.h"
#include "eeprom.h"
#include "sys_mon.h"
#include "gcq.h"

/* fal */
#include "fw_if_test.h"
#include "fw_if_gcq.h"
#include "fw_if_ospi.h"
#include "fw_if_muxed_device.h"

/* proxy drivers */
#include "axc_proxy_driver.h"
#include "apc_proxy_driver.h"
#include "asc_proxy_driver.h"
#include "ami_proxy_driver.h"
#include "bmc_proxy_driver.h"

/* bim app data */
#include "profile_bim.h"

/* apps */
#include "asdm.h"
#include "in_band_telemetry.h"
#include "out_of_band_telemetry.h"
#include "bim.h"

/* PDR data */
#include "profile_pdr.h"

/* sensor data */
#include "profile_sensors.h"

/* hardware definitions */
#include "profile_hal.h"
#include "profile_fal.h"
#include "profile_muxed_device.h"
#include "profile_debug_menu.h"


/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/

#define PL_NODE_ID              (0x18700000) /** PL Node ID to configure */
#define AMC_OUTPUT_LEVEL        ( PLL_OUTPUT_LEVEL_WARNING )
#define AMC_LOGGING_LEVEL       ( PLL_OUTPUT_LEVEL_LOGGING )

#define AMC_NAME                "AMC"

#define AMC_HASH_LEN            ( 7 )
#define AMC_DATE_LEN            ( 8 )

#define AMC_TASK_DEFAULT_STACK        ( 0x1000 )
#define AMC_TASK_SLEEP_MS             ( 100 )
#define AMC_GET_PROJECT_INFO_SLEEP_MS ( 1500 )


/******************************************************************************/
/* Enums                                                                      */
/******************************************************************************/

/**
 * @enum    AMC_TASK_PRIOS
 * @brief   AMC Task priorities
 */
typedef enum
{
    AMC_TASK_PRIO_RSVD = 5,     /* TODO: get actual value from osal.h */

    AMC_TASK_PRIO_DEFAULT,
    MAX_AMC_TASK_PRIO

} AMC_TASK_PRIOS;


/******************************************************************************/
/* EVL Callback Declarations                                                  */
/******************************************************************************/

/**
 * @brief   EVL Callbacks for binding to Proxy Drivers
 *
 * @param   pxSignal     Event raised
 *
 * @return  OK if no errors were raised in the callback
 *          ERROR if an error was raised in the callback
 */
static int iApcCallback( EVL_SIGNAL *pxSignal );
static int iAmiCallback( EVL_SIGNAL *pxSignal );
static int iAxcCallback( EVL_SIGNAL *pxSignal );
static int iBmcCallback( EVL_SIGNAL *pxSignal );


/******************************************************************************/
/* Local Function Declarations                                                */
/******************************************************************************/

/**
 * @brief   Get project info
 *
 * @return  N/A
 */
static void vGetProjectInfo( void );

/**
 * @brief   Initialise core libraries
 *
 * @return  OK if all core libraries initialised successfully
 *          ERROR if any or all core libraries not initialised
 */
static int iInitCoreLibs( void );

/**
 * @brief   Initialise device drivers
 *
 * @return  OK if all device drivers initialised and created successfully
 *          ERROR if any or all device drivers not initialised
 */
static int iInitDeviceDrivers( void );

/**
 * @brief   Initialise Proxy Driver layer
 *
 * @return  OK if all Proxy Drivers initialised and bound successfully
 *          ERROR if any or all proxy drivers not initialised
 */
static int iInitProxies( void );

/**
 * @brief   Initialise App layer
 *
 * @return  OK if all Apps initialised and created successfully
 *          ERROR if any or all apps not initialised
 */
static int iInitApp( void );

/**
 * @brief   Initialise Debug monitoring
 *
 * @return  OK if debug initialised (or if debug disabled)
 *          ERROR if debug enabled but did not initialise
 */
static int iInitDebug( void );

/**
 * @brief   Load PDI from OSPI flash on power-up
 *
 * @return  OK if PDI loaded successfully
 *          ERROR if PDI loading failed
 */
static int iLoadPdiOnPowerUp( void );

/**
 * @brief   The main task that init the FAL & proxy drivers
 *
 * @return  N/A
 */
static void vTaskFuncMain( void );

/**
 * @brief   Configure the shared memory table stored at the start of
 *          shared memory and used by the AMI to deremine the AMC state
 * @return  N/A
 */
static void vConfigureSharedMemTable( void );


/******************************************************************************/
/* Local variables                                                            */
/******************************************************************************/

/* Note: the default I2C clock frequency isn't used */
static I2C_CFG_TYPE xI2cCfg[ I2C_NUM_INSTANCES ] =
{
    {
        HAL_I2C_BUS_0_DEVICE_ID,
        ( uint64_t )HAL_I2C_BUS_0_BASEADDR,
        HAL_I2C_BUS_0_I2C_CLK_FREQ_HZ,
        HAL_I2C_RETRY_COUNT,
        HAL_I2C_BUS_0_SW_RESET_OFFSET,
        HAL_I2C_BUS_0_RESET_ON_INIT,
        HAL_I2C_BUS_0_HW_RESET_ADDR,
        HAL_I2C_BUS_0_HW_RESET_MASK,
        HAL_I2C_BUS_0_HW_DEVICE_RESET
    },
    {
        HAL_I2C_BUS_1_DEVICE_ID,
        ( uint64_t )HAL_I2C_BUS_1_BASEADDR,
        HAL_I2C_BUS_1_I2C_CLK_FREQ_HZ,
        HAL_I2C_RETRY_COUNT,
        HAL_I2C_BUS_1_SW_RESET_OFFSET,
        HAL_I2C_BUS_1_RESET_ON_INIT,
        HAL_I2C_BUS_1_HW_RESET_ADDR,
        HAL_I2C_BUS_1_HW_RESET_MASK,
        HAL_I2C_BUS_1_HW_DEVICE_RESET
    }
};

static EEPROM_CFG xEepromCfg =
{
    HAL_EEPROM_I2C_BUS,
    HAL_EEPROM_SLAVE_ADDRESS,
    HAL_EEPROM_ADDRESS_SIZE,
    HAL_EEPROM_PAGE_SIZE,
    HAL_EEPROM_NUM_PAGES,
    HAL_EEPROM_DEVICE_ID_ADDRESS,
    HAL_EEPROM_DEVICE_ID_REGISTER,
    HAL_EEPROM_DEVICE_ID
};

/* AXC External Device configs */
static AXC_PROXY_DRIVER_EXTERNAL_DEVICE_CONFIG xQsfpDevice1 =
{
    &xQsfpIf1, 0
};

static AXC_PROXY_DRIVER_EXTERNAL_DEVICE_CONFIG xQsfpDevice2 =
{
    &xQsfpIf2, 1
};

static AXC_PROXY_DRIVER_EXTERNAL_DEVICE_CONFIG xQsfpDevice3 =
{
    &xQsfpIf3, 2
};

static AXC_PROXY_DRIVER_EXTERNAL_DEVICE_CONFIG xQsfpDevice4 =
{
    &xQsfpIf4, 3
};

static AXC_PROXY_DRIVER_EXTERNAL_DEVICE_CONFIG xDimmDevice =
{
    &xDimmIf, 4
};

static uint64_t ullAmcInitStatus = 0;          /* AMC initialisation status */
static XLoader_ClientInstance XLoaderInstance; /* XLoader instance */


/******************************************************************************/
/* Function implementations                                                   */
/******************************************************************************/

/*
 * @brief   AMC main task
 */
static void vTaskFuncMain( void )
{
    int iStatus = ERROR;

    vConfigureSharedMemTable();

    if( OK == iInitCoreLibs() )
    {
        PLL_LOG( AMC_NAME, "Core Libs initialised OK\r\n" );
        iStatus = OK;
    }
    else
    {
        PLL_LOG( AMC_NAME, "Core Libs initialisation ERROR\t\n" );
    }

    if( OK == iInitDeviceDrivers() )
    {
        PLL_LOG( AMC_NAME, "Device drivers Initialised OK\r\n" );
        iStatus = OK;
    }
    else
    {
        PLL_LOG( AMC_NAME, "Device drivers Initialisation ERROR\r\n" );
    }

    if( OK == iFAL_Initialise( &ullAmcInitStatus ) )
    {
        PLL_LOG( AMC_NAME, "FAL Initialised OK\r\n" );
    }
    else
    {
        PLL_LOG( AMC_NAME, "FAL Initialisation ERROR\r\n" );
    }

    if( OK == iInitProxies() )
    {
        PLL_LOG( AMC_NAME, "Proxy Drivers Initialised OK\r\n" );
        iStatus = OK;
    }
    else
    {
        PLL_LOG( AMC_NAME, "Proxy Drivers Initialisation ERROR\r\n" );
    }

    if( OK == iInitApp() )
    {
        PLL_LOG( AMC_NAME, "Apps Initialised OK\r\n" );
    }
    else
    {
        PLL_LOG( AMC_NAME, "Apps Initialisation ERROR\r\n" );
        iStatus = ERROR;
    }

    /* always initialise debug interface (if enabled) */
    if( OK != iInitDebug() )
    {
        PLL_ERR( AMC_NAME, "Debug access initialisation error\r\n" );
    }

    if( ERROR == iStatus )
    {
        /*
         * The final step before starting the main task is to configure the start
         * of the shared memory with the information needed by the AMI.
         */
        PLL_ERR( AMC_NAME, "Error Main Task has initialisation failures\r\n" );
    }

    PLL_INF( AMC_NAME, "ullAmcInitStatus:\n\r" );
    PLL_INF( AMC_NAME,
             "ucPllInitialised                %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_PLL_INITIALISED            ? "TRUE" : "FALSE" ) );
    PLL_INF( AMC_NAME,
             "ucEvlInitialised                %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_EVL_INITIALISED            ? "TRUE" : "FALSE" ) );
    PLL_INF( AMC_NAME,
             "ucI2cInitialised                %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_I2C_INITIALISED            ? "TRUE" : "FALSE" ) );
    PLL_INF( AMC_NAME,
             "ucEepromInitialised             %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_EEPROM_INITIALISED         ? "TRUE" : "FALSE" ) );
    PLL_INF( AMC_NAME,
             "ucSysmonInitialised             %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_SYSMON_INITIALISED         ? "TRUE" : "FALSE" ) );
#if (HAL_SMBUS_FEATURE == 1)
    PLL_INF( AMC_NAME,
             "ucSmbusPcieLinkInitialised      %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_SMBUS_PCIE_LINK_INITIALISED  ? "TRUE" : "FALSE" ) );
    PLL_INF( AMC_NAME,
             "ucMuxedDeviceFalInitialised     %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_MUXED_DEVICE_FAL_INITIALISED ? "TRUE" : "FALSE" ) );
#endif
    PLL_INF( AMC_NAME,
             "ucGcqFalInitialised             %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_GCQ_FAL_INITIALISED         ? "TRUE" : "FALSE" ) );
#if (HAL_EMMC_FEATURE == 1)
    PLL_INF( AMC_NAME,
             "ucEmmcFalInitialised            %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_EMMC_FAL_INITIALISED        ? "TRUE" : "FALSE" ) );
#endif
    PLL_INF( AMC_NAME,
             "ucOspiFalInitialised            %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_OSPI_FAL_INITIALISED        ? "TRUE" : "FALSE" ) );
#if (HAL_SMBUS_FEATURE == 1)
    PLL_INF( AMC_NAME,
             "ucSmbusFalInitialised           %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_SMBUS_FAL_INITIALISED       ? "TRUE" : "FALSE" ) );
    PLL_INF( AMC_NAME,
             "ucMuxedDeviceFalCreated         %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_MUXED_DEVICE_FAL_CREATED     ? "TRUE" : "FALSE" ) );
#endif
    PLL_INF( AMC_NAME,
             "ucGcqFalCreated                 %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_GCQ_FAL_CREATED             ? "TRUE" : "FALSE" ) );
#if (HAL_EMMC_FEATURE == 1)
    PLL_INF( AMC_NAME,
             "ucEmmcFalCreated                %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_EMMC_FAL_CREATED            ? "TRUE" : "FALSE" ) );
#endif
    PLL_INF( AMC_NAME,
             "ucOspiFalCreated                %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_OSPI_FAL_CREATED            ? "TRUE" : "FALSE" ) );
#if (HAL_SMBUS_FEATURE == 1)
    PLL_INF( AMC_NAME,
             "ucSmbusFalCreated               %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_SMBUS_FAL_CREATED           ? "TRUE" : "FALSE" ) );
#endif
    PLL_INF( AMC_NAME,
             "ucApcInitialised                %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_APC_INITIALISED            ? "TRUE" : "FALSE" ) );
    PLL_INF( AMC_NAME,
             "ucAxcInitialised                %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_AXC_INITIALISED            ? "TRUE" : "FALSE" ) );
    PLL_INF( AMC_NAME,
             "ucAscInitialised                %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_ASC_INITIALISED            ? "TRUE" : "FALSE" ) );
    PLL_INF( AMC_NAME,
             "ucAmiInitialised                %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_AMI_INITIALISED            ? "TRUE" : "FALSE" ) );
#if (HAL_SMBUS_FEATURE == 1)
    PLL_INF( AMC_NAME,
             "ucBmcInitialised                %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_BMC_INITIALISED            ? "TRUE" : "FALSE" ) );
#endif
    PLL_INF( AMC_NAME,
             "ucAsdmInitialised               %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_ASDM_INITIALISED           ? "TRUE" : "FALSE" ) );
    PLL_INF( AMC_NAME,
             "ucInBandInitialised             %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_IN_BAND_INITIALISED         ? "TRUE" : "FALSE" ) );
#if (HAL_SMBUS_FEATURE == 1)
    PLL_INF( AMC_NAME,
             "ucOutOfBandInitialised          %s\n\r",
             ( ullAmcInitStatus & AMC_CFG_OUT_OF_BAND_INITIALISED      ? "TRUE" : "FALSE" ) );
#endif

    for( ;; )
    {
        iOSAL_Task_SleepMs( AMC_TASK_SLEEP_MS );
    }
}

/**
 * @brief   Main entry point
 */
int main( void )
{
    void *pvMainTaskHandle = NULL;

    if( OSAL_ERRORS_OS_NOT_STARTED != iOSAL_StartOS( TRUE,
                                                     &pvMainTaskHandle,
                                                     &vTaskFuncMain,
                                                     AMC_TASK_DEFAULT_STACK,
                                                     AMC_TASK_PRIO_DEFAULT ) )
    {
        PLL_ERR( AMC_NAME, "Error failed to start the OS Task\r\n" );
    }

    return -1;
}


/******************************************************************************/
/* EVL Callback Implementations                                               */
/******************************************************************************/

/**
 * @brief   AXC Proxy Driver EVL callback
 */
static int iAxcCallback( EVL_SIGNAL *pxSignal )
{
    int iStatus = ERROR;

    if( ( NULL != pxSignal ) && ( AMC_CFG_UNIQUE_ID_AXC == pxSignal->ucModule ) )
    {
        switch( pxSignal->ucEventType )
        {
            case AXC_PROXY_DRIVER_E_QSFP_PRESENT:
                iStatus = OK;
                break;

            case AXC_PROXY_DRIVER_E_QSFP_NOT_PRESENT:
                iStatus = OK;
                break;

            default:
                break;
        }
    }
    return iStatus;
}

/**
 * @brief   APC Proxy Driver EVL callback
 */
static int iApcCallback( EVL_SIGNAL *pxSignal )
{
    int iStatus = ERROR;

    if( ( NULL != pxSignal ) && ( AMC_CFG_UNIQUE_ID_APC == pxSignal->ucModule ) )
    {
        switch( pxSignal->ucEventType )
        {
            case APC_PROXY_DRIVER_E_DOWNLOAD_STARTED:
                iStatus = OK;
                break;

            case APC_PROXY_DRIVER_E_DOWNLOAD_COMPLETE:
                iStatus = iAMI_SetPdiDownloadCompleteResponse( pxSignal, AMI_PROXY_RESULT_SUCCESS );
                break;

            case APC_PROXY_DRIVER_E_DOWNLOAD_FAILED:
                iStatus = iAMI_SetPdiDownloadCompleteResponse( pxSignal, AMI_PROXY_RESULT_FAILURE );
                break;

            case APC_PROXY_DRIVER_E_DOWNLOAD_BUSY:
                iStatus = iAMI_SetPdiDownloadCompleteResponse( pxSignal, AMI_PROXY_RESULT_ALREADY_IN_PROGRESS );
                break;

            case APC_PROXY_DRIVER_E_COPY_STARTED:
                iStatus = OK;
                break;

            case APC_PROXY_DRIVER_E_COPY_COMPLETE:
                iStatus = iAMI_SetPdiCopyCompleteResponse( pxSignal, AMI_PROXY_RESULT_SUCCESS );
                break;

            case APC_PROXY_DRIVER_E_COPY_FAILED:
                iStatus = iAMI_SetPdiCopyCompleteResponse( pxSignal, AMI_PROXY_RESULT_FAILURE );
                break;

            case APC_PROXY_DRIVER_E_COPY_BUSY:
                iStatus = iAMI_SetPdiCopyCompleteResponse( pxSignal, AMI_PROXY_RESULT_ALREADY_IN_PROGRESS );
                break;

            case APC_PROXY_DRIVER_E_PARTITION_SELECTED:
                iStatus = iAMI_SetBootSelectCompleteResponse( pxSignal, AMI_PROXY_RESULT_SUCCESS );
                break;

            case APC_PROXY_DRIVER_E_PARTITION_SELECTION_FAILED:
                iStatus = iAMI_SetBootSelectCompleteResponse( pxSignal, AMI_PROXY_RESULT_FAILURE );
                break;

            case APC_PROXY_DRIVER_E_PROGRAM_COMPLETE:
                iStatus = iAMI_SetPdiProgramCompleteResponse( pxSignal, AMI_PROXY_RESULT_SUCCESS );
                break;

            case APC_PROXY_DRIVER_E_PROGRAM_FAILED:
                iStatus = iAMI_SetPdiProgramCompleteResponse( pxSignal, AMI_PROXY_RESULT_FAILURE );
                break;

            case APC_PROXY_DRIVER_E_PROGRAM_BUSY:
                iStatus = iAMI_SetPdiProgramCompleteResponse( pxSignal, AMI_PROXY_RESULT_ALREADY_IN_PROGRESS );
                break;

            default:
                break;
        }
    }

    return iStatus;
}

/**
 * @brief   AMI Proxy Driver EVL callback
 */
static int iAmiCallback( EVL_SIGNAL *pxSignal )
{
    int iStatus = ERROR;

    if( ( NULL != pxSignal ) && ( AMC_CFG_UNIQUE_ID_AMI == pxSignal->ucModule ) )
    {
        switch( pxSignal->ucEventType )
        {
            case AMI_PROXY_DRIVER_E_GET_IDENTITY:
            {
                PLL_DBG( AMC_NAME, "Event Get Identity (0x%02X)\r\n", pxSignal->ucEventType );

                AMI_PROXY_RESULT xResult = AMI_PROXY_RESULT_SUCCESS;

                GCQVersion xGcqVersion = { 0 };

                if( OK != iGCQGetVersion( &xGcqVersion ) )
                {
                    PLL_DBG( AMC_NAME, "Error getting sGCQ version\r\n" );
                    xResult = AMI_PROXY_RESULT_FAILURE;
                }

                AMI_PROXY_IDENTITY_RESPONSE xIdentityResponse =
                {
                    .ucVerMajor     = ( uint8_t )GIT_TAG_VER_MAJOR,
                    .ucVerMinor     = ( uint8_t )GIT_TAG_VER_MINOR,
                    .ucVerPatch     = ( uint8_t )GIT_TAG_VER_PATCH,
                    .ucLocalChanges = ( uint8_t )( GIT_STATUS )?( 1 ):( 0 ),
                    .usDevCommits   = ( uint16_t )GIT_TAG_VER_DEV_COMMITS,
                    .ucLinkVerMajor = xGcqVersion.ucVerMajor,
                    .ucLinkVerMinor = xGcqVersion.ucVerMinor
                };
                iStatus = iAMI_SetIdentityResponse( pxSignal, xResult, &xIdentityResponse );

                /* AMI is ready - enable hot reset */
                if( OK == iAPC_EnableHotReset( pxSignal ) )
                {
                    PLL_DBG( AMC_NAME, "Hot reset enabled\r\n" );
                }

                if( OK == iPLL_SendBootRecords() )
                {
                    PLL_INF( AMC_NAME, "Boot logs sent OK\r\n" );
                    iStatus = OK;
                }
                else
                {
                    PLL_ERR( AMC_NAME, "ERROR sending boot logs\r\n" );
                }
                break;
            }

            default:
            {
                iStatus = OK;
                break;
            }
        }
    }

    return iStatus;
}

/**
 * @brief   BMC Proxy Driver EVL callback
 */
static int iBmcCallback( EVL_SIGNAL *pxSignal )
{
    int iStatus = ERROR;

    if( ( NULL != pxSignal ) && ( AMC_CFG_UNIQUE_ID_BMC == pxSignal->ucModule ) )
    {
        switch( pxSignal->ucEventType )
        {
            default:
                break;
        }
    }

    return iStatus;
}

/**
 * @brief   Get project info
 */
static void vGetProjectInfo( void )
{
    char    pcOsName[ OSAL_OS_NAME_LEN ] = "unknown";
    uint8_t ucVerMaj                     = 0, ucVerMin = 0, ucVerBld = 0;

    iOSAL_GetOsVersion( pcOsName, &ucVerMaj, &ucVerMin, &ucVerBld );

    /* Sleep so we don't interfere with any other prints. */
    iOSAL_Task_SleepMs( AMC_GET_PROJECT_INFO_SLEEP_MS );

    vPLL_Printf( "\r\n" );
    vPLL_Printf( "###################################################################\r\n" );
    vPLL_Printf( "#                        ╔═╗    ╔╦╗    ╔═╗                        #\r\n" );
    vPLL_Printf( "#                        ╠═╣    ║║║    ║                          #\r\n" );
    vPLL_Printf( "#                        ╩ ╩    ╩ ╩    ╚═╝                        #\r\n" );
    vPLL_Printf( "#                 Adaptive Management Controller                  #\r\n" );
    vPLL_Printf( "#                                                                 #\r\n" );
    vPLL_Printf( "# Copyright (c) 2024 - 2026 Advanced Micro Devices, Inc.          #\r\n" );
    vPLL_Printf( "# All rights reserved.                                            #\r\n" );
    vPLL_Printf( "# SPDX-License-Identifier: MIT                                    #\r\n" );
    vPLL_Printf( "#                                                                 #\r\n" );
    vPLL_Printf( "###################################################################\r\n" );
    PLL_LOG( AMC_NAME,
             "AMC: %d.%d.%d-%d.%.*s.%.*s%c [%s]\r\n",
             GIT_TAG_VER_MAJOR,
             GIT_TAG_VER_MINOR,
             GIT_TAG_VER_PATCH,
             GIT_TAG_VER_DEV_COMMITS,
             AMC_HASH_LEN,
             GIT_HASH,
             AMC_DATE_LEN,
             GIT_DATE,
             ( GIT_STATUS )?( '*' ):( ' ' ),
             UTC_BUILD_TIME );
    PLL_LOG( AMC_NAME,
             "OS:  %s v%u.%u.%u\r\n",
             pcOsName,
             ucVerMaj,
             ucVerMin,
             ucVerBld );
    vPLL_Printf( "\r\n\r\n" );
}

/**
 * @brief   Initialise core libraries
 */
static int iInitCoreLibs( void )
{
    int iStatus = ERROR;

    if( OK == iPLL_Initialise( AMC_OUTPUT_LEVEL, AMC_LOGGING_LEVEL ) )
    {
        PLL_INF( AMC_NAME, "PLL initialised OK\r\n" );
        iStatus           = OK;
        ullAmcInitStatus |= AMC_CFG_PLL_INITIALISED;
    }
    else
    {
        PLL_ERR( AMC_NAME, "PLL initialisation ERROR\r\n" );
    }

    if( OK == iEVL_Initialise() )
    {
        PLL_INF( AMC_NAME, "EVL initialised OK\r\n" );
        iStatus           = OK;
        ullAmcInitStatus |= AMC_CFG_EVL_INITIALISED;
    }
    else
    {
        PLL_ERR( AMC_NAME, "EVL initialisation ERROR\r\n" );
    }

    return iStatus;
}

/**
 * @brief   Initialise device drivers
 */
static int iInitDeviceDrivers( void )
{
    int iStatus = OK;

    if( OK == iI2C_Init( xI2cCfg, I2C_DEFAULT_BUS_IDLE_WAIT_MS ) )
    {
        PLL_INF( AMC_NAME, "I2C driver Initialised OK\r\n" );
        ullAmcInitStatus |= AMC_CFG_I2C_INITIALISED;
    }
    else
    {
        PLL_ERR( AMC_NAME, "I2C driver Initialisation ERROR\r\n" );
        iStatus = ERROR;
    }

    if( AMC_CFG_I2C_INITIALISED == ( ullAmcInitStatus & AMC_CFG_I2C_INITIALISED ) )
    {
        if( OK == iEEPROM_Initialise( HAL_EEPROM_VERSION, &xEepromCfg ) )
        {
            PLL_INF( AMC_NAME, "iEEPROM_Initialised OK\r\n" );
            ullAmcInitStatus |= AMC_CFG_EEPROM_INITIALISED;

            if( ERROR == iEEPROM_DisplayEepromValues( ) )
            {
                PLL_ERR( AMC_NAME, "iEEPROM_DisplayEepromValues FAILED\r\n" );
            }
        }
        else
        {
            PLL_ERR( AMC_NAME, "iEEPROM_Initialised FAILED\r\n" );
        }
    }

    if( OK == iSYS_MON_Initialise() )
    {
        PLL_INF( AMC_NAME, "SysMon Driver Initialised OK\r\n" );
        ullAmcInitStatus |= AMC_CFG_SYSMON_INITIALISED;
    }
    else
    {
        PLL_ERR( AMC_NAME, "SysMon Driver Initialisation ERROR\r\n" );
        iStatus = ERROR;
    }

    return iStatus;
}

/**
 * @brief   Initialise Proxy Driver layer
 */
static int iInitProxies( void )
{
    int iStatus = OK;

    if( AMC_CFG_APC_PREREQUISITES == ( ullAmcInitStatus & AMC_CFG_APC_PREREQUISITES ) )
    {
        if( OK == iAPC_Initialise( AMC_CFG_UNIQUE_ID_APC,
                                   pxOspiIf,
                                   pxEmmcIf,
                                   &XLoaderInstance,
                                   AMC_TASK_PRIO_DEFAULT,
                                   AMC_TASK_DEFAULT_STACK ) )
        {
            if( OK == iAPC_BindCallback( &iApcCallback ) )
            {
                PLL_INF( AMC_NAME, "APC Proxy Driver initialised and bound\r\n" );
            }
            else
            {
                PLL_ERR( AMC_NAME, "Error binding to APC Proxy Driver\r\n" );
            }
            ullAmcInitStatus |= AMC_CFG_APC_INITIALISED;

            /* Load PDI from OSPI flash on power-up */
            if( OK == iLoadPdiOnPowerUp() )
            {
                PLL_INF( AMC_NAME, "PDI loaded from OSPI flash on power-up\r\n" );
            }
            else
            {
                PLL_WRN( AMC_NAME, "Failed to load PDI from OSPI flash on power-up\r\n" );
            }
        }
        else
        {
            PLL_ERR( AMC_NAME, "Error initialising APC Proxy Driver\r\n" );
            iStatus = ERROR;
        }
    }

    if( 0 != MAX_NUM_EXTERNAL_DEVICES )
    {
        if( AMC_CFG_AXC_PREREQUISITES == ( ullAmcInitStatus & AMC_CFG_AXC_PREREQUISITES ) )
        {
            if( OK == iAXC_Initialise( AMC_CFG_UNIQUE_ID_AXC, AMC_TASK_PRIO_DEFAULT, AMC_TASK_DEFAULT_STACK ) )
            {
                if( OK == iAXC_BindCallback( &iAxcCallback ) )
                {
                    if( ( OK == iAXC_AddExternalDevice( &xQsfpDevice1 ) ) &&
                        ( OK == iAXC_AddExternalDevice( &xQsfpDevice2 ) ) &&
                        ( OK == iAXC_AddExternalDevice( &xQsfpDevice3 ) ) &&
                        ( OK == iAXC_AddExternalDevice( &xQsfpDevice4 ) ) &&
                        ( OK == iAXC_AddExternalDevice( &xDimmDevice ) ) )
                    {
                        PLL_INF( AMC_NAME, "AXC Proxy Driver initialised and bound\r\n" );
                        ullAmcInitStatus |= AMC_CFG_AXC_INITIALISED;
                    }
                    else
                    {
                        PLL_ERR( AMC_NAME, "Error adding External Device to AXC Proxy Driver\r\n" );
                    }
                }
                else
                {
                    PLL_ERR( AMC_NAME, "Error binding to AXC Proxy Driver\r\n" );
                    iStatus = ERROR;
                }
            }
            else
            {
                PLL_ERR( AMC_NAME, "Error initialising AXC Proxy Driver\r\n" );
                iStatus = ERROR;
            }
        }
    }
    else
    {
        PLL_INF( AMC_NAME, "No external devices available - skipping AXC initialisation\r\n" );
        ullAmcInitStatus |= AMC_CFG_AXC_INITIALISED;
    }

    #if HAL_I2C_MUXED_DEVICE
    if ( AMC_CFG_ASC_MUX_PREREQUISITES == ( ullAmcInitStatus & AMC_CFG_ASC_MUX_PREREQUISITES ) )
    #else
    if ( AMC_CFG_ASC_PREREQUISITES == ( ullAmcInitStatus & AMC_CFG_ASC_PREREQUISITES ) )
    #endif
    {
        if( OK == iASC_Initialise( AMC_CFG_UNIQUE_ID_ASC,
                                   AMC_TASK_PRIO_DEFAULT,
                                   AMC_TASK_DEFAULT_STACK,
                                   PROFILE_SENSORS_SENSOR_DATA,
                                   PROFILE_SENSORS_NUM_SENSORS ) )
        {
            PLL_INF( AMC_NAME, "ASC Proxy Driver initialised\r\n" );
            ullAmcInitStatus |= AMC_CFG_ASC_INITIALISED;
        }
        else
        {
            PLL_ERR( AMC_NAME, "Error initialising ASC Proxy Driver\r\n" );
            iStatus = ERROR;
        }
    }

    if( AMC_CFG_AMI_PREREQUISITES == ( ullAmcInitStatus & AMC_CFG_AMI_PREREQUISITES ) )
    {
        if( OK == iAMI_Initialise( AMC_CFG_UNIQUE_ID_AMI,
                                   &xGcqIf,
                                   0,
                                   AMC_TASK_PRIO_DEFAULT,
                                   AMC_TASK_DEFAULT_STACK ) )
        {
            if( OK == iAMI_BindCallback( &iAmiCallback ) )
            {
                PLL_INF( AMC_NAME, "AMI Proxy Driver initialised and bound\r\n" );
                ullAmcInitStatus |= AMC_CFG_AMI_INITIALISED;
            }
            else
            {
                PLL_ERR( AMC_NAME, "Error binding to AMI Proxy Driver\r\n" );
            }
        }
        else
        {
            PLL_ERR( AMC_NAME, "Error initialising AMI Proxy Driver\r\n" );
            iStatus = ERROR;
        }
    }

    if( NULL != pxSMBusIf )
    {
        /* Get the UUID */
        uint8_t ucUuidSize               = 0;
        uint8_t pucUuid[ HAL_UUID_SIZE ] = { 0 };

        if( AMC_CFG_I2C_INITIALISED == ( ullAmcInitStatus & AMC_CFG_I2C_INITIALISED ) )
        {
            if( AMC_CFG_EEPROM_INITIALISED == ( ullAmcInitStatus & AMC_CFG_EEPROM_INITIALISED ) )
            {
                iStatus = iEEPROM_GetUuid( pucUuid, &ucUuidSize );
                if( OK == iStatus )
                {
                    if( HAL_UUID_SIZE != ucUuidSize )
                    {
                        PLL_ERR( AMC_NAME, "UUID Size incorrect\r\n" );
                        iStatus = ERROR;
                    }
                }
                else
                {
                    PLL_ERR( AMC_NAME, "Unable to read UUID\r\n" );
                }
            }
            else
            {
                /* Use the default (all 0s) UUID */
            }

            if( AMC_CFG_BMC_PREREQUISITES == ( ullAmcInitStatus & AMC_CFG_BMC_PREREQUISITES ) )
            {
                if( OK == iBMC_Initialise( AMC_CFG_UNIQUE_ID_BMC,
                                           pxSMBusIf,
                                           0,
                                           AMC_TASK_PRIO_DEFAULT,
                                           AMC_TASK_DEFAULT_STACK,
                                           pxPdrTemperatureSensors,
                                           TOTAL_PDR_TEMPERATURE,
                                           pxPdrVoltageSensors,
                                           TOTAL_PDR_VOLTAGE,
                                           pxPdrCurrentSensors,
                                           TOTAL_PDR_CURRENT,
                                           pxPdrPowerSensors,
                                           TOTAL_PDR_POWER,
                                           pxPdrSensorNames,
                                           TOTAL_PDR_NUMERIC_ASCI_SENSORS,
                                           pucUuid ) )
                {
                    if( OK == iBMC_BindCallback( &iBmcCallback ) )
                    {
                        PLL_INF( AMC_NAME, "BMC Proxy Driver initialised and bound\r\n" );
                        ullAmcInitStatus |= AMC_CFG_BMC_INITIALISED;
                    }
                    else
                    {
                        PLL_ERR( AMC_NAME, "Error binding to BMC Proxy Driver\r\n" );
                    }
                }
                else
                {
                    PLL_ERR( AMC_NAME, "Error initialising BMC Proxy Driver\r\n" );
                    iStatus = ERROR;
                }
            }
        }
    }

    return iStatus;
}

/**
 * @brief   Initialise App layer
 */
static int iInitApp( void )
{
    int iStatus = OK;

    if( AMC_CFG_ASDM_PREREQUISITES == ( ullAmcInitStatus & AMC_CFG_ASDM_PREREQUISITES ) )
    {
        if( OK != iASDM_Initialise( PROFILE_SENSORS_NUM_SENSORS ) )
        {
            PLL_ERR( AMC_NAME, "ASDM Initialisation ERROR\r\n" );
            iStatus = ERROR;
        }
        else
        {
            ullAmcInitStatus |= AMC_CFG_ASDM_INITIALISED;
        }
    }

    if( AMC_CFG_IN_BAND_PREREQUISITES == ( ullAmcInitStatus & AMC_CFG_IN_BAND_PREREQUISITES ) )
    {
        if( OK != iIN_BAND_TELEMETRY_Initialise( HAL_RPU_SHARED_MEMORY_BASEADDR ) )
        {
            PLL_ERR( AMC_NAME, "In Band Telemetry Initialisation ERROR\r\n" );
            iStatus = ERROR;
        }
        else
        {
            ullAmcInitStatus |= AMC_CFG_IN_BAND_INITIALISED;
            PLL_INF( AMC_NAME, "In-band service: ready\r\n" );
        }
    }

    if( AMC_CFG_OUT_OF_BAND_PREREQUISITES == ( ullAmcInitStatus & AMC_CFG_OUT_OF_BAND_PREREQUISITES ) )
    {
        if( OK != iOUT_OF_BAND_TELEMETRY_Initialise() )
        {
            PLL_ERR( AMC_NAME, "Out of Band Telemetry Initialisation ERROR\r\n" );
            iStatus = ERROR;
        }
        else
        {
            ullAmcInitStatus |= AMC_CFG_OUT_OF_BAND_INITIALISED;
            PLL_INF( AMC_NAME, "Out-of-band service: ready\r\n" );
        }
    }

    if( OK != iBIM_Initialise( PROFILE_BIM_MODULE_DATA ) )
    {
        PLL_ERR( AMC_NAME, "Built in Monitoring Initialisation ERROR\r\n" );
        iStatus = ERROR;
    }
    else
    {
        PLL_LOG( AMC_NAME, "Built in Monitoring (BIM) application started\r\n" );
    }

    return iStatus;
}

/**
 * @brief   Initialise Debug monitoring
 */
static int iInitDebug( void )
{
    int iStatus = OK;

#ifdef DEBUG_BUILD
    if( OK != iDAL_Initialise( "AMC_Debug", AMC_TASK_PRIO_DEFAULT, AMC_TASK_DEFAULT_STACK, &vGetProjectInfo ) )
    {
        PLL_ERR( AMC_NAME, "Unable to initialise debug menu\r\n" );
        iStatus = ERROR;
    }
    else
    {
        vDebugMenu_Initialise();
        PLL_INF( AMC_NAME, "Debug initialised\r\n" );
    }
#endif

    return iStatus;
}

/**
 * @brief   PLM Get image UUID
 */
static int iPlmGetUid(uint32_t* uuid)
{
    static XMailbox MailboxInstance;

    int iStatus = XST_SUCCESS;
    XLoader_ImageInfo ImageInfo;

    iStatus = XMailbox_Initialize(&MailboxInstance, XPAR_XIPIPSU_0_BASEADDR);
	if (XST_SUCCESS == iStatus)
    {
        iStatus = XLoader_ClientInit(&XLoaderInstance, &MailboxInstance);
        if (XST_SUCCESS == iStatus)
        {
            iStatus = XLoader_GetImageInfo(&XLoaderInstance, PL_NODE_ID, &ImageInfo);
            if (XST_SUCCESS == iStatus)
            {
                *uuid = ImageInfo.UID;
            }
        }
	}

    if (XST_SUCCESS != iStatus)
    {
        PLL_ERR( AMC_NAME, "PLM UUID Read Failed =%d uuid=%x", iStatus, ImageInfo.UID);
    }

    return iStatus;
}

/**
 * @brief   Configure the shared memory table stored at the start of
 *          shared memory and used by the AMI to determine the AMC state
 */
static void vConfigureSharedMemTable( void )
{
    HALShmTable xShmTable = { 0 };
    uint8_t *pucDestAdd = NULL;
    uint32_t uuid = 0;

    xShmTable.ulMagicNum                = HAL_SHARED_MEM_TABLE_MAGIC_NO;
    xShmTable.xRingBuf.ulRingBufOffset  = HAL_SHARED_MEM_TABLE_SIZE;
    xShmTable.xRingBuf.ulRingBufLen     = HAL_RPU_RING_BUFFER_LEN;
    xShmTable.xStatus.ulStatusOff       = HAL_SHARED_MEM_TABLE_SIZE + HAL_RPU_RING_BUFFER_LEN;
    xShmTable.xStatus.ulStatusLen       = sizeof( uint32_t );
    xShmTable.xUuid.ulUuidOff           = xShmTable.xStatus.ulStatusOff + xShmTable.xStatus.ulStatusLen;
    xShmTable.xUuid.ulUuidLen           = HAL_UUID_SIZE;
    xShmTable.xLogMsg.ulLogMsgIndex     = 0;
    xShmTable.xLogMsg.ulLogMsgBufOffset = xShmTable.xUuid.ulUuidOff + xShmTable.xUuid.ulUuidLen;
    xShmTable.xLogMsg.ulLogMsgBufLen    = PLL_LOG_BUF_LEN;
    xShmTable.xData.ulDataStart         = xShmTable.xLogMsg.ulLogMsgBufOffset +
                                          xShmTable.xLogMsg.ulLogMsgBufLen;
    xShmTable.xData.ulDataEnd           = HAL_RPU_SHARED_MEMORY_SIZE;

    /* Copy the populated table into the start of shared memory */
    pucDestAdd = ( uint8_t* )( HAL_RPU_SHARED_MEMORY_BASEADDR );
    pvOSAL_MemCpy( pucDestAdd, ( uint8_t* )&xShmTable, sizeof( xShmTable ) );
    HAL_FLUSH_CACHE_DATA( HAL_RPU_SHARED_MEMORY_BASEADDR, sizeof( xShmTable ) );

    /* Flush stale logs */
    pvOSAL_MemSet( ( uint8_t* )( HAL_RPU_SHARED_MEMORY_BASEADDR + xShmTable.xLogMsg.ulLogMsgBufOffset ),
                    0,
                    xShmTable.xLogMsg.ulLogMsgBufLen );
    HAL_FLUSH_CACHE_DATA( HAL_RPU_SHARED_MEMORY_BASEADDR + xShmTable.xLogMsg.ulLogMsgBufOffset,
                            xShmTable.xLogMsg.ulLogMsgBufLen );

    /*
     * AMI is waiting for the status to be set to a value of 0x1, currently we have no
     * concept of stopping/starting the AMC so once initialised this will always be valid
     */
    pucDestAdd = ( uint8_t* )( HAL_RPU_SHARED_MEMORY_BASEADDR + xShmTable.xStatus.ulStatusOff );
    pvOSAL_MemSet( pucDestAdd, HAL_ENABLE_AMI_COMMS, xShmTable.xStatus.ulStatusLen );
    HAL_FLUSH_CACHE_DATA( ( HAL_RPU_SHARED_MEMORY_BASEADDR + xShmTable.xStatus.ulStatusOff ),
                          xShmTable.xStatus.ulStatusLen );

    /*
     * UUID
     */
    iPlmGetUid(&uuid);
#ifdef XPAR_UUID_REGISTER_0_BASEADDR
    Xil_Out32(XPAR_UUID_REGISTER_0_BASEADDR, uuid);
    Xil_DCacheFlushRange(XPAR_UUID_REGISTER_0_BASEADDR, sizeof(uint32_t));
#endif
    pucDestAdd = ( uint8_t* )( HAL_RPU_SHARED_MEMORY_BASEADDR + xShmTable.xUuid.ulUuidOff );
    pvOSAL_MemSet( pucDestAdd, 0, xShmTable.xUuid.ulUuidLen );
    pvOSAL_MemCpy( pucDestAdd, &uuid, sizeof(uuid) );
    HAL_FLUSH_CACHE_DATA( ( HAL_RPU_SHARED_MEMORY_BASEADDR + xShmTable.xUuid.ulUuidOff ),
                          xShmTable.xUuid.ulUuidLen );
}

/**
 * @brief   Load PDI from OSPI flash on power-up using PLM
 *
 * This function loads a PDI image from a specified FPT partition in OSPI flash
 * during system power-up. It uses the XLoader_LoadPartialPdi function from PLM
 * with XLOADER_PDI_OSPI as the source.
 */
static int iLoadPdiOnPowerUp( void )
{
    int iStatus = ERROR;
    u32 PdiLoadStatus = 0U;
    APCProxyDriverFptHeader xFptHeader = { 0 };
    APCProxyDriverFptPartition xFptPartition = { 0 };
    uint64_t ullPdiAddr = 0;

    PLL_INF( AMC_NAME, "Loading PDI from OSPI flash on power-up...\r\n" );
    PLL_INF( AMC_NAME, "Boot device: %d, Partition: %d\r\n",
             USER_PDI_POWERUP_BOOT_DEVICE, USER_PDI_POWERUP_PARTITION );

    /* Verify XLoader instance is valid */
    if ( NULL == XLoaderInstance.MailboxPtr )
    {
        PLL_ERR( AMC_NAME, "Error: XLoader instance not initialized\r\n" );
        goto fail;
    }

    /* Get FPT header to validate partition exists */
    if ( OK != iAPC_GetFptHeader( ( APC_BOOT_DEVICES )USER_PDI_POWERUP_BOOT_DEVICE, &xFptHeader ) )
    {
        PLL_ERR( AMC_NAME, "Error: Failed to get FPT header\r\n" );
        goto fail;
    }

    /* Validate partition index */
    if ( USER_PDI_POWERUP_PARTITION >= xFptHeader.ucNumEntries )
    {
        PLL_ERR( AMC_NAME, "Error: Invalid partition %d (max %d)\r\n",
                USER_PDI_POWERUP_PARTITION, xFptHeader.ucNumEntries );
        goto fail;
    }

    /* Get partition info to get PDI address */
    if ( OK != iAPC_GetFptPartition( ( APC_BOOT_DEVICES )USER_PDI_POWERUP_BOOT_DEVICE,
                                    USER_PDI_POWERUP_PARTITION,
                                    &xFptPartition ) )
    {
        PLL_ERR( AMC_NAME, "Error: Failed to get FPT partition info\r\n" );
        goto fail;
    }

    /* Is power up load flag set? */
    if ( ( xFptPartition.user.powerup_flag ) == 0U )
    {
        PLL_INF( AMC_NAME, "Power load user partition : disabled\r\n");
        ulUserPdiLoadStatus = USER_PDI_POWERUP_NOT_LOADED;
        iStatus = OK;
        goto fail;
    }

    /* Get PDI address from partition base address */
    ullPdiAddr = ( uint64_t )xFptPartition.ulPartitionBaseAddr;

    PLL_INF( AMC_NAME, "PDI address: 0x%llx, Size: 0x%x bytes\r\n",
           ullPdiAddr, xFptPartition.ulPartitionSize );

    /* Load PDI directly from OSPI flash using PLM */
    if (XST_SUCCESS != XLoader_LoadPartialPdi( &XLoaderInstance,
                                    XLOADER_PDI_OSPI,
                                    ullPdiAddr,
                                    &PdiLoadStatus ))
    {
        PLL_ERR( AMC_NAME, "Error: Failed to load PDI\r\n" );
    }
    else
    {
        iStatus = OK;
        ulUserPdiLoadStatus = USER_PDI_POWERUP_LOADED;
    }
    iOSAL_Task_SleepMs( AMC_TASK_SLEEP_MS );

fail:
    if ( OK != iStatus )
    {
        PLL_ERR( AMC_NAME, "Error: PDI load failed, status 0x%X, PLM status 0x%X\r\n",
                iStatus, PdiLoadStatus );
        ulUserPdiLoadStatus = USER_PDI_POWERUP_ERROR;
    }

    return iStatus;
}
