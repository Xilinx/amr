/**
 * Copyright (c) 2024 - 2025 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This implenents the functions for accessing the
 * manufacturing eeprom.
 *
 * @file eeprom.c
 */

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "util.h"
#include "pll.h"
#include "osal.h"

#include "eeprom.h"
#include "i2c.h"


/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/
#define EEPROM_ASCII_VAR( x ) ( ( x ) + 1 )
#define EEPROM_NAME                       "EEPROM"
#define EEPROM_2_BYTE_ADDRESS             (  2U )
#define EEPROM_ADDRESS_SIZE_UNINITIALISED (  0U )
#define EEPROM_WRITE_DELAY_MS             ( 10U )
#define EEPROM_ADDRESS_BYTE_ZERO          (  0U )
#define EEPROM_ADDRESS_BYTE_ONE           (  1U )
#define EEPROM_DATA_SINGLE_BYTE           (  1U )
#define EEPROM_ONE_BYTE                   (  1U )
#define EEPROM_TWO_BYTES                  (  2U )
#define UPPER_FIREWALL                    ( 0xBABECAFEU )
#define LOWER_FIREWALL                    ( 0xDEADFACEU )
/* Current EEPROM versions supported */
#define EEPROM_V1_0                      ( 0x01U )

#define EEPROM_MAX_MAC                   (  40U )
#define EEPROM_PAGE_SIZE_MAX             ( 255U )
#define EEPROM_ADDRESS_SIZE_MAX          ( 255U )
#define EEPROM_WRITE_MULTI_BYTE_SIZE_MAX ( 255U )
#define EEPROM_WRITE_BYTE_SIZE_MAX       ( 255U )

/* Default register content in EEPROM if a particular register has not been programmed */
#define EEPROM_DEFAULT_VAL               ( 0xFFU )

#define EEPROM_VERSION_OFFSET            ( 0x00U )
#define EEPROM_VERSION_SIZE              (    1U )
#define EEPROM_BUF_SIZE                  (  128U )
#define EEPROM_FIELD_NA_SIZE             (    0U )              /* For non-existent fields */
#define EEPROM_DEVICE_ID_CHECK_TRY_COUNT (    3U )

/* Verbose data log - disabled by default */
/* #define EEPROM_VERBOSE_DEBUG_ENABLE */

/* Version 1.0 field positions */
#define EEPROM_V1_0_HEADER_CHECKSUM_SIZE      ( 0x08U )
#define EEPROM_V1_0_HEADER_CHECKSUM_OFFSET    ( 0x07U )
#define EEPROM_V1_0_BOARD_CHECKSUM_SIZE       ( 0x60U )
#define EEPROM_V1_0_BOARD_LENGTH_OFFSET       ( 0x09U )
#define EEPROM_V1_0_PRODUCT_NAME_OFFSET       ( 0x16U )         /* Board Product Name  */
#define EEPROM_V1_0_PRODUCT_NAME_SIZE         (   16U )
#define EEPROM_V1_0_PRODUCT_PART_NUM_OFFSET   ( 0x38U )         /* Board Part Number  */
#define EEPROM_V1_0_PRODUCT_PART_NUM_SIZE     (    9U )
#define EEPROM_V1_0_MFG_PART_NUM_OFFSET       ( 0x38U )         /* Manufacturing Part Number */
#define EEPROM_V1_0_MFG_PART_NUM_SIZE         (    9U )
#define EEPROM_V1_0_MFG_PART_REV_OFFSET       ( 0x44U )         /* Manufacturing Part Revision */
#define EEPROM_V1_0_MFG_PART_REV_SIZE         (    8U )
#define EEPROM_V1_0_PRODUCT_SERIAL_OFFSET     ( 0x27U )         /* Product Serial Number */
#define EEPROM_V1_0_PRODUCT_SERIAL_SIZE       (   16U )
#define EEPROM_V1_0_MFG_DATE_OFFSET           ( 0x0BU )         /* Manufacturing Date  */
#define EEPROM_V1_0_MFG_DATE_SIZE             (    3U )
#define EEPROM_V1_0_TOT_MAC_ID_OFFSET         ( 0x7CU )         /* Number of MAC IDs Length */
#define EEPROM_V1_0_TOT_MAC_ID_SIZE           (    1U )
#define EEPROM_V1_0_MAC_OFFSET                ( 0x83U )         /* MAC ID 1 */
#define EEPROM_V1_0_MAC_SIZE                  (    6U )
#define EEPROM_V1_0_UUID_OFFSET               ( 0x56U )         /* UUID */
#define EEPROM_V1_0_UUID_SIZE                 (   16U )
#define EEPROM_V1_0_CHECKSUM_START            (    6U )
#define EEPROM_V1_0_CHECKSUM_END              (  127U )

STATIC_ASSERT( EEPROM_VERSION_SIZE               < EEPROM_MAX_FIELD_SIZE );
STATIC_ASSERT( EEPROM_V1_0_PRODUCT_NAME_SIZE     < EEPROM_MAX_FIELD_SIZE );
STATIC_ASSERT( EEPROM_V1_0_PRODUCT_PART_NUM_SIZE < EEPROM_MAX_FIELD_SIZE );
STATIC_ASSERT( EEPROM_V1_0_MFG_PART_NUM_SIZE     < EEPROM_MAX_FIELD_SIZE );
STATIC_ASSERT( EEPROM_V1_0_MFG_PART_REV_SIZE     < EEPROM_MAX_FIELD_SIZE );
STATIC_ASSERT( EEPROM_V1_0_PRODUCT_SERIAL_SIZE   < EEPROM_MAX_FIELD_SIZE );
STATIC_ASSERT( EEPROM_V1_0_MFG_DATE_SIZE         < EEPROM_MAX_FIELD_SIZE );
STATIC_ASSERT( EEPROM_V1_0_TOT_MAC_ID_SIZE       < EEPROM_MAX_FIELD_SIZE );
STATIC_ASSERT( EEPROM_V1_0_MAC_SIZE              < EEPROM_MAX_FIELD_SIZE );
STATIC_ASSERT( EEPROM_V1_0_UUID_SIZE             < EEPROM_MAX_FIELD_SIZE );


#define EEPROM_STATS( DO )                  \
	DO( EEPROM_STATS_INITIALISATION )   \
	DO( EEPROM_STATS_CHECKSUM )         \
	DO( EEPROM_STATS_READ_FIELD )       \
	DO( EEPROM_STAT_SINGLE_BYTE_READ )  \
	DO( EEPROM_STAT_MULTI_BYTE_READ )   \
	DO( EEPROM_STAT_SINGLE_BYTE_WRITE ) \
	DO( EEPROM_STAT_MULTI_BYTE_WRITE )  \
	DO( EEPROM_STATS_VERIFY_DEVICE_ID ) \
	DO( EEPROM_STATS_MAX )

#define EEPROM_ERROR( DO )                   \
	DO( EEPROM_ERROR_INITIALISATION )    \
	DO( EEPROM_ERROR_SINGLE_BYTE_READ )  \
	DO( EEPROM_ERROR_MULTI_BYTE_READ )   \
	DO( EEPROM_ERROR_SINGLE_BYTE_WRITE ) \
	DO( EEPROM_ERROR_MULTI_BYTE_WRITE )  \
	DO( EEPROM_ERROR_INVALID_VERSION )   \
	DO( EEPROM_ERROR_INCORRECT_VERSION ) \
	DO( EEPROM_ERROR_CHECKSUM )          \
	DO( EEPROM_ERROR_READ_FIELD )        \
	DO( EEPROM_ERROR_VALIDATION )        \
	DO( EEPROM_ERRORS_DEVICE_ID_READ )   \
	DO( EEPROM_ERRORS_VERIFY_DEVICE_ID ) \
	DO( EEPROM_ERROR_MAX )

