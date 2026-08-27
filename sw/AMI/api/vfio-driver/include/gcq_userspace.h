/**
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * GCQ (Generic Command Queue) Userspace Driver
 *
 * This header provides the public API for the userspace GCQ driver,
 * enabling direct hardware access to GCQ ring buffers without kernel involvement.
 *
 * @file gcq_userspace.h
 */

#ifndef _GCQ_USERSPACE_H_
#define _GCQ_USERSPACE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/

#define GCQ_UDID_LEN        16

/**
 * GCQ_HEARTBEAT_CID - Reserved Command ID for background heartbeat packets.
 *
 * This CID is used exclusively by the heartbeat background thread.  It is
 * never returned by ami_gcq_allocate_cid() (which starts at 1 and wraps
 * well below this value in practice) so it will never collide with a
 * normal in-flight command.
 *
 * Callers of ami_gcq_wait_response() automatically discard any completion
 * carrying this CID so heartbeat responses never stall command processing.
 */
#define GCQ_HEARTBEAT_CID   0x8000u

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/**
 * @enum GCQ_ERRORS
 * @brief Return values from GCQ userspace functions
 */
typedef enum
{
    GCQ_ERRORS_NONE                      = 0,   /* Success */
    GCQ_ERRORS_DRIVER_NOT_INITIALISED    = 1,   /* gcq_userspace_init() not called */
    GCQ_ERRORS_DRIVER_IN_USE             = 2,   /* Already initialized */
    GCQ_ERRORS_NO_FREE_INSTANCES         = 3,   /* Too many contexts */
    GCQ_ERRORS_INVALID_INSTANCE          = 4,   /* Invalid context */
    GCQ_ERRORS_INVALID_ARG               = 5,   /* NULL pointer or bad value */
    GCQ_ERRORS_INVALID_HANDLE            = 6,   /* Invalid handle */
    GCQ_ERRORS_INVALID_SLOT_SIZE         = 7,   /* Slot size incorrect */
    GCQ_ERRORS_INVALID_VERSION           = 8,   /* Version mismatch */
    GCQ_ERRORS_INVALID_NUM_SLOTS         = 9,   /* Too many/few slots */
    GCQ_ERRORS_INVALID_PARAMS            = 10,  /* Invalid parameters */
    GCQ_ERRORS_CONSUMER_NOT_ATTACHED     = 11,  /* Consumer not ready */
    GCQ_ERRORS_CONSUMER_NOT_AVAILABLE    = 12,  /* Consumer offline */
    GCQ_ERRORS_CONSUMER_NO_DATA_RECEIVED = 13,  /* No data to read */
    GCQ_ERRORS_PRODUCER_NO_FREE_SLOTS    = 14,  /* Ring buffer full */
    GCQ_ERRORS_NOT_SUPPORTED             = 15,  /* Operation not supported */
    GCQ_ERRORS_TIMEOUT                   = 16,  /* Operation timed out */
    GCQ_ERRORS_MMAP_FAILED               = 17,  /* Memory mapping failed */
    GCQ_ERRORS_UIO_OPEN_FAILED           = 18,  /* [DEPRECATED] Cannot open UIO device */
    GCQ_ERRORS_VFIO_OPEN_FAILED          = 19,  /* Cannot open VFIO device / map BAR0 */
    GCQ_ERRORS_BDF_NOT_SET               = 20,  /* use_metal=true but gcq_userspace_set_bdf() not called */

    MAX_GCQ_ERRORS

} GCQ_ERRORS;

/*****************************************************************************/
/* Structs                                                                   */
/*****************************************************************************/

/**
 * @struct GCQVersion
 * @brief Version information
 */
typedef struct
{
    uint8_t major;
    uint8_t minor;
    uint8_t patch;

} GCQVersion;

/**
 * @struct GCQCfg
 * @brief Configuration for a GCQ instance
 */
typedef struct
{
    uint64_t    ullBaseAddr;     /* Base address for GCQ registers */
    uint64_t    ullRingAddr;     /* Base address for ring buffer memory */
    uint32_t    ulRingLength;    /* Total ring buffer size in bytes */
    uint32_t    ulCQSlotSize;    /* Completion Queue slot size */
    uint32_t    ulSQSlotSize;    /* Submission Queue slot size */
    uint8_t     udid[GCQ_UDID_LEN]; /* Unique device ID */

} GCQCfg;

/**
 * @struct gcq_metal_ctx
 * @brief Opaque context handle for GCQ instance
 *
 * This is an opaque pointer - internal structure not exposed to users
 */
typedef struct gcq_metal_ctx_internal gcq_metal_ctx;

/*****************************************************************************/
/* Public Functions                                                          */
/*****************************************************************************/

/**
 * @brief Initialize the GCQ userspace driver (one-time global init)
 *
 * Must be called once before creating any GCQ contexts.
 * Thread-safe.
 *
 * @return GCQ_ERRORS_NONE on success, error code otherwise
 */
