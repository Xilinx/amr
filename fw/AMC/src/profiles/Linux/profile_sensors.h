/**
 * Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the sensors profile for the Linux platform
 *
 * @file profile_sensors.h
 */

#ifndef _PROFILES_SENSORS_H_
#define _PROFILES_SENSORS_H_

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include "asc_proxy_driver.h"
#include "sys_mon.h"
#include "profile_pdr.h"


/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/

#define PROFILE_SENSORS_NUM_SENSORS ( 5 )


/******************************************************************************/
/* Local Function implementations                                             */
/******************************************************************************/

/**
 * @brief   Wrapper for the iSYS_MON_ReadTemperature function, to keep it the standard driver API format
 *
 * @param   unused1     Unused parameter (normally i2c bus)
 * @param   unused2     Unused parameter (normally i2c address)
 * @param   unused3     Unused parameter (normally i2c channel)
 * @param   pfValue     Pointer to latest sensor value
 *
 * @return  The return value of iSYS_MON_ReadTemperature
 *
 * @note    No sanity checks, etc, are done - this function is solely a wrapper API
 */
static inline int iSYS_MON_WrappedReadTemperature( uint8_t unused1, uint8_t unused2, uint8_t unused3, float *pfValue )
{
    return iSYS_MON_ReadTemperature( pfValue );
}

/**
 * @brief   Wrapper for the iAXC_GetTemperature function, to keep it the standard driver API format
 *
 * @param   unused1      Unused parameter (normally i2c bus)
 * @param   unused2      Unused parameter (normally i2c address)
 * @param   ucChannelNum Channel Num, used to specify QSFP ID
 * @param   pfValue      Pointer to latest sensor value
 *
 * @return  OK           Data retrieved from proxy driver successfully
 *          ERROR        Data not retrieved successfully
 *
 * @note    No sanity checks, etc, are done - this function is solely a wrapper API
 */
static inline int iAXC_WrappedGetTemperature( uint8_t unused1, uint8_t unused2, uint8_t ucChannelNum, float *pfValue )
{
    return iAXC_GetTemperature( ucChannelNum, pfValue );
}

/**
 * @brief   Function pointer called in profile to enable sensors
 *
 * @return  TRUE to indictate sensor is enabled
 */
static inline int iSensorIsEnabled( void )
{
    return TRUE;
}


/******************************************************************************/
/* Public variables                                                           */
/******************************************************************************/

ASC_PROXY_DRIVER_SENSOR_DATA PROFILE_SENSORS_SENSOR_DATA[ PROFILE_SENSORS_NUM_SENSORS ] =
{
    { "FPGA_Temp", FPGA_DEVICE_ID, ASC_PROXY_DRIVER_SENSOR_BITFIELD_TEMPERATURE, FALSE, 0,
      { 0, ASC_SENSOR_I2C_BUS_INVALID, ASC_SENSOR_I2C_BUS_INVALID, ASC_SENSOR_I2C_BUS_INVALID }, iSensorIsEnabled,
      { iSYS_MON_WrappedReadTemperature, NULL, NULL, NULL },
      {
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE } },
      ASC_PROXY_DRIVER_SENSOR_THRESHOLD_STATUS_HEALTHY
    },
    { "Module_0", QSFP_MODULE_0_DEVICE_ID, ASC_PROXY_DRIVER_SENSOR_BITFIELD_TEMPERATURE, FALSE, 0,
      { 0, ASC_SENSOR_I2C_BUS_INVALID, ASC_SENSOR_I2C_BUS_INVALID, ASC_SENSOR_I2C_BUS_INVALID }, iSensorIsEnabled,
      { iAXC_WrappedGetTemperature, NULL, NULL, NULL },
      {
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE } },
      ASC_PROXY_DRIVER_SENSOR_THRESHOLD_STATUS_HEALTHY
    },
    { "Module_1", QSFP_MODULE_1_DEVICE_ID, ASC_PROXY_DRIVER_SENSOR_BITFIELD_TEMPERATURE, FALSE, 0,
      { 1, ASC_SENSOR_I2C_BUS_INVALID, ASC_SENSOR_I2C_BUS_INVALID, ASC_SENSOR_I2C_BUS_INVALID }, iSensorIsEnabled,
      { iAXC_WrappedGetTemperature, NULL, NULL, NULL },
      {
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE } },
      ASC_PROXY_DRIVER_SENSOR_THRESHOLD_STATUS_HEALTHY
    },
    { "Module_2", QSFP_MODULE_2_DEVICE_ID, ASC_PROXY_DRIVER_SENSOR_BITFIELD_TEMPERATURE, FALSE, 0,
      { 2, ASC_SENSOR_I2C_BUS_INVALID, ASC_SENSOR_I2C_BUS_INVALID, ASC_SENSOR_I2C_BUS_INVALID }, iSensorIsEnabled,
      { iAXC_WrappedGetTemperature, NULL, NULL, NULL },
      {
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE } },
      ASC_PROXY_DRIVER_SENSOR_THRESHOLD_STATUS_HEALTHY
    },
    { "Module_3", QSFP_MODULE_3_DEVICE_ID, ASC_PROXY_DRIVER_SENSOR_BITFIELD_TEMPERATURE, FALSE, 0,
      { 3, ASC_SENSOR_I2C_BUS_INVALID, ASC_SENSOR_I2C_BUS_INVALID, ASC_SENSOR_I2C_BUS_INVALID }, iSensorIsEnabled,
      { iAXC_WrappedGetTemperature, NULL, NULL, NULL },
      {
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE },
          { 0, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL,
            ASC_SENSOR_INVALID_VAL, ASC_SENSOR_INVALID_VAL, 0, 0, ASC_PROXY_DRIVER_SENSOR_STATUS_NOT_PRESENT,
            ASC_PROXY_DRIVER_SENSOR_OPERATIONAL_STATUS_ENABLED,  ASC_PROXY_DRIVER_SENSOR_UNIT_MOD_NONE } },
      ASC_PROXY_DRIVER_SENSOR_THRESHOLD_STATUS_HEALTHY
    }
};

#endif /* _PROFILES_SENSORS_H_ */