#define PRINT_STAT( x )       PLL_INF( EEPROM_NAME,           \
				       "%30s. . . .%d\r\n",   \
				       EEPROM_STATS_STR[ x ], \
				       pxThis->pulStatCounters[ x ] )
#define PRINT_ERROR_STAT( x ) PLL_INF( EEPROM_NAME,           \
				       "%30s. . . .%d\r\n",   \
				       EEPROM_ERROR_STR[ x ], \
				       pxThis->pulStatErrorCounters[ x ] )

#define INC_STAT_COUNTER( x )  { if( x < EEPROM_STATS_MAX ) pxThis->pulStatCounters[ x ]++; }
#define INC_ERROR_COUNTER( x ) { if( x < EEPROM_ERROR_MAX ) pxThis->pulStatErrorCounters[ x ]++; }


/******************************************************************************/
/* Enums                                                                      */
/******************************************************************************/

/**
 * @enum    EEPROM_STATS
 * @brief   Enumeration of stats counters for this driver
 */
UTIL_MAKE_ENUM_AND_STRINGS( EEPROM_STATS, EEPROM_STATS, EEPROM_STATS_STR )

/**
 * @enum    EEPROM_ERROR
 * @brief   Enumeration of stats errors for this driver
 */
UTIL_MAKE_ENUM_AND_STRINGS( EEPROM_ERROR, EEPROM_ERROR, EEPROM_ERROR_STR )


/******************************************************************************/
/* Structs                                                                    */
/******************************************************************************/
/**
 * @struct  EEPROM_V1_0_BOARDINFO
 * @brief   Structure to hold the fields for version 4.0 EEPROM data
 */
typedef struct EEPROM_V1_0_BOARDINFO
{
	uint8_t ucEepromVersion[ EEPROM_ASCII_VAR( EEPROM_VERSION_SIZE ) ];
	uint8_t ucProductName[ EEPROM_ASCII_VAR( EEPROM_V1_0_PRODUCT_NAME_SIZE ) ];
	uint8_t ucPartNumber[ EEPROM_ASCII_VAR( EEPROM_V1_0_PRODUCT_PART_NUM_OFFSET ) ];
	uint8_t ucMfgPartNumber[ EEPROM_ASCII_VAR( EEPROM_V1_0_MFG_PART_NUM_SIZE ) ];
	uint8_t ucMfgPartRevision[ EEPROM_ASCII_VAR( EEPROM_V1_0_MFG_PART_REV_SIZE ) ];
	uint8_t ucProductSerial[ EEPROM_ASCII_VAR( EEPROM_V1_0_PRODUCT_SERIAL_SIZE ) ];
	uint8_t ucMfgDate[ EEPROM_ASCII_VAR( EEPROM_V1_0_MFG_DATE_SIZE ) ];
	uint8_t ucNumMacIds;
	uint8_t ucMac[ EEPROM_ASCII_VAR( EEPROM_V1_0_MAC_SIZE ) ];
	uint8_t ucUuid[ EEPROM_ASCII_VAR( EEPROM_V1_0_UUID_SIZE ) ];

} EEPROM_V1_0_BOARDINFO;


/**
 * @struct  EEPROM_BOARDINFO
 * @brief   Structure to hold the fields for EEPROM data, version depends on the product
 */
typedef union EEPROM_BOARDINFO
{
	EEPROM_V1_0_BOARDINFO xBoardInfoV1_0;

} EEPROM_BOARDINFO;

/**
 * @struct  EEPROM_PRIVATE_DATA
 * @brief   Structure to hold ths driver's private data
 */
typedef struct EEPROM_PRIVATE_DATA
{
	uint32_t ulUpperFirewall;
	int iEepromInitialised;
	EEPROM_CFG xEepromCfg;
	EEPROM_VERSION xEepromExpectedVersion;
	EEPROM_VERSION xEepromActualVersion;
	union EEPROM_BOARDINFO xBoardInfo;
	uint8_t                *pucEepromVersion;
	uint8_t                *pucProductName;
	uint8_t                *pucBoardRev;
	uint8_t                *pucBoardSerial;
	uint8_t ucNumMacIds;
	uint8_t                *pucBoardMac;
	uint8_t                *pucBoardActivePassive;
	uint8_t                *pucBoardConfigMode;
	uint8_t                *pucBoardMfgDate;
	uint8_t                *pucBoardPartNum;
	uint8_t                *pucBoardUuid;
	uint8_t                *pucBoardPcieInfo;
	uint8_t                *pucBoardMaxPowerMode;
	uint8_t                *pucMemorySize;
	uint8_t                *pucOemId;
	uint8_t                *pucCapability;
	uint8_t                *pucMfgPartNum;

	uint8_t ucSizeEepromVersion;
	uint8_t ucSizeProductName;
	uint8_t ucSizeBoardRev;
	uint8_t ucSizeBoardSerial;
	uint8_t ucSizeumMacIds;
	uint8_t ucSizeBoardMac;
	uint8_t ucSizeBoardActivePassive;
	uint8_t ucSizeBoardConfigMode;
	uint8_t ucSizeBoardMfgDate;
	uint8_t ucSizeBoardPartNum;
	uint8_t ucSizeBoardUuid;
	uint8_t ucSizeBoardPcieInfo;
	uint8_t ucSizeBoardMaxPowerMode;
	uint8_t ucSizeMemorySize;
	uint8_t ucSizeOemId;
	uint8_t ucSizeCapability;
	uint8_t ucSizeMfgPartNum;

	uint8_t ucOffsetEepromVersion;
	uint8_t ucOffsetProductName;
	uint8_t ucOffsetBoardRev;
	uint8_t ucOffsetBoardSerial;
	uint8_t ucOffsetumMacIds;
	uint8_t ucOffsetBoardMac;
	uint8_t ucOffsetBoardActivePassive;
	uint8_t ucOffsetBoardConfigMode;
	uint8_t ucOffsetBoardMfgDate;
	uint8_t ucOffsetBoardPartNum;
	uint8_t ucOffsetBoardUuid;
	uint8_t ucOffsetBoardPcieInfo;
	uint8_t ucOffsetBoardMaxPowerMode;
	uint8_t ucOffsetMemorySize;
	uint8_t ucOffsetOemId;
	uint8_t ucOffsetCapability;
	uint8_t ucOffsetMfgPartNum;

	uint8_t ucOffsetChecksumLsb;
	uint8_t ucOffsetChecksumMsb;
	uint8_t ucChecksumStart;
	uint8_t ucChecksumEnd;

	uint32_t pulStatCounters[ EEPROM_STATS_MAX ];
	uint32_t pulStatErrorCounters[ EEPROM_ERROR_MAX ];

	uint32_t ulLowerFirewall;

} EEPROM_PRIVATE_DATA;