uint32_t gcq_userspace_init(void);

/**
 * @brief Tear down global GCQ driver state (pair with gcq_userspace_init)
 */
uint32_t gcq_userspace_deinit(void);

/**
 * @brief Create a GCQ context for a specific device.
 *
 * Allocates resources but does not initialize hardware yet.
 * Call gcq_userspace_open() after this to map BARs and start communication.
 *
 * @param ctx_out   Pointer to receive the created context handle.
 * @param dev_num   UIO device number when use_metal=false (e.g. 0 for /dev/uio0).
 *                  Ignored when use_metal=true; the BDF set via
 *                  gcq_userspace_set_bdf() is used instead.
 * @param use_metal If false: use the Linux UIO /dev/uioX + mmap path.
 *                  If true:  use the libmetal / VFIO path; caller must call
 *                  gcq_userspace_set_bdf() before gcq_userspace_open().
 *
 * @return GCQ_ERRORS_NONE on success, error code otherwise.
 */
uint32_t gcq_userspace_create_context(gcq_metal_ctx **ctx_out,
				      int dev_num,
				      bool use_metal);

/**
 * @brief Set the PCI BDF string used for VFIO device open.
 *
 * Must be called when use_metal=true, before gcq_userspace_open().
 * Has no effect when use_metal=false.
 *
 * @param ctx Context handle from gcq_userspace_create_context().
 * @param bdf PCI BDF string in the form "DDDD:BB:DD.F",
 *            e.g. "0000:e2:00.0".
 *
 * @return GCQ_ERRORS_NONE on success, GCQ_ERRORS_INVALID_HANDLE if ctx is NULL.
 */
uint32_t gcq_userspace_set_bdf(gcq_metal_ctx *ctx, const char *bdf);

/**
 * @brief Override the maximum seconds to wait for AMC ready during open.
 *
 * Set to 0 to fail immediately when AMC magic is not yet valid. This is
 * appropriate for device enumeration (overview) where blocking 60 s would
 * hold the VFIO group fd open and prevent other callers.
 * Default (set by gcq_userspace_create_context) is AMC_READY_MAX_WAIT_SEC.
 *
 * @param ctx     Context handle.
 * @param seconds Max wait time in seconds (0 = no wait).
 *
 * @return GCQ_ERRORS_NONE on success, GCQ_ERRORS_INVALID_HANDLE if ctx is NULL.
 */
uint32_t gcq_userspace_set_amc_timeout(gcq_metal_ctx *ctx, uint32_t seconds);

/**
 * @brief Issue VFIO_DEVICE_RESET before closing the context (VFIO backend only).
 *
 * Best-effort PCIe reset while the device fd is still open.  Ignored for UIO.
 *
 * @return GCQ_ERRORS_NONE on success or if reset is unsupported/non-fatal.
 */
uint32_t gcq_userspace_vfio_pcie_reset(gcq_metal_ctx *ctx);

/**
 * @brief Open/initialize a GCQ instance
 *
 * Maps BAR regions, initializes ring buffers, and prepares for communication.
 *
 * @param ctx    Context handle from gcq_userspace_create_context()
 * @param cfg    Configuration parameters (addresses, sizes, etc.)
 *
 * @return GCQ_ERRORS_NONE on success, error code otherwise
 */
uint32_t gcq_userspace_open(gcq_metal_ctx *ctx, const GCQCfg *cfg);

/**
 * @brief Close a GCQ instance
 *
 * Unmaps memory and releases resources, but does not free the context.
 * Context can be re-opened with gcq_userspace_open() if needed.
 *
 * @param ctx    Context handle
 *
 * @return GCQ_ERRORS_NONE on success, error code otherwise
 */
uint32_t gcq_userspace_close(gcq_metal_ctx *ctx);

/**
 * @brief Destroy a GCQ context and free all resources
 *
 * Closes the context if open, then frees memory.
 * After this call, the context handle is invalid.
 *
 * @param ctx    Context handle to destroy
 *
 * @return GCQ_ERRORS_NONE on success, error code otherwise
 */
uint32_t gcq_userspace_destroy_context(gcq_metal_ctx *ctx);

/**
 * @brief Write data to the GCQ (producer operation)
 *
 * Writes data to the Submission Queue (SQ) and notifies the consumer.
 * This sends a command to the AMC firmware.
 *
 * @param ctx          Context handle
 * @param data         Data buffer to write
 * @param size         Number of bytes to write
 * @param timeout_ms   Timeout in milliseconds (0 = no wait, -1 = wait forever)
 *
 * @return GCQ_ERRORS_NONE on success, error code otherwise
 */
uint32_t gcq_userspace_write(gcq_metal_ctx *ctx,
                              const uint8_t *data,
                              uint32_t size,
                              uint32_t timeout_ms);

