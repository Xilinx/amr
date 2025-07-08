/**
 * Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * This header file contains the Operating System Abstraction Layer (OSAL)
 * API for AMC.
 *
 * @file osal.h
 */

#ifndef _OSAL_H_
#define _OSAL_H_

/*****************************************************************************/
/* Includes                                                                  */
/*****************************************************************************/

/* Standard includes */
#include <stdint.h>
#include <stddef.h>

/*****************************************************************************/
/* Defines                                                                   */
/*****************************************************************************/

#define OSAL_TRUE                  ( 1 )
#define OSAL_FALSE                 ( 0 )

#define OSAL_TIMEOUT_NO_WAIT       ( 0U  )
#define OSAL_TIMEOUT_WAIT_FOREVER  ( -1U )
#define OSAL_TIMEOUT_TASK_WAIT_MS  ( 5U  )
#define OSAL_OS_NAME_LEN           ( 15U )

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/**
 * @enum    OSAL_ERRORS
 * @brief   Return values from an OSAL function
 */
typedef enum _OSAL_ERRORS
{
    OSAL_ERRORS_NONE = 0,           /* no errors, call was successful */
    OSAL_ERRORS_PARAMS,             /* invalid parameters passed in to function */
    OSAL_ERRORS_INVALID_HANDLE,     /* invalid / un-initialised handle passed into to function */
    OSAL_ERRORS_OS_IMPLEMENTATION,  /* error code returned from os implementation */
    OSAL_ERRORS_OS_NOT_STARTED,     /* OS has not been started */
    OSAL_ERRORS_INSUFFICIENT_MEM,   /* insufficient memory to complete call */

    MAX_OSAL_ERROR,

} OSAL_ERRORS;

/**
 * @enum    OSAL_TIMER_CONFIG
 * @brief   Return values from an OSAL function
 */
typedef enum _OSAL_TIMER_CONFIG
{
    OSAL_TIMER_CONFIG_ONE_SHOT = 0,	/* timer will be a one-shot, entering dormant state after it expires */
    OSAL_TIMER_CONFIG_PERIODIC,		/* timer will expire repeatedly */

    MAX_OSAL_TIMER_CONFIG,

} OSAL_TIMER_CONFIG;

/**
 * @enum    OSAL_STATS_VERBOSITY
 * @brief   Debug stats verbosity levels
 */
typedef enum _OSAL_STATS_VERBOSITY
{
    OSAL_STATS_VERBOSITY_COUNT_ONLY = 0,
    OSAL_STATS_VERBOSITY_ACTIVE_ONLY,
    OSAL_STATS_VERBOSITY_FULL,

    MAX_OSAL_STATS_VERBOSITY

} OSAL_STATS_VERBOSITY;

/**
 * @enum    OSAL_STATS_TYPE
 * @brief   Debug stat types
 */
typedef enum _OSAL_STATS_TYPE
{
    OSAL_STATS_TYPE_OS = 0,
    OSAL_STATS_TYPE_TASK,
    OSAL_STATS_TYPE_MUTEX,
    OSAL_STATS_TYPE_SEM,
    OSAL_STATS_TYPE_MAILBOX,
    OSAL_STATS_TYPE_EVENT,
    OSAL_STATS_TYPE_TIMER,
    OSAL_STATS_TYPE_MEMORY,
    OSAL_STATS_TYPE_ALL,

    MAX_OSAL_STATS_TYPE_ALL

} OSAL_STATS_TYPE;

/*****************************************************************************/
/* Public APIs                                                               */
/*****************************************************************************/

/**
 * iOSAL_GetOsVersion() - This function will return OS type and version
 *  					  information for the OSAL implementation being used.
 *
 * @pcOs:           Pointer to OS Name buffer.
 * @ucVersionMajor: Pointer to uint8_t holding OS version major.
 * @ucVersionMinor: Pointer to uint8_t holding OS version minor.
 * @ucVersionBuild: Pointer to uint8_t holding OS version build.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_GetOsVersion( char pcOs[ OSAL_OS_NAME_LEN ],
                        uint8_t* pucVersionMajor,
                        uint8_t* pucVersionMinor,
                        uint8_t* pucVersionBuild );

/*****************************************************************************/
/* Scheduler APIs                                                            */
/*****************************************************************************/