/******************************************************************************/
/* Local Variables                                                            */
/******************************************************************************/
static EEPROM_PRIVATE_DATA xLocalData =
{
	UPPER_FIREWALL,         /* ulUpperFirewall       */
	FALSE,                  /* iEepromInitialised    */
	{
		0
	},                      /* xEepromCfg            */
	EEPROM_VERSION_MAX,     /* xEepromExpectedVersion*/
	EEPROM_VERSION_MAX,     /* xEepromActualVersion  */
	{ { {
		    0
	    } } },              /* xBoardInfo            */
	NULL,                   /* pucEepromVersion      */
	NULL,                   /* pucProductName        */
	NULL,                   /* pucBoardRev           */
	NULL,                   /* pucBoardSerial        */
	0,                      /* ucNumMacIds           */
	NULL,                   /* pucBoardMac           */
	NULL,                   /* pucBoardActivePassive */
	NULL,                   /* pucBoardConfigMode    */
	NULL,                   /* pucBoardMfgDate       */
	NULL,                   /* pucBoardPartNum       */
	NULL,                   /* pucBoardUuid          */
	NULL,                   /* pucBoardPcieInfo      */
	NULL,                   /* pucBoardMaxPowerMode  */
	NULL,                   /* pucMemorySize         */
	NULL,                   /* pucOemId              */
	NULL,                   /* pucCapability         */
	NULL,                   /* pucMfgPartNum         */

	0,                      /* ucSizeEepromVersion       */
	0,                      /* ucSizeProductName         */
	0,                      /* ucSizeBoardRev            */
	0,                      /* ucSizeBoardSerial         */
	0,                      /* cNSizeumMacIds            */
	0,                      /* ucSizeBoardMac            */
	0,                      /* ucSizeBoardActivePassive  */
	0,                      /* ucSizeBoardConfigMode     */
	0,                      /* ucSizeBoardMfgDate        */
	0,                      /* ucSizeBoardPartNum        */
	0,                      /* ucSizeBoardUuid           */
	0,                      /* ucSizeBoardPcieInfo       */
	0,                      /* ucSizeBoardMaxPowerMode   */
	0,                      /* ucSizeMemorySize          */
	0,                      /* ucSizeOemId               */
	0,                      /* ucSizeCapability          */
	0,                      /* ucSizeMfgPartNum          */

	0,                      /* ucOffsetEepromVersion       */
	0,                      /* ucOffsetProductName         */
	0,                      /* ucOffsetBoardRev            */
	0,                      /* ucOffsetBoardSerial         */
	0,                      /* cNOffsetumMacIds            */
	0,                      /* ucOffsetBoardMac            */
	0,                      /* ucOffsetBoardActivePassive  */
	0,                      /* ucOffsetBoardConfigMode     */
	0,                      /* ucOffsetBoardMfgDate        */
	0,                      /* ucOffsetBoardPartNum        */
	0,                      /* ucOffsetBoardUuid           */
	0,                      /* ucOffsetBoardPcieInfo       */
	0,                      /* ucOffsetBoardMaxPowerMode   */
	0,                      /* ucOffsetMemorySize          */
	0,                      /* ucOffsetOemId               */
	0,                      /* ucOffsetCapability          */
	0,                      /* ucOffsetMfgPartNum          */

	0,                      /* ucOffsetChecksumLsb         */
	0,                      /* ucOffsetChecksumMsb         */
	0,                      /* ucChecksumStart             */
	0,                      /* ucChecksumEnd               */

	{
		0
	},                      /* pulStatCounters[ EEPROM_STATS_MAX ]      */
	{
		0
	},                      /* pulStatErrorCounters[ EEPROM_ERROR_MAX ] */

	LOWER_FIREWALL          /* ulLowerFirewall             */
};
static EEPROM_PRIVATE_DATA *pxThis = &xLocalData;


/******************************************************************************/
/* Private Function declarations                                              */
/******************************************************************************/

/**
 * @brief   Write a single byte to the EEPROM
 *
 * @param   ucAddressOffset     Address offset of register to write
 * @param   ucRegisterValue     Value to write
 *
 * @return  OK                  Bytes successfully written
 *          ERROR               Bytes write failed
 */
static int ucEepromWriteByte( uint8_t ucAddressOffset, uint8_t ucRegisterValue );

/**
 * @brief   Write one or more bytes to the EEPROM up to the page size
 *
 * @param   ucAddressOffset     Address offset of register to write
 * @param   pucData             Data to write
 * @param   ucWriteSize         Number of bytes to write
 *
 * @return  OK                  Bytes successfully written
 *          ERROR               Bytes write failed
 */
static int ucEepromWriteMultiBytes( uint8_t ucAddressOffset, uint8_t *pucData, uint8_t ucWriteSize );

/**
 * @brief   Read multiple bytes from the EEPROM
 *
 * @param   ucAddressOffset     Address offset of register to read
 * @param   pucRegisterValue    Pointer to the array to hold the read values
 * @param   ucReadSize          Number of bytes to read
 *
 * @return  OK                  Bytes successfully read
 *          ERROR               Bytes read failed
 */
static int ucEepromReadMultiBytes( uint8_t ucAddressOffset, uint8_t *pucRegisterValue, uint8_t ucReadSize );

/**
 * @brief   Read a single byte from the EEPROM
 *
 * @param   ucAddressOffset     Address offset of register to read
 * @param   pucRegisterValue    Pointer to the array to hold the read values
 *
 * @return  OK                  Byte successfully read
 *          ERROR               Byte read failed
 */
static int ucEepromReadByte( uint8_t ucAddressOffset, uint8_t *pucRegisterValue );

/**
 * @brief   Read the EEPROM Field
 *
 * @param   ucAddressOffset     Address offset of register to read
 * @param   pucData             Pointer to the array to hold the read values
 * @param   iIsMac             Is the field a MAC address, formatting treated differently
 *
 * @return  OK                  Field successfully read
 *          ERROR               Field read failed
 */
static int iEepromReadField( uint8_t ucAddressOffset, uint8_t *pucData, uint8_t ucReadSize, int iIsMac );

/**
 * @brief   Point the fields to the required  EEPROM version
 *
 * @param   xVersion            Version of EEPROM data
 *
 * @return  N/A
 */
static void vEepromInitialiseVersionFields( EEPROM_VERSION xVersion );

/**
 * @brief   Verify that the checksum for the EEPROM fields is correct
 *
 * @return  OK                  Checksum successfully verified
 *          ERROR               Checksum verification failed
 */
static int iEepromVerifyChecksum( void );

/**
 * @brief   Verify the EEPROM device ID.
 *
 * @return  OK if successful, else ERROR.
 */
static int iEeprom_VerifyDeviceId( void );

#ifdef EEPROM_VERBOSE_DEBUG_ENABLE
/**
 * @brief   Dump out all the EEPROM contents
 *
 * @return  OK                  Successs
 *          ERROR               Failure
 */
static int iEepromDumpContents( void );
#endif

/******************************************************************************/
/* Public Function implementations                                            */
/******************************************************************************/
/**
 * @brief   Initialises the EEPROM driver.
 */
