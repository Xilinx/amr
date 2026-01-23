/**
 * Copyright (c) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This file contains the utility definitions for the AMC
 *
 * @file util.h
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "standard.h"
#include "osal.h"


/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/
#define UTIL_MAX_UINT8      ( 0xFF )
#define UTIL_MAX_UINT16     ( 0xFFFF )
#define UTIL_MAX_UINT32     ( 0xFFFFFFFF )

#define UTIL_100KHZ         ( 100 * 1000 )
#define UTIL_400KHZ         ( 400 * 1000 )
#define UTIL_1MHZ           ( 1000 * 1000 )

#define UPPER_FIREWALL      ( 0xBABECAFE )
#define LOWER_FIREWALL      ( 0xDEADFACE )

#define UTIL_ELAPSED_TIME_MS( x ) ( ulOSAL_GetUptimeMs() - x );

#define CHECK_RANGE( val, min,  max ) \
    ( ( val ) >= ( min ) && ( val ) < ( max ) && ( min ) < ( max ) ? TRUE : FALSE )

#define ARRAY_LEN( a )      ( sizeof( a ) / sizeof( a[ 0 ] ) )

#define STATIC_ASSERT( x )  _Static_assert( ( x ), "(" #x ") failed" )

#define MIN( x, y )         ( ( ( x ) < ( y ) ) ? ( x ) : ( y ) )
#define MAX( x, y )         ( ( ( x ) > ( y ) ) ? ( x ) : ( y ) )

#define UTIL_MAKE_ENUM( VAR )    VAR,
#define UTIL_MAKE_STRINGS( VAR ) #VAR,
#define UTIL_MAKE_ENUM_AND_STRINGS( source, enumName, enumStringName )      \
    enum enumName { source( UTIL_MAKE_ENUM ) };                             \
    const char *const enumStringName[] = {  source( UTIL_MAKE_STRINGS ) };

#endif /* _UTIL_H_ */