/**
 * iOSAL_StartOS() - This function will create an initial task, and then start
 * 					 the RTOS Scheduler.
 *
 * @iRoundRobinEnabled:   Flag indicating if round robin scheduling should be used.
 *                        If TRUE, Round-Robin is enabled. If FALSE, Round-Robin
 * 					      is disabled.
 * @ppvTaskHandle:        Pointer-to-pointer that will be cast to appropriate OS Task Handle data type
 * @pvStartTask:          Pointer to Start Task function
 * @usStartTaskStackSize: The number of bytes to allocate for use as the start task's stack.
 * @ulStartTaskPriority:  The priority at which the the created start task will execute
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    If successful, this function should never return.
 */
int iOSAL_StartOS( int		iRoundRobinEnabled,
                   void**	ppvTaskHandle,
                   void		( *pvStartTask )( void ),
                   uint16_t	usStartTaskStackSize,
                   uint32_t	ulStartTaskPriority );

/**
 * ulOSAL_GetUptimeTicks() - Returns tick count since OS was initialised.
 *
 * @return  Count of ticks since OS was initialised.
 *
 * @note    This function cannot be called from an ISR.
 */
uint32_t ulOSAL_GetUptimeTicks( void );

/**
 * ulOSAL_GetUptimeMs() - Returns ms count since OS was initialised.
 *
 * @return  Ms since OS was initialised.
 *
 * @note    This function cannot be called from an ISR.
 */
uint32_t ulOSAL_GetUptimeMs( void );

/**
 * ulOSAL_GetUptimeTicksFromISR() - Returns tick count since OS was initialised, from ISR.
 *
 * @return  Count of ticks since OS was initialised.
 *
 * @note    This function can be called from an ISR.
 */
uint32_t ulOSAL_GetUptimeTicksFromISR( void );

/**
 * ulOSAL_GetUptimeMsFromISR() - Returns ms count since OS was initialised, from ISR.
 *
 * @return  Ms since OS was initialised.
 *
 * @note    This function can be called from an ISR.
 */
uint32_t ulOSAL_GetUptimeMsFromISR( void );

/*****************************************************************************/
/* Task APIs                                                                 */
/*****************************************************************************/

/**
 * iOSAL_Task_Create() - Create a new OSAL task.
 *
 * @ppvTaskHandle:   Pointer-to-pointer that will be cast to appropriate OS Task Handle data type
 * @pvTaskFunction:  Pointer to Task function
 * @usTaskStackSize: The number of bytes to allocate for use as the task's stack.
 * @pvTaskParam:     The value to be passed as the parameter to the Task function
 * @ulTaskPriority:  The priority at which the the created task will execute
 * @pcTaskName:      Descriptive name for the task
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *          OSAL_ERRORS_OS_NOT_STARTED      OS has not been started
 *
 * @note    The Task Handle must be initialised as NULL.
 */
int iOSAL_Task_Create( void**      ppvTaskHandle,
                       void        ( *pvTaskFunction )( void* pvTaskParam ),
                       uint16_t    usTaskStackSize,
                       void*       pvTaskParam,
                       uint32_t    ulTaskPriority,
                       const char* pcTaskName );

/**
 * iOSAL_Task_Delete() - Remove OSAL task from RTOS Kernal.
 *
 * @ppvTaskHandle:   Pointer-to-pointer that will be cast to appropriate OS Task Handle data type
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_NOT_STARTED      OS has not been started
 *
 * @note    pvTaskHandle must have been previously initialised with iOSAL_Task_Create.
 *          If successful, the handle will be reset to NULL.
 */
int iOSAL_Task_Delete( void** ppvTaskHandle );

/**
 * iOSAL_Task_Suspend() - Suspend any OSAL task.
 *
 * @pvTaskHandle:   Pointer that will be cast to appropriate OS Task Handle data type
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_NOT_STARTED      OS has not been started
 *
 * @note    Setting pvTaskHandle NULL, should cause calling task to be suspended
 */
int iOSAL_Task_Suspend( void* pvTaskHandle );

/**
 * iOSAL_Task_Resume() - Resume any OSAL task.
 *
 * @pvTaskHandle:   Pointer that will be cast to appropriate OS Task Handle data type
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_NOT_STARTED      OS has not been started
 *
 * @note    Setting pvTaskHandle NULL, should cause calling task to be resumed
 */
int iOSAL_Task_Resume( void* pvTaskHandle );

/**
 * iOSAL_Task_SleepTicks() - Delay a task for a given number of ticks.
 *
 * @ulSleepTicks:  The amount of time, in tick periods, that the calling task should block.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *          OSAL_ERRORS_OS_NOT_STARTED      OS has not been started
 *
 */
