/**
 * Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the profile debug menu for the Linux
 *
 * @file profile_debug_menu.c
 */

/*****************************************************************************/
/* Includes                                                                  */
/*****************************************************************************/

#include "profile_debug_menu.h"
#include "profile_muxed_device.h"
#include "profile_hal.h"
#include "profile_fal.h"
#include "dal.h"

/* debug includes */
#include "osal_debug.h"
#include "pll_debug.h"
#include "dal_debug.h"
#include "evl_debug.h"
#include "eeprom_debug.h"
#include "sys_mon_debug.h"
#include "i2c_debug.h"
#include "ami_proxy_driver_debug.h"
#include "apc_proxy_driver_debug.h"
#include "axc_proxy_driver_debug.h"
#include "asc_proxy_driver_debug.h"
#include "bmc_proxy_driver_debug.h"
#include "asdm_debug.h"
#include "in_band_telemetry_debug.h"
#include "bim_debug.h"


/******************************************************************************/
/* Public Function Implementations                                            */
/******************************************************************************/

/**
 * @brief   Initialise Debug Menu
 */
void vDebugMenu_Initialise()
{
    /* top level directories */
    static DAL_HDL pxDeviceDrivers   = NULL;
    static DAL_HDL pxCoreLibsTop     = NULL;
    static DAL_HDL pxProxyDriversTop = NULL;
    static DAL_HDL pxAppsTop         = NULL;

    /* osal */
    vOSAL_DebugInit();

    /* device drivers */
    pxDeviceDrivers = pxDAL_NewDirectory( "device_drivers" );

    vSYS_MON_DebugInit( pxDeviceDrivers );
    vEeprom_DebugInit( pxDeviceDrivers );
    vI2C_DebugInit( pxDeviceDrivers );
    /* core libraries */
    pxCoreLibsTop = pxDAL_NewDirectory( "core_libs" );

    vPLL_DebugInit( pxCoreLibsTop );
    vDAL_DebugInit( pxCoreLibsTop );
    vEVL_DebugInit( pxCoreLibsTop );

    vFAL_DebugInitialise();

    /* proxy drivers */
    pxProxyDriversTop = pxDAL_NewDirectory( "proxy_drivers" );

    vAMI_DebugInit( pxProxyDriversTop );
    vAPC_DebugInit( pxProxyDriversTop );
    vASC_DebugInit( pxProxyDriversTop );
    if( 0 != MAX_NUM_EXTERNAL_DEVICES_AVAILABLE )
    {
        vAXC_DebugInit( pxProxyDriversTop );
    }
    vBMC_DebugInit( pxProxyDriversTop );

    /* apps */
    pxAppsTop = pxDAL_NewDirectory( "apps" );

    vASDM_DebugInit( pxAppsTop );
    vIN_BAND_TELEMETRY_DebugInit( pxAppsTop, HAL_RPU_SHARED_MEMORY_BASE_ADDR );
    vBIM_DebugInit( pxAppsTop );
}