int iEEPROM_Initialise( EEPROM_VERSION xEepromVersion, EEPROM_CFG *pxEepromCfg )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( FALSE == pxThis->iEepromInitialised )  &&
	    ( NULL != pxEepromCfg ) &&
	    ( ( uint16_t ) EEPROM_WRITE_BYTE_SIZE_MAX >= ( uint16_t ) ( pxEepromCfg->ucEepromAddressSize + 1 ) ) &&
	    ( ( uint16_t ) EEPROM_WRITE_MULTI_BYTE_SIZE_MAX >=
	      ( uint16_t ) ( pxEepromCfg->ucEepromAddressSize + pxEepromCfg->ucEepromPageSize ) ) )
	{
		pxThis->xEepromExpectedVersion = xEepromVersion;
		pvOSAL_MemCpy( &pxThis->xEepromCfg, pxEepromCfg, sizeof( pxThis->xEepromCfg ) );
		vEepromInitialiseVersionFields( pxThis->xEepromExpectedVersion );

		PLL_ERR( EEPROM_NAME, "iEepromReadField size=%d%d\r\n", pxThis->ucSizeEepromVersion);

		iStatus = iEeprom_VerifyDeviceId();
		if( OK == iStatus )
		{
			/* First lets read the EEPROM Version if possible */
			iStatus = iEepromReadField( pxThis->ucOffsetEepromVersion,
			                            pxThis->pucEepromVersion,
			                            pxThis->ucSizeEepromVersion,
			                            FALSE );
			PLL_ERR( EEPROM_NAME, "iEepromReadField version status=%d\r\n", iStatus );
			if( OK == iStatus )
			{
				uint32_t ulEepromVersion = pxThis->pucEepromVersion[ 0 ];
				/* The i2c read passed now check what it read */
				switch( ulEepromVersion )
				{
				case EEPROM_V1_0:
				{
					pxThis->xEepromActualVersion = EEPROM_VERSION_1_0;
					pxThis->pucEepromVersion[ 0 ] += 0x30;	/* ASCII */
					break;
				}

				default:
				{
					iStatus = ERROR;
					PLL_ERR( EEPROM_NAME, "Version error %X\r\n", ulEepromVersion );
					INC_ERROR_COUNTER( EEPROM_ERROR_INVALID_VERSION );
				}
				}

				if( OK == iStatus )
				{
					if( pxThis->xEepromExpectedVersion != pxThis->xEepromActualVersion )
					{
						PLL_WRN( EEPROM_NAME, "Unexpected EEPROM version\r\n" );
						INC_ERROR_COUNTER( EEPROM_ERROR_INCORRECT_VERSION );

						/* Re-initialise fields for the actual version */
						vEepromInitialiseVersionFields( pxThis->xEepromActualVersion );
					}
				}
			}
			else
			{
				INC_ERROR_COUNTER( EEPROM_ERROR_READ_FIELD );
			}

			if( OK == iStatus )
			{
				iStatus = iEepromVerifyChecksum();
				if( OK == iStatus )
				{
					/* If EEPROM Version was read ok now read the other fields */
					if( OK == iStatus )
					{
						iStatus = iEepromReadField( pxThis->ucOffsetProductName,
						                            pxThis->pucProductName,
						                            pxThis->ucSizeProductName,
						                            FALSE );
					}

					if( OK == iStatus )
					{
						iStatus = iEepromReadField( pxThis->ucOffsetBoardRev,
						                            pxThis->pucBoardRev,
						                            pxThis->ucSizeBoardRev,
						                            FALSE );
					}

					if( OK == iStatus )
					{
						iStatus = iEepromReadField( pxThis->ucOffsetBoardSerial,
						                            pxThis->pucBoardSerial,
						                            pxThis->ucSizeBoardSerial,
						                            FALSE );
					}

					if( OK == iStatus )
					{
						uint8_t ucNumMacIdsArray[ EEPROM_V1_0_TOT_MAC_ID_SIZE ] =
						{
							0
						};
						iStatus = iEepromReadField( pxThis->ucOffsetumMacIds,
						                            ucNumMacIdsArray,
						                            pxThis->ucSizeumMacIds,
						                            FALSE );

						if( OK == iStatus )
						{
							/* Xilinx IANA ID (3) + Version Number (1) */
							if (ucNumMacIdsArray[0] >= 4)
							{
								pxThis->ucNumMacIds = (ucNumMacIdsArray[0] - 4) / EEPROM_V1_0_MAC_SIZE;
							}
							else
							{
								pxThis->ucNumMacIds = 0;
							}
						}
					}

					if( OK == iStatus )
					{
						iStatus = iEepromReadField( pxThis->ucOffsetBoardMac,
						                            pxThis->pucBoardMac,
						                            pxThis->ucSizeBoardMac,
						                            TRUE );
					}

					if( OK == iStatus )
					{
						iStatus = iEepromReadField( pxThis->ucOffsetBoardMfgDate,
						                            pxThis->pucBoardMfgDate,
						                            pxThis->ucSizeBoardMfgDate,
						                            FALSE );
						#if 0
						if( OK == iStatus )
						{
							time_t ts;
							const uint64_t secs_from_1970_1996 = 820454400;
							uint32_t fru_ts = (uint32_t)(pxThis->pucBoardMfgDate[2] << 16) |  /* MSB */
											    		(pxThis->pucBoardMfgDate[1] <<  8) |
														(pxThis->pucBoardMfgDate[0]      ); /* LSB */


							/*if (FRU_BOARD_DATE_UNSPEC == fru_ts) {
								ts = IPMI_TIME_UNSPECIFIED;
							}
							else*/ {
								ts = fru_ts * 60 + secs_from_1970_1996;
							}
							struct tm* tyme = gmtime(&ts);
							PLL_ERR( EEPROM_NAME, "day=%d mon=%d year=%d\r\n", tyme->tm_mday, tyme->tm_mon, tyme->tm_year );

						}
						#endif
					}

					if( OK == iStatus )
					{
						iStatus = iEepromReadField( pxThis->ucOffsetBoardPartNum,
						                            pxThis->pucBoardPartNum,
						                            pxThis->ucSizeBoardPartNum,
						                            FALSE );
					}

					if( OK == iStatus )
					{
						uint8_t* p = pxThis->pucBoardUuid;
						iStatus = iEepromReadField( pxThis->ucOffsetBoardUuid,
						                            pxThis->pucBoardUuid,
						                            pxThis->ucSizeBoardUuid,
						                            FALSE );
						PLL_ERR( EEPROM_NAME, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
							p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],p[12],p[13],p[14],p[15]);
					}

					if( OK == iStatus )
					{
						iStatus = iEepromReadField( pxThis->ucOffsetMfgPartNum,
													pxThis->pucMfgPartNum,
													pxThis->ucSizeMfgPartNum,
													FALSE );
					}
				}

				if( OK == iStatus )
				{
					pxThis->iEepromInitialised = TRUE;
				}
			}
			else
			{
				INC_ERROR_COUNTER( EEPROM_ERROR_CHECKSUM );
			}
		}
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	if( OK == iStatus )
	{
		INC_STAT_COUNTER( EEPROM_STATS_INITIALISATION );
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_INITIALISATION );
	}

#ifdef EEPROM_VERBOSE_DEBUG_ENABLE
	iEepromDumpContents();
#endif

	return iStatus;
}

/**
 * @brief   Read the EEPROM Version
 */
int iEEPROM_GetEepromVersion( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucEepromVersion, pxThis->ucSizeEepromVersion );
		*pucSizeBytes = pxThis->ucSizeEepromVersion;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the Product Name
 */
int iEEPROM_GetProductName( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucProductName, pxThis->ucSizeProductName );
		*pucSizeBytes = pxThis->ucSizeProductName;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the Product Revision
 */
int iEEPROM_GetProductRevision( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucBoardRev, pxThis->ucSizeBoardRev );
		*pucSizeBytes = pxThis->ucSizeBoardRev;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the board serial number
 */
int iEEPROM_GetSerialNumber( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucBoardSerial, pxThis->ucSizeBoardSerial );
		*pucSizeBytes = pxThis->ucSizeBoardSerial;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the number of MAC addresses
 */
int iEEPROM_GetMacAddressCount( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, &pxThis->ucNumMacIds, pxThis->ucSizeumMacIds );
		*pucSizeBytes = pxThis->ucSizeumMacIds;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the First MAC Address
 */
int iEEPROM_GetFirstMacAddress( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucBoardMac, pxThis->ucSizeBoardMac );
		*pucSizeBytes = pxThis->ucSizeBoardMac;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the Active/Passive state
 */
int iEEPROM_GetActiveState( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucBoardActivePassive, pxThis->ucSizeBoardActivePassive );
		*pucSizeBytes = pxThis->ucSizeBoardActivePassive;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the Config Mode
 */
int iEEPROM_GetConfigMode( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucBoardConfigMode, pxThis->ucSizeBoardConfigMode );
		*pucSizeBytes = pxThis->ucSizeBoardConfigMode;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the Manufacturing Date
 */