int iOSAL_Task_SleepTicks( uint32_t ulSleepTicks );

/**
 * iOSAL_Task_SleepMs() - Delay a task for a given number of ms.
 *
 * @ulSleepMs:  The amount of time, in ms, that the calling task should block.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *          OSAL_ERRORS_OS_NOT_STARTED      OS has not been started
 *
 */
int iOSAL_Task_SleepMs( uint32_t ulSleepMs );

/*****************************************************************************/
/* Semaphore APIs                                                            */
/*****************************************************************************/

/**
 * iOSAL_Semaphore_Create() - Creates a binary or counting semaphore, and sets OS Semaphore Handle by which the semaphore can be referenced.
 *
 * @ppvSemHandle: Pointer-to-pointer that will be cast to appropriate OS Semaphore Handle data type.
 * @ullCount:     The initial count value assigned to the semaphore when it is created.
 * @ullBucket:    The maximum count value that can be reached.
 * @pcSemName:    Descriptive name for the semaphore.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    The Semaphore Handle must be initialised as NULL.
 *          To create a binary semaphore, set ullBucket to 1.
 */
int iOSAL_Semaphore_Create( void** ppvSemHandle,
                            uint32_t ullCount,
                            uint32_t ullBucket,
                            const char* pcSemName );

/**
 * iOSAL_Semaphore_Destroy() - Deletes the binary or counting semaphore, to which the handle refers.
 *
 * @ppvSemHandle: Pointer-to-pointer that will be cast to appropriate OS Semaphore Handle data type.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *
 * @note    If successful, the handle will be reset to NULL.
 */
int iOSAL_Semaphore_Destroy( void** ppvSemHandle );

/**
 * iOSAL_Semaphore_Pend() - Pends to / obtains a previously created semaphore, to which the handle refers.
 *
 * @pvSemHandle: Pointer that will be cast to appropriate OS Semaphore Handle data type.
 * @ulTimeoutMs: Timeout in ms to wait for a semaphore to become available.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_Semaphore_Pend( void* pvSemHandle, uint32_t ulTimeoutMs );

/**
 * iOSAL_Semaphore_Post() - Posts / Releases a previously created semaphore, to which the handle refers.
 *
 * @pvSemHandle: Pointer that will be cast to appropriate OS Semaphore Handle data type
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    If calling from an ISR, iOSAL_Semaphore_PostFromISR is recommended.
 */
int iOSAL_Semaphore_Post( void* pvSemHandle );

/**
 * iOSAL_Semaphore_PostFromISR() - A version of iOSAL_Semaphore_Post() that can be called from an ISR.
 *
 * @pvSemHandle: Pointer that will be cast to appropriate OS Semaphore Handle data type
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_Semaphore_PostFromISR( void* pvSemHandle );

/*****************************************************************************/
/* Mutex APIs                                                                */
/*****************************************************************************/

/**
 * iOSAL_Mutex_Create() - Creates a Mutex, and sets OS Mutex Handle by which the Mutex can be referenced.
 *
 * @ppvMutexHandle: Pointer-to-pointer that will be cast to appropriate OS Mutex Handle data type
 * @pcMutexName:    Descriptive name for the Mutex
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    The Mutex Handle must be initialised as NULL.
 */
int iOSAL_Mutex_Create( void**      ppvMutexHandle,
                        const char* pcMutexName );

/**
 * iOSAL_Mutex_Destroy() - Deletes a Mutex, to which the handle refers.
 *
 * @ppvMutexHandle: Pointer-to-pointer that will be cast to appropriate OS Mutex Handle data type
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    If successful, the handle will be reset to NULL.
 */
int iOSAL_Mutex_Destroy( void** ppvMutexHandle );

/**
 * iOSAL_Mutex_Take() - btains a previously created Mutex, to which the handle refers.
 *
 * @pvMutexHandle: Pointer that will be cast to appropriate OS Mutex Handle data type.
 * @ulTimeoutMs: Timeout in ms to wait for a Mutex to become available.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_Mutex_Take( void*    pvMutexHandle,
                      uint32_t ulTimeoutMs );

/**
 * iOSAL_Mutex_Release() - Releases a previously created Mutex, to which the handle refers.
 *
 * @pvMutexHandle: Pointer that will be cast to appropriate OS Mutex Handle data type.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_Mutex_Release( void* pvMutexHandle );

/*****************************************************************************/
/* Mailbox APIs                                                              */
/*****************************************************************************/

