/**
 * Copyright (c) 2023 - 2024 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This header file contains the function declarations for the System Monitor
 * sensor.
 *
 * @file sys_mon.h
 *
 */

#ifndef _SYS_MON_H_
#define _SYS_MON_H_

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include "standard.h"


/******************************************************************************/
/* Enums                                                                      */
/******************************************************************************/

/**
 * @enum    SYS_MON_VOLTAGES_ENUM
 * @brief   Enumeration of available SYS_MON voltage values
 */
typedef enum SYS_MON_VOLTAGES_ENUM
{
#ifdef PROFILE_RAVE
    SYS_MON_VOLTAGES_VCCAUX = 0,
    SYS_MON_VOLTAGES_VCCSOC,
    SYS_MON_VOLTAGES_VCCO302,
    SYS_MON_VOLTAGES_VCCAUXPMC,
    SYS_MON_VOLTAGES_VCCO500,
    SYS_MON_VOLTAGES_VCCPMC,
    SYS_MON_VOLTAGES_VCCPSFP,
    SYS_MON_VOLTAGES_VCCPSLP,
    SYS_MON_VOLTAGES_VPVN,
    SYS_MON_VOLTAGES_VCCO703,
    SYS_MON_VOLTAGES_VAUXCH0,
    SYS_MON_VOLTAGES_VCCAUXSMON,
#else
    SYS_MON_VOLTAGES_VCCAUX = 0,
    SYS_MON_VOLTAGES_VCCAUXSMON,
    SYS_MON_VOLTAGES_VCCAUXPMC,
#endif
    MAX_SYS_MON_VOLTAGE
} SYS_MON_VOLTAGES_ENUM;


/******************************************************************************/
/* Function declarations                                                      */
/******************************************************************************/

/**
 * @brief   Initialise System Monitor
 *
 * @return  OK                  SYS_MON was initialised successfully
 *          ERROR               SYS_MON was not initialised
 * 
 */
int iSYS_MON_Initialise( void );

/**
 * @brief   Read temperature from System Monitor
 *
 * @param   pfTemperatureInC    Pointer to temperature in deg/C
 *
 * @return  OK                  Temperature read successfully
 *          ERROR               Temperature not read successfully
 * 
 */
int iSYS_MON_ReadTemperature( float *pfTemperatureInC );

/**
 * @brief   Read voltage from System Monitor
 *
 * @param   xVoltageType        Voltage to read
 * @param   pfVoltageInMV       Pointer to voltage in milli Volts
 *
 * @return  OK                  Voltage read successfully
 *          ERROR               Voltage not read successfully
 * 
 */
int iSYS_MON_ReadVoltage( SYS_MON_VOLTAGES_ENUM xVoltageType, float *pfVoltageInMV );

/**
 * @brief   Print all the stats gathered by the driver
 *
 * @return  OK                  Stats retrieved from driver successfully
 *          ERROR               Stats not retrieved successfully
 * 
 */
int iSYS_MON_PrintStatistics( void );

/**
 * @brief   Clear all the stats in the driver
 *
 * @return  OK                  Stats cleared successfully
 *          ERROR               Stats not cleared successfully
 * 
 */
int iSYS_MON_ClearStatistics( void );

#endif
