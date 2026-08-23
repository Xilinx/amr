/**
 * Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the public API for the Event Library
 *
 * @file evl.h
 */

#ifndef _EVL_H_
#define _EVL_H_

#include "standard.h"

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/


#ifndef EVL_LOG_LEN
#define EVL_LOG_LEN ( 100 )
#endif

#define EVL_MAX_BINDINGS ( 10 )


/******************************************************************************/
/* Typedefs and strcuts                                                       */
/******************************************************************************/

/**
 * @struct  EVLSignal
 * @brief   Single instance of an event
 */
typedef struct EVLSignal
{
    uint8_t ucModule;           /* Unique ID of the module raising the event   */
    uint8_t ucEventType;        /* Unique ID of the event raised by the module */
    uint8_t ucInstance;         /* Specific instance of the event raised       */
    /* - for optional tracking                     */
    uint8_t ucAdditionalData;   /* Additional data if required.                */

} EVLSignal;

/**
 * @brief   Event signalling callback
 *
 * @param   pxSignal    Information on event raised
 *
 * @return  OK if no errors were raised in the callback
 *          ERROR if an error was raised in the callback
 *
 */
typedef int ( EVL_CALLBACK )( EVLSignal *pxSignal );

/**
 * @struct  EVLRecord
 * @brief   Record of EVL bindings
 */
typedef struct EVLRecord EVLRecord;


/******************************************************************************/
/* Public function declarations                                               */
/******************************************************************************/

/**
 * @brief   Initalise Event Library
 *
 * @return  OK if library initialised successfully
 *          ERROR if library not initialised
 *
 */
int iEVL_Initialise( void );

/**
 * @brief Initialise Event Library Record
 *
 * @param ppxRecord  Record to be initialised
 *
 * @return OK if record initialised successfully
 *         ERROR if record not initialised
 */
int iEVL_CreateRecord( EVLRecord **ppxRecord );

/**
 * @brief   Bind a callback into a module
 *
 * @param   pxRecord        Record to bind callback to
 * @param   pxNewCallback   New callback to bind
 *
 * @return  OK if callback bound successfully
 *          ERROR if callback not bound
 *
 * @note    Only EVL_MAX_BINDINGS may be bound to a single record
 */
int iEVL_BindCallback( EVLRecord *pxRecord, EVL_CALLBACK *pxNewCallback );

/**
 * @brief   Raise an event to each bound-in callback
 *
 * @param   pxRecord        Record of bindings
 * @param   pxSignal        Innstance of event being raised
 *
 * @return  OR if no errors returned from the callbacks
 *          ERROR if an error was returned by the callback
 *
 */
int iEVL_RaiseEvent( EVLRecord *pxRecord, EVLSignal *pxSignal );

/**
 * @brief   Get event stats
 *
 * @param   pxRecord        Record of bindings
 *
 * @return  OR if the stats were retrieved
 *          ERROR if the stats could not be retrieved
 *
 */
int iEVL_GetStats( EVLRecord *pxRecord );

/**
 * @brief   Print all the stats gathered by the library
 *
 * @return  OK          Stats retrieved from library successfully
 *          ERROR       Stats not retrieved successfully
 *
 */
int iEVL_PrintStatistics( void );

/**
 * @brief   Clear all the stats in the library
 *
 * @return  OK          Stats cleared successfully
 *          ERROR       Stats not cleared successfully
 *
 */
int iEVL_ClearStatistics( void );

/**
 * @brief   Retrieve the event log
 *
 * @return  OK          Log retrieved successfully
 *          ERROR       Log not retrieved
 */
int iEVL_PrintLog( void );

/**
 * @brief   Toggle verbosity of EVL, TRUE or FALSE
 */
void vEVL_SetVerbosity( int iVerbosity );

#endif /* _EVL_H_ */