int iEEPROM_GetManufacturingDate( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucBoardMfgDate, pxThis->ucSizeBoardMfgDate );
		*pucSizeBytes = pxThis->ucSizeBoardMfgDate;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the Part Number
 */
int iEEPROM_GetPartNumber( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucBoardPartNum, pxThis->ucSizeBoardPartNum );
		*pucSizeBytes = pxThis->ucSizeBoardPartNum;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the Manufacturer Part Number
 */
int iEEPROM_GetMfgPartNumber( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucMfgPartNum, pxThis->ucSizeMfgPartNum );
		*pucSizeBytes = pxThis->ucSizeMfgPartNum;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the UUID
 */
int iEEPROM_GetUuid( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		uint8_t* p = pucField;
		pvOSAL_MemCpy( pucField, pxThis->pucBoardUuid, pxThis->ucSizeBoardUuid );
		*pucSizeBytes = pxThis->ucSizeBoardUuid;
		PLL_ERR( EEPROM_NAME, "%d %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
			*pucSizeBytes, p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],p[12],p[13],p[14],p[15]);
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the PCIe ID
 */
int iEEPROM_GetPcieId( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucBoardPcieInfo, pxThis->ucSizeBoardPcieInfo );
		*pucSizeBytes = pxThis->ucSizeBoardPcieInfo;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the Max Power Mode
 */
int iEEPROM_GetMaxPowerMode( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucBoardMaxPowerMode, pxThis->ucSizeBoardMaxPowerMode );
		*pucSizeBytes = pxThis->ucSizeBoardMaxPowerMode;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the Memory Size
 */
int iEEPROM_GetMemorySize( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucMemorySize, pxThis->ucSizeMemorySize );
		*pucSizeBytes = pxThis->ucSizeMemorySize;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read the OEM ID
 */
int iEEPROM_GetOemId( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucOemId, pxThis->ucSizeOemId );
		*pucSizeBytes = pxThis->ucSizeOemId;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}


	return iStatus;
}

/**
 * @brief   Read the Capability
 */
int iEEPROM_GetCapability( uint8_t *pucField, uint8_t *pucSizeBytes )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucField ) &&
	    ( NULL != pucSizeBytes ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		pvOSAL_MemCpy( pucField, pxThis->pucCapability, pxThis->ucSizeCapability );
		*pucSizeBytes = pxThis->ucSizeCapability;
		iStatus       = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read raw data
 */
int iEEPROM_ReadRawValue( uint8_t *pucData, uint8_t ucSizeBytes, uint8_t ucEepromAddr )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucData ) &&
	    ( EEPROM_ADDRESS_SIZE_UNINITIALISED != pxThis->xEepromCfg.ucEepromAddressSize ) &&
	    ( EEPROM_MAX_DATA_SIZE >= ucSizeBytes ) )
	{
		iStatus = OK;
		if( OK != ucEepromReadMultiBytes( ucEepromAddr, pucData, ucSizeBytes ) )
		{
			iStatus = ERROR;
		}
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Write raw data
 */
int iEEPROM_WriteRawValue( uint8_t *pucData, uint8_t ucSizeBytes, uint8_t ucEepromAddr )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( NULL != pucData ) &&
	    ( EEPROM_ADDRESS_SIZE_UNINITIALISED != pxThis->xEepromCfg.ucEepromAddressSize ) &&
	    ( EEPROM_MAX_DATA_SIZE >= ucSizeBytes ) )
	{
		iStatus = OK;

		if( EEPROM_DATA_SINGLE_BYTE == ucSizeBytes )
		{
			if( OK != ucEepromWriteByte( ucEepromAddr, pucData[ EEPROM_ADDRESS_BYTE_ZERO ] ) )
			{
				iStatus = ERROR;
			}
		}
		else
		{
			uint8_t ucOffset           = ucEepromAddr;
			uint8_t ucBytesToWrite     = 0;
			uint8_t ucBytesInFirstPage = 0;

			if( ucOffset % pxThis->xEepromCfg.ucEepromPageSize )
			{
				ucBytesInFirstPage = ( pxThis->xEepromCfg.ucEepromPageSize -
				                       ( ucEepromAddr % pxThis->xEepromCfg.ucEepromPageSize ) );
				if( ucBytesInFirstPage > ucSizeBytes )
				{
					ucBytesInFirstPage = ucSizeBytes;
				}
				iStatus = ucEepromWriteMultiBytes( ucOffset, pucData, ucBytesInFirstPage );
				if( OK == iStatus )
				{
					ucOffset += ucBytesInFirstPage;
				}
			}

			if( OK == iStatus )
			{
				ucBytesToWrite = ucSizeBytes - ucBytesInFirstPage;

				while( 0 < ucBytesToWrite )
				{
					uint8_t ucBytesToWriteNextPage = 0;

					/* Either write a page or the remainder of a page */
					if( pxThis->xEepromCfg.ucEepromPageSize <= ucBytesToWrite )
					{
						ucBytesToWriteNextPage = pxThis->xEepromCfg.ucEepromPageSize;
						ucBytesToWrite        -= pxThis->xEepromCfg.ucEepromPageSize;
					}
					else
					{
						ucBytesToWriteNextPage = ucBytesToWrite;
						ucBytesToWrite         = 0;
					}

					iStatus = ucEepromWriteMultiBytes( ucOffset,
					                                   &pucData[ ucOffset - ucEepromAddr ],
					                                   ucBytesToWriteNextPage );
					if( OK != iStatus )
					{
						/* Failure counter set within function, break and return */
						break;
					}
					ucOffset += ucBytesToWriteNextPage;
				}
			}
		}
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Print all the stats gathered by the eeprom driver
 */
int iEEPROM_PrintStatistics( void )
{
	int iStatus = ERROR;
	int i       = 0;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) )
	{
		PLL_INF( EEPROM_NAME, "======================================\n\r" );
		PLL_INF( EEPROM_NAME, "Statistics:\n\r" );
		for( i = 0; i < EEPROM_STATS_MAX; i++ )
		{
			PRINT_STAT( i );
		}
		PLL_INF( EEPROM_NAME, "--------------------------------------\n\r" );
		PLL_INF( EEPROM_NAME, "Errors:\n\r" );
		for( i = 0; i < EEPROM_ERROR_MAX; i++ )
		{
			PRINT_ERROR_STAT( i );
		}
		PLL_INF( EEPROM_NAME, "======================================\n\r" );
		iStatus = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Clear all the stats in the eeprom driver
 */
int iEEPROM_ClearStatistics( void )
{
	int iStatus = ERROR;
	int i       = 0;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) )
	{
		for( i = 0; i < EEPROM_STATS_MAX; i++ )
		{
			pxThis->pulStatCounters[ i ] = 0;
		}
		for( i = 0; i < EEPROM_ERROR_MAX; i++ )
		{
			pxThis->pulStatErrorCounters[ i ] = 0;
		}
		iStatus = OK;
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Display all the EEPROM fields
 */
int iEEPROM_DisplayEepromValues( void )
{
	int iStatus = ERROR;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) &&
	    ( TRUE == pxThis->iEepromInitialised ) )
	{
		iStatus = OK;

		PLL_LOG( EEPROM_NAME,
		         "Manufacturing INFO: EEPROM Version        : %s\n\r",
		         pxThis->pucEepromVersion );
		PLL_LOG( EEPROM_NAME,
		         "Manufacturing INFO: Name                  : %s\n\r",
		         pxThis->pucProductName );
		PLL_LOG( EEPROM_NAME,
		         "Manufacturing INFO: Board Rev             : %s\n\r",
		         pxThis->pucBoardRev );
		PLL_LOG( EEPROM_NAME,
		         "Manufacturing INFO: Serial Number         : %s\n\r",
		         pxThis->pucBoardSerial );
		PLL_LOG( EEPROM_NAME,
		         "Manufacturing INFO: # MACS                : %d\n\r",
		         pxThis->ucNumMacIds );
		PLL_LOG( EEPROM_NAME,
		         "Manufacturing INFO: Mac Address 1         : %02x:%02x:%02x:%02x:%02x:%02x\n\r",
		         pxThis->pucBoardMac[ 0 ],
		         pxThis->pucBoardMac[ 1 ],
		         pxThis->pucBoardMac[ 2 ],
		         pxThis->pucBoardMac[ 3 ],
		         pxThis->pucBoardMac[ 4 ],
		         pxThis->pucBoardMac[ 5 ] );
		PLL_LOG( EEPROM_NAME,
		         "Manufacturing INFO: MFG DATE              : %02x%02x%02x\n\r",
		         pxThis->pucBoardMfgDate[ 0 ],
		         pxThis->pucBoardMfgDate[ 1 ],
		         pxThis->pucBoardMfgDate[ 2 ] );
		PLL_LOG( EEPROM_NAME,
		         "Manufacturing INFO: Board Part Num        : %s\n\r",
		         pxThis->pucBoardPartNum );
		PLL_LOG( EEPROM_NAME,
		         "Manufacturing INFO: UUID                  : %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n\r",
		         pxThis->pucBoardUuid[ 0 ],
		         pxThis->pucBoardUuid[ 1 ],
		         pxThis->pucBoardUuid[ 2 ],
		         pxThis->pucBoardUuid[ 3 ],
		         pxThis->pucBoardUuid[ 4 ],
		         pxThis->pucBoardUuid[ 5 ],
		         pxThis->pucBoardUuid[ 6 ],
		         pxThis->pucBoardUuid[ 7 ],
		         pxThis->pucBoardUuid[ 8 ],
		         pxThis->pucBoardUuid[ 9 ],
		         pxThis->pucBoardUuid[ 10 ],
		         pxThis->pucBoardUuid[ 11 ],
		         pxThis->pucBoardUuid[ 12 ],
		         pxThis->pucBoardUuid[ 13 ],
		         pxThis->pucBoardUuid[ 14 ],
		         pxThis->pucBoardUuid[ 15 ] );
		PLL_LOG( EEPROM_NAME, "Manufacturing INFO: MFG part number       : %s\n\r", pxThis->pucMfgPartNum );
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}