/**
 * @brief Read data from the GCQ (consumer operation)
 *
 * Reads data from the Completion Queue (CQ).
 * This receives a response from the AMC firmware.
 *
 * @param ctx          Context handle
 * @param data         Buffer to receive data
 * @param size         Pointer to buffer size; updated with bytes actually read
 * @param timeout_ms   Timeout in milliseconds (0 = no wait, -1 = wait forever)
 *
 * @return GCQ_ERRORS_NONE on success, error code otherwise
 */
uint32_t gcq_userspace_read(gcq_metal_ctx *ctx,
                             uint8_t *data,
                             uint32_t *size,
                             uint32_t timeout_ms);

/**
 * @brief Get version information
 *
 * @param version    Pointer to receive version info
 *
 * @return GCQ_ERRORS_NONE on success, error code otherwise
 */
uint32_t gcq_userspace_get_version(GCQVersion *version);

/**
 * @brief Get BAR0 base address
 *
 * Returns the BAR0 mmap base pointer for direct memory access.
 * Used internally for SDR and other operations that need to read
 * firmware-written data from shared memory.
 *
 * @param ctx    Context handle
 *
 * @return BAR0 base pointer or NULL on error
 */
void *gcq_userspace_get_bar0(gcq_metal_ctx *ctx);

/**
 * @brief Discard every completion currently visible in the hardware CQ
 *
 * Advances the host CQ consumer until it matches the firmware producer
 * register. Used before/after PDI chunks so the 4-slot ring cannot fill.
 *
 * @param ctx           Context handle
 * @param purged_count  Optional count of slots discarded (may be NULL)
 *
 * @return GCQ_ERRORS_NONE on success
 */
uint32_t gcq_userspace_cq_drain(gcq_metal_ctx *ctx, uint32_t *drained_count);

/**
 * @brief Start the background heartbeat thread (after IDENTIFY/version OK)
 *
 * Legacy ami.ko started heartbeat_health_thread only after version checks
 * in setup_amc().  The logging thread is already running from gcq_open().
 *
 * @return GCQ_ERRORS_NONE on success
 */
uint32_t gcq_userspace_start_heartbeat(gcq_metal_ctx *ctx);

/**
 * @brief Clear persisted AMC log ring offset for a PCI BDF (after reload/POR)
 */
void gcq_userspace_clear_log_index_for_bdf(const char *bdf);

/**
 * @brief Pause or resume the background heartbeat thread
 *
 * While paused, the heartbeat thread stops issuing HEARTBEAT commands so the
 * 4-slot CQ is not polluted with heartbeat completions during a multi-chunk
 * PDI/flash transfer. Pause before starting the transfer and resume (and drain
 * the CQ) once it completes.
 *
 * @param ctx    Context handle
 * @param pause  true to suspend heartbeats, false to resume
 *
 * @return GCQ_ERRORS_NONE on success, GCQ_ERRORS_INVALID_ARG if ctx is NULL
 */
uint32_t gcq_userspace_pause_heartbeat(gcq_metal_ctx *ctx, bool pause);

/**
 * @brief Wait until CQ is empty and firmware has consumed all SQ entries
 */
uint32_t gcq_userspace_wait_idle(gcq_metal_ctx *ctx, uint32_t timeout_ms);

/**
 * @brief Force-sync CQ consumer to firmware producer (attach/recovery only)
 *
 * Do not call between PDI chunks while waiting for a specific CID — use
 * gcq_userspace_cq_drain() instead.
 */
uint32_t gcq_userspace_cq_purge(gcq_metal_ctx *ctx, uint32_t *purged_count);

/**
 * @brief Parse AMC firmware version from the log ring buffer
 *
 * Scans the firmware log messages for the first "X.Y.Z" version string.
 * Falls back to this when the IDENTIFY GCQ command is unavailable.
 *
 * @param ctx    Context handle (BAR0 must be mapped)
 * @param major  Output: major version
 * @param minor  Output: minor version
 * @param patch  Output: patch version
 *
 * @return GCQ_ERRORS_NONE on success, GCQ_ERRORS_CONSUMER_NOT_AVAILABLE
 *         if log is not yet populated or no version string was found
 */
uint32_t gcq_userspace_get_amc_version_from_log(gcq_metal_ctx *ctx,
                                                  uint8_t *major,
                                                  uint8_t *minor,
                                                  uint8_t *patch);

/**
 * gcq_userspace_pause_logging() - Pause the logging thread
 * @ctx: GCQ context
 *
 * Pauses the logging thread to avoid errors during device resets.
 *
 * @return GCQ_ERRORS_NONE on success, GCQ_ERRORS_INVALID_ARG if ctx is NULL
 */
uint32_t gcq_userspace_pause_logging(gcq_metal_ctx *ctx);

/**
 * gcq_userspace_resume_logging() - Resume the logging thread
 * @ctx: GCQ context
 *
 * Resumes the logging thread after device reset.
 *
 * @return GCQ_ERRORS_NONE on success, GCQ_ERRORS_INVALID_ARG if ctx is NULL
 */
uint32_t gcq_userspace_resume_logging(gcq_metal_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _GCQ_USERSPACE_H_ */
