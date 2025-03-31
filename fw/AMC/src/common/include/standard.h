/**
 * Copyright (c) 2023 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the standard definitions for the AMC
 *
 * @file standard.h
 */

#ifndef _STANDARD_H_
#define _STANDARD_H_

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/

#ifndef OK
#define OK          ( 0 )
#endif

#ifndef ERROR
#define ERROR       ( -1 )
#endif

#ifndef TRUE
#define TRUE        ( 1 )
#endif

#ifndef FALSE
#define FALSE       ( 0 )
#endif

#ifndef UNUSED
#define UNUSED( p ) ( void )( p )
#endif

/**
 * @enum   MODULE_STATE
 *
 * @brief  Module states
 */
typedef enum MODULE_STATE
{
	MODULE_STATE_OK = 0,
	MODULE_STATE_UNINITIALISED,
	MODULE_STATE_ERROR,

	MAX_MODULE_STATE

} MODULE_STATE;

#endif /* _STANDARD_H_ */