/******************************************************************************/
/* Private Function declarations                                              */
/******************************************************************************/

/**
 * @brief   Write a single byte to the EEPROM
 */
static int ucEepromWriteByte( uint8_t ucAddressOffset, uint8_t ucRegisterValue )
{
	int iStatus = ERROR;
	uint8_t pucBuffer[ EEPROM_WRITE_BYTE_SIZE_MAX ] =
	{
		0
	};

	if( ( EEPROM_ONE_BYTE <= pxThis->xEepromCfg.ucEepromAddressSize ) &&
	    ( EEPROM_TWO_BYTES >= pxThis->xEepromCfg.ucEepromAddressSize ) )
	{
		uint32_t ulDataLen = pxThis->xEepromCfg.ucEepromAddressSize + EEPROM_DATA_SINGLE_BYTE;

		if( EEPROM_2_BYTE_ADDRESS == pxThis->xEepromCfg.ucEepromAddressSize )
		{
			PLL_ERR( EEPROM_NAME, "2 bytes address: 0x%2X %d 0x%02X 0x%02X\n\r",
				pxThis->xEepromCfg.ucEepromSlaveAddress,
				pxThis->xEepromCfg.ucEepromAddressSize,
				ucAddressOffset,
				ucRegisterValue);

			pucBuffer[ EEPROM_ADDRESS_BYTE_ONE ]     = ucAddressOffset;
			pucBuffer[ EEPROM_ADDRESS_BYTE_ONE + 1 ] = ucRegisterValue;
		}
		else
		{
			pucBuffer[ EEPROM_ADDRESS_BYTE_ZERO ]     = ucAddressOffset;
			pucBuffer[ EEPROM_ADDRESS_BYTE_ZERO + 1 ] = ucRegisterValue;
		}

		iStatus = iI2C_Send( pxThis->xEepromCfg.ucEepromI2cBus,
							pxThis->xEepromCfg.ucEepromSlaveAddress,
							pucBuffer,
							ulDataLen );
		if( ERROR == iStatus )
		{
			INC_ERROR_COUNTER( EEPROM_ERROR_SINGLE_BYTE_WRITE );
		}
		else
		{
			INC_STAT_COUNTER( EEPROM_STAT_SINGLE_BYTE_WRITE );
		}
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Write one or more bytes to the EEPROM up to the page size
 */
static int ucEepromWriteMultiBytes( uint8_t ucAddressOffset, uint8_t *pucData, uint8_t ucWriteSize )
{
	int iStatus = ERROR;
	uint8_t pucBuffer[ EEPROM_WRITE_MULTI_BYTE_SIZE_MAX ] =
	{
		0
	};

	if( ( EEPROM_ONE_BYTE <= pxThis->xEepromCfg.ucEepromAddressSize ) &&
	    ( EEPROM_TWO_BYTES >= pxThis->xEepromCfg.ucEepromAddressSize ) &&
	    ( pxThis->xEepromCfg.ucEepromPageSize >= ucWriteSize ) )
	{
		uint32_t ulDataLen = pxThis->xEepromCfg.ucEepromAddressSize + ucWriteSize;

		if( EEPROM_2_BYTE_ADDRESS == pxThis->xEepromCfg.ucEepromAddressSize )
		{
			pucBuffer[ EEPROM_ADDRESS_BYTE_ONE ] = ucAddressOffset;
			pvOSAL_MemCpy( &pucBuffer[ EEPROM_ADDRESS_BYTE_ONE + 1 ], pucData, ucWriteSize );
		}
		else
		{
			pucBuffer[ EEPROM_ADDRESS_BYTE_ZERO ] = ucAddressOffset;
			pvOSAL_MemCpy( &pucBuffer[ EEPROM_ADDRESS_BYTE_ZERO + 1 ], pucData, ucWriteSize );
		}

		iStatus = iI2C_Send( pxThis->xEepromCfg.ucEepromI2cBus,
							pxThis->xEepromCfg.ucEepromSlaveAddress,
							pucBuffer,
							ulDataLen );
		if( ERROR == iStatus )
		{
			INC_ERROR_COUNTER( EEPROM_ERROR_MULTI_BYTE_WRITE );
		}
		else
		{
			/* Small delay required between each write */
			iOSAL_Task_SleepMs( EEPROM_WRITE_DELAY_MS );
			INC_STAT_COUNTER( EEPROM_STAT_MULTI_BYTE_WRITE );
		}
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read a single byte from the EEPROM
 */
static int ucEepromReadByte( uint8_t ucAddressOffset, uint8_t *pucRegisterValue )
{
	int iStatus = ERROR;
	uint8_t pucAddressOffset[ EEPROM_ADDRESS_SIZE_MAX ] =
	{
		0
	};

	if( ( NULL != pucRegisterValue ) &&
	    ( EEPROM_ONE_BYTE <= pxThis->xEepromCfg.ucEepromAddressSize ) &&
	    ( EEPROM_TWO_BYTES >= pxThis->xEepromCfg.ucEepromAddressSize ) )
	{
		if( EEPROM_2_BYTE_ADDRESS == pxThis->xEepromCfg.ucEepromAddressSize )
		{
			pucAddressOffset[ EEPROM_ADDRESS_BYTE_ONE ] = ucAddressOffset;
		}
		else
		{
			pucAddressOffset[ EEPROM_ADDRESS_BYTE_ZERO ] = ucAddressOffset;
		}

		iStatus = iI2C_SendRecv( pxThis->xEepromCfg.ucEepromI2cBus,
		                         pxThis->xEepromCfg.ucEepromSlaveAddress,
		                         pucAddressOffset,
		                         pxThis->xEepromCfg.ucEepromAddressSize,
		                         pucRegisterValue,
		                         EEPROM_DATA_SINGLE_BYTE );
		if( ERROR == iStatus )
		{
			INC_ERROR_COUNTER( EEPROM_ERROR_SINGLE_BYTE_READ );
		}
		else
		{
			INC_STAT_COUNTER( EEPROM_STAT_SINGLE_BYTE_READ );
		}
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	return iStatus;
}

/**
 * @brief   Read multiple bytes from the EEPROM
 */
static int ucEepromReadMultiBytes( uint8_t ucAddressOffset, uint8_t *pucRegisterValue, uint8_t ucReadSize )
{
	int iStatus = ERROR;
	uint8_t pucAddressOffset[ EEPROM_ADDRESS_SIZE_MAX ] =
	{
		0
	};

	if( ( NULL != pucRegisterValue ) &&
	    ( EEPROM_ONE_BYTE <= pxThis->xEepromCfg.ucEepromAddressSize ) &&
	    ( EEPROM_TWO_BYTES >= pxThis->xEepromCfg.ucEepromAddressSize ) )
	{
		if( EEPROM_2_BYTE_ADDRESS == pxThis->xEepromCfg.ucEepromAddressSize )
		{
			pucAddressOffset[ EEPROM_ADDRESS_BYTE_ONE ] = ucAddressOffset;
		}
		else
		{
			pucAddressOffset[ EEPROM_ADDRESS_BYTE_ZERO ] = ucAddressOffset;
		}

		iStatus = iI2C_SendRecv( pxThis->xEepromCfg.ucEepromI2cBus,
		                         pxThis->xEepromCfg.ucEepromSlaveAddress,
		                         pucAddressOffset,
		                         pxThis->xEepromCfg.ucEepromAddressSize,
		                         pucRegisterValue,
		                         ucReadSize );
		if( ERROR == iStatus )
		{
			INC_ERROR_COUNTER( EEPROM_ERROR_MULTI_BYTE_READ );
		}
		else
		{
			INC_STAT_COUNTER( EEPROM_STAT_MULTI_BYTE_READ );
		}
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}


	return iStatus;
}

/**
 * @brief   Read the EEPROM Field
 *
 * @return  N/A
 */
static int iEepromReadField( uint8_t ucAddressOffset, uint8_t *pucData, uint8_t ucReadSize, int iIsMac )
{
	int iStatus = ERROR;
	int i       = 0;

	PLL_ERR( EEPROM_NAME, "iEepromReadField offset=%d size=%d add_size=%d\r\n",
		ucAddressOffset, ucReadSize, pxThis->xEepromCfg.ucEepromAddressSize);
	if( ( NULL != pucData ) &&
	    ( EEPROM_ONE_BYTE <= pxThis->xEepromCfg.ucEepromAddressSize ) &&
	    ( EEPROM_TWO_BYTES >= pxThis->xEepromCfg.ucEepromAddressSize ) )
	{
		for( i = 0; i < ucReadSize; i++ )
		{
			iStatus = ucEepromReadByte( ucAddressOffset++, &pucData[ i ] );
			if( FALSE == iIsMac )
			{
				if( pucData[ i ] == EEPROM_DEFAULT_VAL )
				{
					pucData[ i ] = '\0';
				}
			}
			if( ERROR == iStatus )
			{
				break;
			}
		}

		/* Explicitly set \0 at the end of field string */
		pucData[ ucReadSize ] = '\0';
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}

	if( ERROR == iStatus )
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_READ_FIELD );
	}
	else
	{
		INC_STAT_COUNTER( EEPROM_STATS_READ_FIELD );
	}

	return iStatus;
}

/**
 * @brief   Point the fields to the required  EEPROM version
 */
static void vEepromInitialiseVersionFields( EEPROM_VERSION xVersion )
{
    PLL_ERR( EEPROM_NAME, "vEepromInitialiseVersionFields eeprom version = %d \r\n", xVersion);

	if( EEPROM_VERSION_1_0 == xVersion )
	{
		pxThis->pucEepromVersion = ( uint8_t * )( pxThis->xBoardInfo.xBoardInfoV1_0.ucEepromVersion );
		pxThis->pucProductName   = ( uint8_t * )( pxThis->xBoardInfo.xBoardInfoV1_0.ucProductName );
		pxThis->pucBoardPartNum  = ( uint8_t * )( pxThis->xBoardInfo.xBoardInfoV1_0.ucPartNumber );
		pxThis->pucMfgPartNum    = ( uint8_t * )( pxThis->xBoardInfo.xBoardInfoV1_0.ucMfgPartNumber );
		pxThis->pucBoardRev      = ( uint8_t * )( pxThis->xBoardInfo.xBoardInfoV1_0.ucMfgPartRevision );
		pxThis->pucBoardSerial   = ( uint8_t * )( pxThis->xBoardInfo.xBoardInfoV1_0.ucProductSerial );
		pxThis->pucBoardMfgDate  = ( uint8_t * )( pxThis->xBoardInfo.xBoardInfoV1_0.ucMfgDate );
		pxThis->ucNumMacIds      = (                  pxThis->xBoardInfo.xBoardInfoV1_0.ucNumMacIds );
		pxThis->pucBoardMac      = ( uint8_t * )( pxThis->xBoardInfo.xBoardInfoV1_0.ucMac );
		pxThis->pucBoardUuid     = ( uint8_t * )( pxThis->xBoardInfo.xBoardInfoV1_0.ucUuid );

		pxThis->ucSizeEepromVersion      = EEPROM_VERSION_SIZE;
		pxThis->ucSizeProductName        = EEPROM_V1_0_PRODUCT_NAME_SIZE;
		pxThis->ucSizeBoardPartNum       = EEPROM_V1_0_PRODUCT_PART_NUM_SIZE;
		pxThis->ucSizeBoardRev           = EEPROM_V1_0_MFG_PART_REV_SIZE;
		pxThis->ucSizeBoardSerial        = EEPROM_V1_0_PRODUCT_SERIAL_SIZE;
		pxThis->ucSizeumMacIds           = EEPROM_V1_0_TOT_MAC_ID_SIZE;
		pxThis->ucSizeBoardMac           = EEPROM_V1_0_MAC_SIZE;
		pxThis->ucSizeBoardMfgDate       = EEPROM_V1_0_MFG_DATE_SIZE;
		pxThis->ucSizeBoardUuid          = EEPROM_V1_0_UUID_SIZE;
		pxThis->ucSizeMfgPartNum         = EEPROM_V1_0_MFG_PART_NUM_SIZE;
		pxThis->ucSizeBoardActivePassive = EEPROM_FIELD_NA_SIZE;
		pxThis->ucSizeBoardConfigMode    = EEPROM_FIELD_NA_SIZE;
		pxThis->ucSizeBoardPcieInfo      = EEPROM_FIELD_NA_SIZE;
		pxThis->ucSizeBoardMaxPowerMode  = EEPROM_FIELD_NA_SIZE;
		pxThis->ucSizeMemorySize         = EEPROM_FIELD_NA_SIZE;
		pxThis->ucSizeOemId              = EEPROM_FIELD_NA_SIZE;
		pxThis->ucSizeCapability         = EEPROM_FIELD_NA_SIZE;

		pxThis->ucOffsetEepromVersion = EEPROM_VERSION_OFFSET;
		pxThis->ucOffsetProductName   = EEPROM_V1_0_PRODUCT_NAME_OFFSET;
		pxThis->ucOffsetBoardRev      = EEPROM_V1_0_MFG_PART_REV_OFFSET;
		pxThis->ucOffsetBoardSerial   = EEPROM_V1_0_PRODUCT_SERIAL_OFFSET;
		pxThis->ucOffsetumMacIds      = EEPROM_V1_0_TOT_MAC_ID_OFFSET;
		pxThis->ucOffsetBoardMac      = EEPROM_V1_0_MAC_OFFSET;
		pxThis->ucOffsetBoardMfgDate  = EEPROM_V1_0_MFG_DATE_OFFSET;
		pxThis->ucOffsetBoardPartNum  = EEPROM_V1_0_PRODUCT_PART_NUM_OFFSET;
		pxThis->ucOffsetMfgPartNum    = EEPROM_V1_0_MFG_PART_NUM_OFFSET;
		pxThis->ucOffsetBoardUuid     = EEPROM_V1_0_UUID_OFFSET;
		pxThis->ucOffsetChecksumLsb   = EEPROM_V1_0_HEADER_CHECKSUM_OFFSET;
		pxThis->ucOffsetChecksumMsb   = 0;
		pxThis->ucChecksumStart       = EEPROM_V1_0_CHECKSUM_START;
		pxThis->ucChecksumEnd         = EEPROM_V1_0_CHECKSUM_END;
	}
}

/**
 * @brief   Verify that the checksum for the EEPROM Header is correct
 */
static int iEepromVerifyChecksum( void )
{
	int iStatus                    = ERROR;
	unsigned char pucData[ EEPROM_V1_0_BOARD_CHECKSUM_SIZE  + EEPROM_V1_0_HEADER_CHECKSUM_SIZE] =
	{
		0
	};
	int i                        = 0;
	uint8_t ucAddressOffset      = 0;
	uint8_t usCalculatedChecksum = 0;

	iStatus = ucEepromReadMultiBytes( ucAddressOffset, &pucData[ i ], EEPROM_V1_0_BOARD_CHECKSUM_SIZE
		+ EEPROM_V1_0_HEADER_CHECKSUM_SIZE);
	if( OK == iStatus )
	{
		for( i = 0; i < EEPROM_V1_0_HEADER_CHECKSUM_SIZE; ++i )
		{
			usCalculatedChecksum += pucData[ i ];
		}

		if( usCalculatedChecksum != 0U )
		{
			iStatus = ERROR;
			INC_ERROR_COUNTER( EEPROM_ERROR_CHECKSUM );
		}
		else
		{
			INC_STAT_COUNTER( EEPROM_STATS_CHECKSUM );
		}
	}

	if( OK == iStatus )
	{
		uint8_t ucBoardInfoend = pucData[ EEPROM_V1_0_BOARD_LENGTH_OFFSET ] * 8 +
										EEPROM_V1_0_HEADER_CHECKSUM_SIZE;
		if( OK == iStatus )
		{
			usCalculatedChecksum = 0;
			for( i = EEPROM_V1_0_HEADER_CHECKSUM_SIZE; i < ucBoardInfoend; ++i )
			{
				usCalculatedChecksum += pucData[ i ];
			}

			if( usCalculatedChecksum != 0U )
			{
			PLL_ERR( EEPROM_NAME, "iEepromVerifyHeaderChecksum1 checksum = %d \r\n", usCalculatedChecksum);
				iStatus = ERROR;
				INC_ERROR_COUNTER( EEPROM_ERROR_CHECKSUM );
			}
			else
			{
				INC_STAT_COUNTER( EEPROM_STATS_CHECKSUM );
			}
		}
	}
	return iStatus;
}

/**
 * @brief   Verify the device ID of EEPROM
 */
static int iEeprom_VerifyDeviceId( void )
{
	int iStatus = ERROR;
	uint8_t pucWriteBuf[ EEPROM_WRITE_MULTI_BYTE_SIZE_MAX ] =
	{
		0
	};
	uint8_t pucReadBuf[ EEPROM_2_BYTE_ADDRESS ] =
	{
		0
	};
	uint16_t usDeviceId = 0;

	if( ( UPPER_FIREWALL == pxThis->ulUpperFirewall ) &&
	    ( LOWER_FIREWALL == pxThis->ulLowerFirewall ) )
	{
		int i = 0;

		pucWriteBuf[ 0 ] = pxThis->xEepromCfg.ucEepromDeviceIdRegister;
		for( i = 0; i < EEPROM_DEVICE_ID_CHECK_TRY_COUNT; i++ )
		{
			iStatus = iI2C_SendRecv( pxThis->xEepromCfg.ucEepromI2cBus,
			                         pxThis->xEepromCfg.ucEepromDeviceIdAddress,
			                         pucWriteBuf,
			                         1,
			                         &pucReadBuf[ 0 ],
			                         EEPROM_2_BYTE_ADDRESS );
           PLL_ERR( EEPROM_NAME, "iEeprom_VerifyDeviceId iI2C_SendRecv status = %d \r\n", iStatus);
           PLL_ERR( EEPROM_NAME, "iEeprom_VerifyDeviceId ucEepromDeviceIdRegister=0x%X read=0x%X%X \r\n",
		    pxThis->xEepromCfg.ucEepromDeviceIdRegister, pucReadBuf[0], pucReadBuf[1]);
			if( ERROR == iStatus )
			{
				INC_ERROR_COUNTER( EEPROM_ERRORS_DEVICE_ID_READ );
			}
			else
			{
				usDeviceId = ( pucReadBuf[ 0 ] << 8 ) | pucReadBuf[ 1 ];
				if( pxThis->xEepromCfg.usEepromDeviceId == usDeviceId )
				{
					INC_STAT_COUNTER( EEPROM_STATS_VERIFY_DEVICE_ID );
					break;
				}
				else
				{
					INC_ERROR_COUNTER( EEPROM_ERRORS_VERIFY_DEVICE_ID );
				}
			}
		}
	}
	else
	{
		INC_ERROR_COUNTER( EEPROM_ERROR_VALIDATION );
	}
	return OK;
	return iStatus;
}

/**
 * @brief   Dump out all the EEPROM contents
 */
#ifdef EEPROM_VERBOSE_DEBUG_ENABLE
static int iEepromDumpContents( void )
{
	uint8_t pucDataBuff[ pxThis->xEepromCfg.ucEepromPageSize ] =
	{
		0
	};
	int iStatus = ERROR;
	int iPageId = 0;
	int i       = 0;

	for( iPageId = 0; iPageId < pxThis->xEepromCfg.ucEepromNumPages; iPageId++ )
	{
		iStatus = iEEPROM_ReadRawValue( pucDataBuff,
		                                pxThis->xEepromCfg.ucEepromPageSize,
		                                ( pxThis->xEepromCfg.ucEepromPageSize * iPageId ) );
		if( OK == iStatus )
		{
			for( i = 0; i < pxThis->xEepromCfg.ucEepromPageSize; i++ )
			{
				if( 0 == ( i % pxThis->xEepromCfg.ucEepromPageSize ) )
				{
					if( 0 != i )
					{
						vPLL_Printf( "\r\n" );
					}
					vPLL_Printf( "\t[ 0x%04X ]. . . . . :", ( iPageId * pxThis->xEepromCfg.ucEepromPageSize ) );
				}
				vPLL_Printf( " %02X", pucDataBuff[ i ] );
			}
		}

		vPLL_Printf( "\r\n" );
	}

	return iStatus;
}
#endif /* EEPROM_VERBOSE_DEBUG_ENABLE */