/**
 * iOSAL_MBox_Create() - Creates a new MailBox, and sets OS MailBox Handle by which the MailBox can be referenced.
 *
 * @ppvMBoxHandle: Pointer-to-pointer that will be cast to appropriate OS MailBox Handle data type.
 * @ulMBoxLength:  The max number of items the MailBox can hold.
 * @ulItemSize:    Size (Bytes) of each item in the MailBox.
 * @pcMBoxName:    Descriptive name for the MailBox.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    The MailBox Handle must be initialised as NULL.
 */
int iOSAL_MBox_Create( void**      ppvMBoxHandle,
                       uint32_t    ulMBoxLength,
                       uint32_t    ulItemSize,
                       const char* pcMBoxName );

/**
 * iOSAL_MBox_Destroy() - Resets a MailBox, to which the handle refers.
 *
 * @ppvMBoxHandle: Pointer-to-pointer that will be cast to appropriate OS MailBox Handle data type.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    If successful, the handle will be reset to NULL.
 */
int iOSAL_MBox_Destroy( void** ppvMBoxHandle );

/**
 * iOSAL_MBox_Pend() - Recieve an item from a message Mailbox, to which the handle refers.
 *
 * @pvMBoxHandle: Pointer that will be cast to appropriate OS Mailbox Handle data type.
 * @pvMBoxBuffer: A pointer to the buffer into which the received item will be copied.
 * @ulTimeoutMs:  The maximum amount of time (ms) the task should block waiting for an item to receive.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_MBox_Pend( void*    pvMBoxHandle,
                     void*    pvMBoxBuffer,
                     uint32_t ulTimeoutMs );

/**
 * iOSAL_MBox_Post() - Posts an item onto a MailBox, to which the handle refers.
 *
 * @pvMBoxHandle: Pointer that will be cast to appropriate OS MailBox Handle data type.
 * @pvMBoxItem:   A pointer to the item that is to be placed on the mailBox.
 * @ulTimeoutMs:  The maximum amount of time (ms) the task should block waiting for space to become available on the mailBox.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    If calling from an ISR, iOSAL_MBox_PostFromISR is recommended.
 */
int iOSAL_MBox_Post( void*    pvMBoxHandle,
                     void*    pvMBoxItem,
                     uint32_t ulTimeoutMs );

/**
 * @iOSAL_MBox_PostFromISR() - A version of iOSAL_MBox_Post() that can be called from an ISR.
 *
 * @pvMBoxHandle: Pointer that will be cast to appropriate OS MailBox Handle data type.
 * @pvMBoxItem:   A pointer to the item that is to be placed on the mailBox.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_MBox_PostFromISR( void* pvMBoxHandle, void* pvMBoxItem );

/*****************************************************************************/
/* Event APIs                                                                */
/*****************************************************************************/

/**
 * iOSAL_EventFlag_Create() - Creates a new Event Flag group, and sets OS Handle by which the Event Flag group can be referenced.
 *
 * @ppvEventFlagHandle: Pointer-to-pointer that will be cast to appropriate OS Event Flag Handle data type.
 * @pcEventFlagName:    Descriptive name for the MailBox.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    The Event Flag Handle must be initialised as NULL.
 */
int iOSAL_EventFlag_Create( void** ppvEventFlagHandle, const char* pcEventFlagName );

/**
 * iOSAL_EventFlag_Destroy() - Deletes an Event Flag group, to which the handle refers.
 *
 * @pvEventFlagHandle: Pointer that will be cast to appropriate OS Event Flag Handle data type.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    If successful, the handle will be reset to NULL.
 */
int iOSAL_EventFlag_Destroy( void** ppvEventFlagHandle );

/**
 * iOSAL_EventFlag_Pend() - Pend task to Wait for a bit or group of bits to become set.
 *
 * @pvEventFlagHandle: Pointer that will be cast to appropriate OS Event Flag Handle data type.
 * @ulFlagWait:        Bitwise value to specify the bits to wait on being set or cleared.
 * @ulTimeoutMs:       The maximum amount of time (ms) the task should block waiting specified bits to be set.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    This function cannot be called from an ISR.
 */
int iOSAL_EventFlag_Pend( void* pvEventFlagHandle, uint32_t ulFlagWait, uint32_t ulTimeoutMs );

/**
 * iOSAL_EventFlag_Post() - Sets or clears event flag bits, to which the handle refers.
 *
 * @pvEventFlagHandle: Pointer that will be cast to appropriate OS Event Flag Handle data type.
 * @ulFlagSet:         Bitwise value to specify the bits to be set or cleared.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    If calling from an ISR, iOSAL_EventFlag_PostFromISR is recommended.
 */
int iOSAL_EventFlag_Post( void* pvEventFlagHandle, uint32_t ulFlagSet );

/**
 * iOSAL_EventFlag_PostFromISR() - A version of iOSAL_EventFlag_Post() that can be called from an ISR.
 *
 * @pvEventFlagHandle: Pointer that will be cast to appropriate OS Event Flag Handle data type.
 * @ulFlagSet:         Bitwise value to specify the bits to be set or cleared.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_EventFlag_PostFromISR( void* pvEventFlagHandle, uint32_t ulFlagSet );

/*****************************************************************************/
/* Timer APIs                                                                */
/*****************************************************************************/

/**
 * iOSAL_Timer_Create() - Creates a new software timer instance, and sets OS Handle by which the Timer can be referenced.
 *
 * @ppvTimerHandle:  Pointer-to-pointer that will be cast to appropriate OS Timer Handle data type.
 * @xTimerConfig:    Timer config defining if timer is one-shot or periodic
 * @pvTimerCallback: The function to call when the timer expires.
 * @pcTimerName:     A human readable text name assigned to the timer.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    The Timer Handle must be initialised as NULL.
 */
int iOSAL_Timer_Create( void** ppvTimerHandle,
                        OSAL_TIMER_CONFIG xTimerConfig,
                        void ( *pvTimerCallback )( void* pvTimerHandle ),
                        const char* pcTimerName );

/**
 * iOSAL_Timer_Destroy() - Deletes a Timer, to which the handle refers.
 *
 * @ppvTimerHandle: Pointer-to-pointer that will be cast to appropriate OS Timer Handle data type.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 * @note    If successful, the handle will be reset to NULL.
 */
int iOSAL_Timer_Destroy( void** ppvTimerHandle );

/**
 * iOSAL_Timer_Start() - Starts a Timer, to which the handle refers.
 *
 * @pvTimerHandle: Pointer that will be cast to appropriate OS Timer Handle data type.
 * @ulDurationMs:  Number of ms until the timer callback is triggered.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_Timer_Start( void* pvTimerHandle, uint32_t ulDurationMs );

/**
 * iOSAL_Timer_Stop() - Stops a Timer, to which the handle refers.
 *
 * @pvTimerHandle: Pointer that will be cast to appropriate OS Timer Handle data type.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_Timer_Stop( void* pvTimerHandle );

/**
 * iOSAL_Timer_Reset() - Re-Starts a Timer, to which the handle refers.
 *
 * @pvTimerHandle: Pointer that will be cast to appropriate OS Timer Handle data type.
 * @ulDurationMs:  Number of ms until the timer callback is triggered.
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_INVALID_HANDLE      invalid / un-initialised handle passed into to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_Timer_Reset( void* pvTimerHandle, uint32_t ulDurationMs );

/*****************************************************************************/
/* Interrupt APIs                                                            */
/*****************************************************************************/

/**
 * iOSAL_Interrupt_Setup() - Sets up interrupt handler callback with appropriate interrupt ID
 *
 * @ucInterruptID:      Interrupt ID (defined in BSP)
 * @pvInterruptHandler: Pointer to interrupt handler callback function.
 * @pvCallBackRef:      Param to be passed into interrupt handler callback function
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_Interrupt_Setup( uint8_t ucInterruptID,
                           void    ( *pvInterruptHandler )( void* pvCallBackRef ),
                           void*   pvCallBackRef );

/**
 * iOSAL_Interrupt_Enable() - Enable OSAL interrupts
 *
 * @ucInterruptID: Interrupt ID (defined in BSP)
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_Interrupt_Enable( uint8_t ucInterruptID );

/**
 * iOSAL_Interrupt_Disable() - Disable OSAL interrupts
 *
 * @ucInterruptID: Interrupt ID (defined in BSP)
 *
 * @return  OSAL_ERRORS_NONE                no errors, call was successful
 *          OSAL_ERRORS_PARAMS              invalid parameters passed in to function
 *          OSAL_ERRORS_OS_IMPLEMENTATION   error code returned from os implementation
 *
 */
int iOSAL_Interrupt_Disable( uint8_t ucInterruptID );

/*****************************************************************************/
/* Thread safe APIs                                                          */
/*****************************************************************************/

/**
 * vOSAL_EnterCritical() - Mark the start of critical code region.
 *
 * @return  N/A.
 *
 */
void vOSAL_EnterCritical( void );

/**
 * vOSAL_ExitCritical() - Mark the end of critical code region.
 *
 * @return  N/A.
 *
 */
void vOSAL_ExitCritical( void );

/**
 * pvOSAL_MemAlloc() - OSAL wrapper for task/thread safe memory allocation.
 *
 * @xSize: The number of bytes to allocate.
 *
 * @return  Pointer to the beginning of newly allocated memory.
 *          NULL if unsuccessful.
 *
 */
void* pvOSAL_MemAlloc( uint32_t xSize );

/**
 * pvOSAL_MemSet() - OSAL wrapper for task/thread safe memory set.
 *
 * @pvDestination: Pointer to the block of memory to be set.
 * @iValue:        The value to be set.
 * @usSize:        The number of bytes to be set, to the specified valve.
 *
 * @return  Pointer to the beginning of newly set memory.
 *          NULL if unsuccessful.
 *
 */
void* pvOSAL_MemSet( void* pvDestination, int iValue, uint32_t usSize );

/**
 * pvOSAL_MemCpy() - OSAL wrapper for task/thread safe memory copy.
 *
 * @pvDestination: Pointer to the destination where the content is to be copied.
 * @pvSource:      Pointer to the source of data to be copied.
 * @usSize:        The number of bytes to copy.
 *
 * @return  Pointer to the destination where the content is copied.
 *          NULL if unsuccessful.
 *
 */
void* pvOSAL_MemCpy( void* pvDestination, const void* pvSource, uint32_t usSize );

/**
 * vOSAL_MemFree() - OSAL wrapper for task/thread safe memory deallocation.
 *
 * @ppv: Pointer-to-pointer to the memory to deallocate.
 *
 * @return  N/A.
 *
 * @note    After free'ing the allocated memory, this function will also
 *          nullify the pointer to the deallocated memory.
 */
void vOSAL_MemFree( void** ppv );

/**
 * vOSAL_MemMove() - OSAL wrapper for task/thread safe memory movement.
 *
 * @pvDestination: Pointer to the destination array where the content is to be copied,
 *                 type-casted to a pointer of type void*.
 * @pvSource       Pointer to the source of data to be copied, type-casted to a pointer of type const void*.
 * @usPayload_size Number of bytes to copy.
 *
 * @return  N/A.
 *
 */
void vOSAL_MemMove( void *pvDestination, void *pvSource, uint32_t usPayload_size );

/**
 * vOSAL_Printf() - OSAL wrapper for task/thread safe prints.
 *
 * @pcFormat  C string that contains the text to be written.
 *
 * @return  N/A.
 *
 */
void vOSAL_Printf( const char* pcFormat, ... );

/**
 * cOSAL_GetChar() - OSAL wrapper for task/thread safe char reading.
 *
 * @return  character read from STDIN.
 *
 */
char cOSAL_GetChar( void );

/**
 * pcOSAL_StrNCpy() - OSAL wrapper for task/thread safe string copy.
 *
 * @pcDestination: Pointer to the destination char array where the content is to be copied.
 * @pcSource:      Pointer to the string to be copied.
 * @usSize:        The number of bytes to copy.
 *
 * @return  Pointer to the destination where the string is copied.
 *          NULL if unsuccessful.
 *
 */
char* pcOSAL_StrNCpy( char *pcDestination, const char *pcSource, uint32_t usSize );

/**
 * iOSAL_MemCmp() - OSAL wrapper for task/thread safe memory compare.
 *
 * @pvMemoryOne: Pointer to the first memory block.
 * @pvMemoryTwo: Pointer to the second memory block.
 * @usSize:      Number of bytes to compare.
 *
 * @return  0 if the the contents of both memory blocks are equal, non 0 if not.
 *
 */
int iOSAL_MemCmp( const void  *pvMemoryOne, const void *pvMemoryTwo, uint32_t usSize );



/*****************************************************************************/
/* Debug Stats APIs                                                          */
/*****************************************************************************/

/**
 * vOSAL_PrintAllStats() - Prints OSAL debug stats.
 *
 * @eVerbosity:  Verbosity level for the debug data.
 * @eStatType:   Type of stat to print.
 *
 */
void vOSAL_PrintAllStats( OSAL_STATS_VERBOSITY eVerbosity, OSAL_STATS_TYPE eStatType );

/**
 * vOSAL_ClearAllStats() - Clears debug stats and frees associated memory.
 *
 */
void vOSAL_ClearAllStats( void );

#endif /* _OSAL_H_ */
