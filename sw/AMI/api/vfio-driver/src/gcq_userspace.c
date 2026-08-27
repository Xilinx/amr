/**
 * Copyright (C) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * GCQ Userspace Driver - V2 Implementation
 *
 * This version follows EXACTLY the same flow as the old kernel driver:
 * - Read AMC shared memory structure at BAR0+AMC_SHARED_MEM_OFFSET
 * - Discover GCQ ring buffer location dynamically
 * - Configure using discovered addresses
 *
 * @file gcq_userspace.c
 */

#include "gcq_userspace.h"
#include "ami_amc_shared_mem.h"
#include "metal_compat.h"
#include "ami_gcq_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stddef.h>
#include <signal.h>
#include <setjmp.h>
#include <linux/vfio.h>
#include <syslog.h>

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/

#define XILINX_SGCQ_SIZE_BYTES          AMC_SHARED_MEM_OFFSET   /* GCQ registers = 4KB */

#define AMC_PROXY_REQUEST_SIZE          512      /* SQ slot size */
#define AMC_PROXY_RESPONSE_SIZE         16       /* CQ slot size */

#define GCQ_ALLOC_MAGIC                 0x5847513F  /* GCQ ring buffer magic */
#define GCQ_VER_MAJOR                   1
#define GCQ_VER_MINOR                   0
#define GCQ_VER_PATCH                   0
#define GCQ_MIN_NUM_SLOTS               2

#define GCQ_INSTANCE_UPPER_FIREWALL     0xBEEFCAFE
#define GCQ_INSTANCE_LOWER_FIREWALL     0xDEADFACE

/* Producer register offsets */
#define GCQ_PRODUCER_SQ_TAIL_POINTER    0x0000  /* RW */
#define GCQ_CONSUMER_CQ_TAIL_POINTER    0x0100  /* RW */

#define CHECK_32BIT_ALIGNMENT(x)        (0 == ((x) & 0x3))

#define AMC_READY_MAX_WAIT_SEC          60

/* Firmware log ring buffer (matches legacy kernel driver constants) */
#define AMC_LOG_ENTRY_SIZE              96   /* bytes per log record (char buff[96]) */
#define AMC_LOG_MAX_RECS                50   /* max records in ring */

/* Background threads — match legacy ami_amc_control.c */
#define HEARTBEAT_REQUEST_INTERVAL_MS     500
#define REQUEST_HEARTBEAT_TIMEOUT_MS      500
#define HEARTBEAT_FAIL_THRESHOLD          3
#define LOGGING_SLEEP_INTERVAL_MS         500

/* Firmware log forwarding is opt-in. By default, do not emit AMC boot/runtime
 * logs into the calling application's stderr stream. Set AMI_FW_LOG_STDERR=1
 * to re-enable the old behaviour for debugging. Firmware logs are still sent
 * to the standard userspace logging path (syslog/journald). */
#define AMI_FW_LOG_STDERR_ENV           "AMI_FW_LOG_STDERR"
#define AMI_SYSLOG_IDENT                 "ami_tool"

/* Debug */
#ifndef GCQ_DEBUG_ENABLE
#define GCQ_DEBUG_ENABLE 0
#endif

#if GCQ_DEBUG_ENABLE
#define GCQ_DEBUG(fmt, ...) printf("[GCQ] " fmt, ##__VA_ARGS__)
#else
#define GCQ_DEBUG(fmt, ...) do {} while(0)
#endif

/******************************************************************************/
/* Enums                                                                      */
/******************************************************************************/

typedef enum
{
    GCQ_STATE_CLOSED      = 0,
    GCQ_STATE_INIT        = 1,
    GCQ_STATE_OPENED      = 2,
    GCQ_STATE_ATTACHED    = 3,
} GCQ_STATE;

/******************************************************************************/
/* Structs                                                                    */
/******************************************************************************/

/**
 * @struct GCQHeader
 * @brief Ring buffer header format (matches firmware)
 */
typedef struct
{
    uint32_t ulHdrMagic;
    uint32_t ulHdrVersion;
    uint32_t ulHdrNumSlots;
    uint32_t ulHdrSQOffset;
    uint32_t ulHdrSQSlotSize;
    uint32_t ulHdrCQOffset;
    uint32_t ulHdrSQConsumed;   /* [6] Firmware writes */
    uint32_t ulHdrCQConsumed;   /* [7] Host writes */
    uint32_t ulHdrFlags;        /* [8] */
    uint32_t ulHdrSQProduced;   /* [9] Host writes */
    uint32_t ulHdrCQProduced;   /* [10] Firmware writes */
} __attribute__((packed)) GCQHeader;

/**
 * @struct GCQRing
 */
typedef struct
{
    uint32_t    ulRingNumSlots;
    uint32_t    ulRingSlotSize;
    uint32_t    ulRingProduced;         /* Local cache */
    uint32_t    ulRingConsumed;         /* Local cache */
    uint64_t    ullRingProducedAddr;    /* Where to write produced pointer */
    uint64_t    ullRingConsumedAddr;    /* Where to read consumed pointer */
    uint64_t    ullRingSlotAddr;        /* Base of slot array */
} GCQRing;

/**
 * @struct gcq_metal_ctx_internal
 */
struct gcq_metal_ctx_internal
{
    uint32_t        ulUpperFirewall;

    /* Backend selection: false = Linux UIO, true = VFIO via metal_io_region */
    bool            use_metal;

    /* PCI BDF string (used when use_metal == true), e.g. "0000:e2:00.0" */
    char            bdf_str[32];

    /* UIO access (use_metal == false) */
    int             uio_fd;
    int             uio_num;

    /* VFIO handles (use_metal == true; -1 when not in use) */
    int             vfio_container_fd;
    int             vfio_group_fd;
    int             vfio_device_fd;

    /* BAR0 I/O region — always populated after gcq_userspace_open().
     * bar0_io.virt is the same pointer stored in bar_base (kept for
     * backwards-compat with code that uses bar_base directly). */
    struct metal_io_region bar0_io;

    /* Memory mappings - everything in BAR0 */
    void            *bar_base;         /* == bar0_io.virt after open */
    size_t          bar_size;          /* == bar0_io.size after open */

    /* Calculated addresses (virtual pointers into BAR0) */
    void            *gcq_base_virt_addr;           /* BAR0 + 0x0000 (registers) */
    void            *gcq_payload_base_virt_addr;   /* BAR0 + AMC_SHARED_MEM_OFFSET (shared mem) */
    void            *gcq_ring_buf_base_virt_addr;  /* BAR0 + AMC_SHARED_MEM_OFFSET + ring_buf_off */

    /* AMC shared memory (read from hardware) */
    struct amc_shared_mem amc_shared_mem;

    /* Ring buffers */
    GCQRing         sq;
    GCQRing         cq;
    GCQRing         *producer;          /* Points to SQ */
    GCQRing         *consumer;          /* Points to CQ */

    /* State */
    GCQ_STATE       state;
    bool            consumer_attached;
    bool            is_initialized;

    /* Synchronization */
    pthread_mutex_t lock;
    pthread_mutex_t log_drain_lock;

    /* Heartbeat background thread */
    pthread_t       hb_thread;
    volatile bool   hb_running;
    volatile bool   hb_paused;   /* Skip heartbeats during PDI/flash (userspace CQ guard) */
    volatile bool   gcq_halted;  /* Set after heartbeat fatal (legacy stop_gcq_services) */

    /* Firmware log background thread */
    pthread_t       log_thread;
    volatile bool   log_running;
    uint32_t        last_log_index;  /* Last seen log ring write index */

    /* AMC magic wait: 0 = fail immediately if not ready (for overview/enum) */
    uint32_t        amc_wait_sec;    /* Max seconds to wait for AMC ready */

    /* Per-context GCQ session state (isolates devices for concurrent probing).
     * next_cid: command-ID allocator for this device.
     * pending:  out-of-order completions read from this device's CQ. */
    uint16_t                 next_cid;
    struct gcq_pending_queue pending;

    uint32_t        ulLowerFirewall;
};

/* Accessors so ami_gcq_common.c can reach per-context session state without
 * seeing the full (private) ctx layout. */
struct gcq_pending_queue *gcq_userspace_pending(gcq_metal_ctx *ctx)
{
    return ctx ? &ctx->pending : NULL;
}

uint16_t *gcq_userspace_cid_counter(gcq_metal_ctx *ctx)
{
    return ctx ? &ctx->next_cid : NULL;
}

static bool gcq_amc_magic_valid(uint32_t magic)
{
    return (magic == AMC_MAGIC_NUMBER);
}

static sigjmp_buf s_sigbus_jmp;

static bool gcq_fw_log_stderr_enabled(void)
{
    const char *env = getenv(AMI_FW_LOG_STDERR_ENV);

    if (!env)
        return false;

    return (strcmp(env, "0") != 0) && (strcmp(env, "") != 0);
}

static void gcq_fw_log_emit(const char *msg)
{
    if (!msg || msg[0] == '\0')
        return;

    syslog(LOG_INFO, "[AMC] %s", msg);

    if (gcq_fw_log_stderr_enabled())
        fprintf(stderr, "[AMC] %s\n", msg);
}

/* GCQ never reached ATTACHED — heartbeat thread was not started. */
static void gcq_log_open_failed(uint32_t err, const char *detail)
{
    syslog(LOG_ERR,
           "GCQ open failed (code %u): %s — heartbeat monitoring not started",
           err, detail ? detail : "unknown");
}

/**
 * dump_amc_log() - Drain new AMC log records (legacy ami_amc_control.c).
 */
static void dump_amc_log(gcq_metal_ctx *ctx)
{
    volatile struct amc_log_msg *lm;
    uint32_t current_log_idx;
    uint32_t i;

    if (!ctx || !ctx->gcq_payload_base_virt_addr)
        return;

    pthread_mutex_lock(&ctx->log_drain_lock);

    lm = (volatile struct amc_log_msg *)
        ((uint8_t *)ctx->gcq_payload_base_virt_addr +
         offsetof(struct amc_shared_mem, log_msg));

    __sync_synchronize();
    current_log_idx = lm->log_msg_index;
    __sync_synchronize();

    if (current_log_idx > AMC_LOG_MAX_RECS) {
        static bool warned;

        if (!warned) {
            fprintf(stderr,
                    "[GCQ] Logging thread error - invalid PCI data read!\n");
            warned = true;
        }
        pthread_mutex_unlock(&ctx->log_drain_lock);
        return;
    }

    i = ctx->last_log_index;
    while (i != current_log_idx) {
        uint32_t buf_off = lm->log_msg_buf_off;
        volatile uint8_t *rec_src;
        char msg[AMC_LOG_ENTRY_SIZE + 1];
        int j;

        if (buf_off == 0)
            break;

        rec_src = (volatile uint8_t *)ctx->gcq_payload_base_virt_addr +
                  buf_off + (i * AMC_LOG_ENTRY_SIZE);

        __sync_synchronize();
        for (j = 0; j < AMC_LOG_ENTRY_SIZE; j++)
            msg[j] = rec_src[j];
        __sync_synchronize();
        msg[AMC_LOG_ENTRY_SIZE] = '\0';

        gcq_fw_log_emit(msg);
        i = (i + 1) % AMC_LOG_MAX_RECS;
    }

    ctx->last_log_index = current_log_idx;
    pthread_mutex_unlock(&ctx->log_drain_lock);
}

static void gcq_log_index_path(gcq_metal_ctx *ctx, char *path, size_t path_len)
{
    char tag[40];
    size_t i;

    if (!ctx || !path || path_len == 0)
        return;

    if (ctx->bdf_str[0] != '\0') {
        snprintf(tag, sizeof(tag), "%s", ctx->bdf_str);
        for (i = 0; tag[i] != '\0'; i++) {
            if (tag[i] == ':')
                tag[i] = '_';
        }
    } else {
        snprintf(tag, sizeof(tag), "uio%d", ctx->uio_num);
    }

    snprintf(path, path_len, "/tmp/ami_log_idx_%s", tag);
}

static void gcq_log_load_index(gcq_metal_ctx *ctx)
{
    char path[128];
    FILE *fp;
    uint32_t saved = 0;

    if (!ctx)
        return;

    gcq_log_index_path(ctx, path, sizeof(path));
    fp = fopen(path, "r");
    if (!fp)
        return;

    if (fscanf(fp, "%u", &saved) == 1 && saved <= AMC_LOG_MAX_RECS)
        ctx->last_log_index = saved;

    fclose(fp);
}

static void gcq_log_save_index(gcq_metal_ctx *ctx)
{
    char path[128];
    FILE *fp;

    if (!ctx)
        return;

    gcq_log_index_path(ctx, path, sizeof(path));
    fp = fopen(path, "w");
    if (!fp)
        return;

    fprintf(fp, "%u\n", ctx->last_log_index);
    fflush(fp);
    fclose(fp);
}

static void gcq_log_clear_saved_index(gcq_metal_ctx *ctx)
{
    char path[128];

    if (!ctx)
        return;

    gcq_log_index_path(ctx, path, sizeof(path));
    unlink(path);
}

void gcq_userspace_clear_log_index_for_bdf(const char *bdf)
{
    char tag[40];
    char path[128];
    size_t i;

    if (!bdf || bdf[0] == '\0')
        return;

    snprintf(tag, sizeof(tag), "%s", bdf);
    for (i = 0; tag[i] != '\0'; i++) {
        if (tag[i] == ':')
            tag[i] = '_';
    }

    snprintf(path, sizeof(path), "/tmp/ami_log_idx_%s", tag);
    unlink(path);
}

static bool gcq_log_flush_ring_enabled(void)
{
    const char *env = getenv("AMI_FW_LOG_FLUSH_RING");

    if (!env)
        return false;

    return (strcmp(env, "0") != 0) && (strcmp(env, "") != 0);
}

static void gcq_sigbus_handler(int sig)
{
    (void)sig;
    siglongjmp(s_sigbus_jmp, 1);
}

/**
 * gcq_read_amc_shared_mem() - Safely copy AMC shared memory from BAR0+AMC_SHARED_MEM_OFFSET.
 *
 * Returns true on success, false if a SIGBUS (PCIe UR / device not ready) is
 * caught during the read.  On false the caller should treat magic as invalid.
 */
static bool gcq_read_amc_shared_mem(gcq_metal_ctx *ctx)
{
    volatile uint8_t *shm_src;
    uint8_t *shm_dest;
    size_t i;
    struct sigaction sa_new = { .sa_handler = gcq_sigbus_handler };
    struct sigaction sa_old;

    sigemptyset(&sa_new.sa_mask);
    sa_new.sa_flags = 0;

    sigaction(SIGBUS, &sa_new, &sa_old);

    if (sigsetjmp(s_sigbus_jmp, 1) != 0) {
        /* SIGBUS caught: PCIe device not responding at BAR0+AMC_SHARED_MEM_OFFSET */
        sigaction(SIGBUS, &sa_old, NULL);
        fprintf(stderr,
                "[GCQ] SIGBUS reading BAR0+AMC_SHARED_MEM_OFFSET — PCIe device not "
                "responding (firmware not loaded or card needs cold reboot)\n");
        memset(&ctx->amc_shared_mem, 0xff, sizeof(ctx->amc_shared_mem));
        return false;
    }

    __sync_synchronize();
    shm_src  = (volatile uint8_t *)ctx->gcq_payload_base_virt_addr;
    shm_dest = (uint8_t *)&ctx->amc_shared_mem;
    for (i = 0; i < sizeof(ctx->amc_shared_mem); i++)
        shm_dest[i] = shm_src[i];
    __sync_synchronize();

    sigaction(SIGBUS, &sa_old, NULL);
    return true;
}

/******************************************************************************/
/* Global Variables                                                           */
/******************************************************************************/

static bool g_gcq_initialized = false;
static pthread_mutex_t g_init_lock = PTHREAD_MUTEX_INITIALIZER;

/******************************************************************************/
/* Private Functions                                                          */
/******************************************************************************/

/**
 * Read 32-bit register/memory
 */
static inline uint32_t gcq_read32(void *addr)
{
    volatile uint32_t *ptr = (volatile uint32_t *)addr;
    uint32_t val = *ptr;
    __sync_synchronize();
    return val;
}

/**
 * Write 32-bit register/memory
 */
static inline void gcq_write32(void *addr, uint32_t value)
{
    volatile uint32_t *ptr = (volatile uint32_t *)addr;
    *ptr = value;
    __sync_synchronize();
}

/**
 * Initialize a ring structure
 */
static void gcq_init_ring(GCQRing *ring,
                          uint64_t produced_addr,
                          uint64_t consumed_addr,
                          uint64_t slot_array_addr,
                          uint32_t num_slots,
                          uint32_t slot_size)
{
    ring->ulRingNumSlots = num_slots;
    ring->ulRingSlotSize = slot_size;
    ring->ulRingProduced = 0;
    ring->ulRingConsumed = 0;
    ring->ullRingProducedAddr = produced_addr;
    ring->ullRingConsumedAddr = consumed_addr;
    ring->ullRingSlotAddr = slot_array_addr;

    GCQ_DEBUG("  Ring init: slots=%u size=%u produced_addr=0x%lx consumed_addr=0x%lx slot_addr=0x%lx\n",
              num_slots, slot_size, produced_addr, consumed_addr, slot_array_addr);
}

static void gcq_cq_discard_slots(GCQRing *ring, uint32_t max_slots);
static void gcq_cq_sync_consumer(gcq_metal_ctx *ctx, GCQRing *ring);

/**
 * Fast-forward to current firmware position (from ami_gcq.c)
 *
 * For SQ (we produce): Set our produced = firmware's consumed
 * For CQ (we consume): Set our consumed = firmware's produced
 */
static void gcq_fast_forward_sq(gcq_metal_ctx *ctx, GCQRing *ring)
{
    uint32_t fw_consumed = 0;
    uint32_t hw_produced = 0;
    uint32_t gap;
    int attempts = 0;
    int max_attempts;

    (void)ctx;

    /*
     * Host is SQ producer: do not run ahead of firmware consumer.
     * If hw_produced > fw_consumed, slots are still in flight — wait for them.
     *
     * CRITICAL FIX: The original 10ms timeout (1000 attempts × 10us) was too short
     * when reopening device after PDI programming. Firmware might still be processing
     * thousands of chunks, and we need to wait for it to fully drain before syncing.
     *
     * Strategy: Use adaptive timeout based on queue backlog:
     * - Small gap (<=10): Wait up to 100ms (normal device open case)
     * - Large gap (>10): Wait up to 5 seconds (post-PDI-programming case)
     */
    fw_consumed = gcq_read32((void *)ring->ullRingConsumedAddr);
    hw_produced = gcq_read32((void *)ring->ullRingProducedAddr);
    gap = hw_produced - fw_consumed;

    if (gap > 10) {
        /* Large backlog detected - give firmware up to 5 seconds to drain */
        max_attempts = 500000;  /* 5 seconds max */
        fprintf(stderr, "[GCQ] Large SQ backlog detected (%u entries), waiting for firmware to drain...\n", gap);
    } else {
        /* Normal case - quick sync */
        max_attempts = 10000;  /* 100ms max */
    }

    while (attempts < max_attempts) {
        fw_consumed = gcq_read32((void *)ring->ullRingConsumedAddr);
        hw_produced = gcq_read32((void *)ring->ullRingProducedAddr);
        if (hw_produced == fw_consumed)
            break;
        usleep(10);  /* 10us polling interval */
        attempts++;
    }

    fw_consumed = gcq_read32((void *)ring->ullRingConsumedAddr);
    ring->ulRingProduced = fw_consumed;

    /* Publish next SQ slot index to hardware */
    gcq_write32((void *)ring->ullRingProducedAddr, fw_consumed);

    if (attempts >= max_attempts && hw_produced != fw_consumed) {
        fprintf(stderr, "[GCQ] WARNING: SQ sync timeout after %d attempts, gap still %u\n",
                attempts, hw_produced - fw_consumed);
    }

    GCQ_DEBUG("  Sync SQ: produced=consumed=%u (hw was %u, gap was %u, attempts=%d)\n",
              fw_consumed, hw_produced, gap, attempts);
}

static void gcq_fast_forward_cq(gcq_metal_ctx *ctx, GCQRing *ring)
{
    gcq_cq_sync_consumer(ctx, ring);
    GCQ_DEBUG("  Fast-forward CQ: consumed=produced=%u\n", ring->ulRingConsumed);
}

/**
 * Attach to consumer (firmware) - from ami_gcq.c:530-600
 */
static uint32_t gcq_attach_consumer(gcq_metal_ctx *ctx)
{
    GCQHeader header;
    uint32_t magic;

    GCQ_DEBUG("Attempting to attach to consumer (firmware)...\n");

    /* Read magic number from ring buffer header */
    magic = gcq_read32(ctx->gcq_ring_buf_base_virt_addr);
    GCQ_DEBUG("  GCQ Header magic: 0x%08x (expected 0x%08x)\n", magic, GCQ_ALLOC_MAGIC);

    if (magic != GCQ_ALLOC_MAGIC) {
        fprintf(stderr,
                "[GCQ] GCQ ring magic invalid at BAR0+AMC_GCQ_RING_OFFSET: 0x%08x (expected 0x%08x)\n",
                magic, GCQ_ALLOC_MAGIC);
        return GCQ_ERRORS_CONSUMER_NOT_AVAILABLE;
    }

    /* Read full header with volatile to ensure fresh data */
    __sync_synchronize();
    volatile uint8_t *hdr_src = (volatile uint8_t *)ctx->gcq_ring_buf_base_virt_addr;
    uint8_t *hdr_dest = (uint8_t *)&header;
    for (size_t i = 0; i < sizeof(header); i++) {
        hdr_dest[i] = hdr_src[i];
    }
    __sync_synchronize();

    /* Validate version */
    uint32_t major = (header.ulHdrVersion >> 16);
    if (major != GCQ_VER_MAJOR) {
        GCQ_DEBUG("  ERROR: Version mismatch: got %u.x, expected %u.x\n",
                  major, GCQ_VER_MAJOR);
        return GCQ_ERRORS_INVALID_VERSION;
    }

    /* Validate slot count matches */
    if (header.ulHdrNumSlots != ctx->sq.ulRingNumSlots) {
        GCQ_DEBUG("  ERROR: Slot count mismatch: header=%u configured=%u\n",
                  header.ulHdrNumSlots, ctx->sq.ulRingNumSlots);
        return GCQ_ERRORS_INVALID_NUM_SLOTS;
    }

    /* Validate slot size matches */
    if (header.ulHdrSQSlotSize != ctx->sq.ulRingSlotSize) {
        GCQ_DEBUG("  ERROR: SQ slot size mismatch: header=%u configured=%u\n",
                  header.ulHdrSQSlotSize, ctx->sq.ulRingSlotSize);
        return GCQ_ERRORS_INVALID_SLOT_SIZE;
    }

    /* Fast-forward to current firmware position */
    gcq_fast_forward_sq(ctx, &ctx->sq);
    gcq_fast_forward_cq(ctx, &ctx->cq);

    ctx->consumer_attached = true;
    ctx->state = GCQ_STATE_ATTACHED;

    GCQ_DEBUG("✓ Successfully attached to consumer!\n");
    return GCQ_ERRORS_NONE;
}

/**
 * Check if data can be consumed from ring
 */
static inline bool gcq_can_consume(GCQRing *ring, void *base)
{
    uint32_t hw_produced = gcq_read32((void *)ring->ullRingProducedAddr);
    return (hw_produced != ring->ulRingConsumed);
}

/**
 * Check if we can produce to ring
 */
static inline bool gcq_can_produce(GCQRing *ring, void *base)
{
    uint32_t hw_consumed = gcq_read32((void *)ring->ullRingConsumedAddr);
    uint32_t next_produced = ring->ulRingProduced + 1;
    return (next_produced != hw_consumed);
}

/* Unread CQ entries (monotonic produced/consumed counters, 4 slots in hardware). */
static inline uint32_t gcq_cq_hw_produced(const GCQRing *ring)
{
    return gcq_read32((void *)ring->ullRingProducedAddr);
}

static inline uint32_t gcq_cq_pending(const GCQRing *ring)
{
    return gcq_cq_hw_produced(ring) - ring->ulRingConsumed;
}

/*
 * Discard up to @max_slots CQ entries and publish consumed to the firmware.
 * When host consumed is far behind (e.g. 0 vs FW produced 27), only the last
 * ulRingNumSlots slots may still hold data — do not loop (produced - consumed)
 * times or we re-read the same four slots and spam warnings.
 */
static void gcq_cq_discard_slots(GCQRing *ring, uint32_t max_slots)
{
    uint8_t dummy[AMC_PROXY_RESPONSE_SIZE];
    uint32_t n = 0;

    while (n < max_slots && gcq_cq_pending(ring) > 0) {
        uint32_t slot_index = ring->ulRingConsumed & (ring->ulRingNumSlots - 1);
        uint64_t slot_addr = ring->ullRingSlotAddr +
                             (uint64_t)ring->ulRingSlotSize * slot_index;

        memcpy(dummy, (void *)slot_addr, sizeof(dummy));
        __sync_synchronize();
        ring->ulRingConsumed++;
        n++;
    }
}

static void gcq_cq_sync_consumer(gcq_metal_ctx *ctx, GCQRing *ring)
{
    uint32_t fw_produced = gcq_cq_hw_produced(ring);
    uint32_t pending = fw_produced - ring->ulRingConsumed;

    (void)ctx;

    if (pending > ring->ulRingNumSlots) {
        /* Host started at 0 after reopen; FW index is cumulative from prior session */
        ring->ulRingConsumed = fw_produced - ring->ulRingNumSlots;
        pending = ring->ulRingNumSlots;
    }

    gcq_cq_discard_slots(ring, pending);

    if (ring->ulRingConsumed != fw_produced)
        ring->ulRingConsumed = fw_produced;

    gcq_write32((void *)ring->ullRingConsumedAddr, ring->ulRingConsumed);
    ring->ulRingProduced = fw_produced;
}

/******************************************************************************/
/* Public API Implementation                                                 */
/******************************************************************************/

/* Legacy amc_event_id heartbeat events (ami_amc_control.h / ami_top.c) */
typedef enum {
    GCQ_HEARTBEAT_EVENT_EXPIRED = 0,
    GCQ_HEARTBEAT_EVENT_VALIDATION,
    GCQ_HEARTBEAT_EVENT_FATAL,
} gcq_heartbeat_event_id;

/**
 * gcq_heartbeat_raise_event() - Mirror legacy amc_event_cb() in ami_top.c.
 */
static void gcq_heartbeat_raise_event(gcq_heartbeat_event_id id, gcq_metal_ctx *ctx)
{
    switch (id) {
    case GCQ_HEARTBEAT_EVENT_EXPIRED:
        syslog(LOG_ERR, "AMC Heartbeat expired event received");
        break;
    case GCQ_HEARTBEAT_EVENT_VALIDATION:
        syslog(LOG_ERR, "AMC Heartbeat validation event received");
        break;
    case GCQ_HEARTBEAT_EVENT_FATAL:
        syslog(LOG_ERR, "AMC Heartbeat fatal event received, stopping sGCQ...");
        if (ctx)
            ctx->gcq_halted = true;
        break;
    default:
        break;
    }
}

/*
 * Sleep up to total_ms, waking every ~25ms to re-check *running. Lets a
 * background thread exit within one step of the stop flag being cleared,
 * instead of blocking a full interval and stalling handle teardown.
 */
static void gcq_interruptible_sleep(volatile bool *running, long total_ms)
{
    const long step_ms = 25;
    struct timespec step = { .tv_sec = 0, .tv_nsec = step_ms * 1000000L };
    long slept;

    for (slept = 0; slept < total_ms && *running; slept += step_ms)
        nanosleep(&step, NULL);
}

/**
 * gcq_heartbeat_thread() - Background thread: periodic AMC heartbeat (legacy).
 *
 * Matches heartbeat_health_thread() in ami_amc_control.c: work first, then
 * HEARTBEAT_REQUEST_INTERVAL_MS sleep; silent on success; PR_ERR + event_cb
 * on failure (syslog/journald here).
 */
static void *gcq_heartbeat_thread(void *arg)
{
    gcq_metal_ctx *ctx = (gcq_metal_ctx *)arg;
    uint8_t request_id = 0;
    int fail_count = 0;
    bool fatal_event_raised = false;

    while (ctx->hb_running) {
        if (ctx->state == GCQ_STATE_ATTACHED && !ctx->hb_paused && !ctx->gcq_halted) {
            if (!fatal_event_raised && fail_count < HEARTBEAT_FAIL_THRESHOLD) {
                struct amc_proxy_cmd_request req = {0};
                struct com_queue_entry resp = {0};
                uint16_t cid;
                uint32_t response_id;
                uint32_t wr;
                int wait_rc;

                cid = ami_gcq_allocate_cid(ctx);
                req.hdr.opcode = AMC_PROXY_CMD_OPCODE_HEARTBEAT;
                req.hdr.state = 1;
                req.hdr.cid = cid;
                req.hdr.count = 1;
                req.raw_payload[0] = request_id;

                wr = gcq_userspace_write(ctx, (uint8_t *)&req,
                                         AMC_PROXY_REQUEST_SIZE,
                                         REQUEST_HEARTBEAT_TIMEOUT_MS);
                if (wr != GCQ_ERRORS_NONE) {
                    syslog(LOG_ERR, "Failed to get the heartbeat msg!");
                    gcq_heartbeat_raise_event(GCQ_HEARTBEAT_EVENT_EXPIRED, ctx);
                    ami_gcq_free_cid(cid);
                    fail_count++;
                } else {
                    wait_rc = ami_gcq_wait_response(ctx, cid, &resp,
                                                    REQUEST_HEARTBEAT_TIMEOUT_MS);
                    if (wait_rc != 0) {
                        syslog(LOG_ERR, "Failed to get the heartbeat msg!");
                        gcq_heartbeat_raise_event(GCQ_HEARTBEAT_EVENT_EXPIRED,
                                                  ctx);
                        ami_gcq_free_cid(cid);
                        fail_count++;
                    } else {
                        response_id = resp.payload[0];
                        if (resp.rcode != AMC_PROXY_RESULT_SUCCESS ||
                            (uint8_t)response_id != request_id) {
                            syslog(LOG_ERR, "Heartbeat validation failed!");
                            gcq_heartbeat_raise_event(
                                GCQ_HEARTBEAT_EVENT_VALIDATION, ctx);
                            fail_count++;
                        } else {
                            fail_count = 0;
                        }
                        ami_gcq_free_cid(cid);
                    }
                }

                request_id++;
            } else if (!fatal_event_raised) {
                syslog(LOG_ERR,
                       "Heartbeat fail count above threshold! "
                       "Raising fatal event...");
                gcq_heartbeat_raise_event(GCQ_HEARTBEAT_EVENT_FATAL, ctx);
                fatal_event_raised = true;
            }
        }

        gcq_interruptible_sleep(&ctx->hb_running, HEARTBEAT_REQUEST_INTERVAL_MS);
        if (!ctx->hb_running)
            break;
    }

    return NULL;
}

/**
 * gcq_log_thread() - Background thread: forward AMC firmware log messages.
 *
 * Legacy ami.ko kept last_printed_msg_index for the lifetime of the driver.
 * Short-lived ami_tool sessions persist that offset per BDF under /tmp so each
 * command only forwards records not already emitted.  AMI_FW_LOG_FLUSH_RING=1
 * clears the ring and saved offset (legacy first-probe flush).
 */
static void *gcq_log_thread(void *arg)
{
    gcq_metal_ctx *ctx = (gcq_metal_ctx *)arg;

    if (gcq_log_flush_ring_enabled()) {
        volatile struct amc_log_msg *lm;
        void *log_buffer_addr;
        uint32_t buf_len;

        lm = (volatile struct amc_log_msg *)
            ((uint8_t *)ctx->gcq_payload_base_virt_addr +
             offsetof(struct amc_shared_mem, log_msg));
        __sync_synchronize();
        buf_len = lm->log_msg_buf_len;
        if (lm->log_msg_buf_off != 0 && buf_len != 0) {
            log_buffer_addr = (uint8_t *)ctx->gcq_payload_base_virt_addr +
                              lm->log_msg_buf_off;
            memset(log_buffer_addr, 0, buf_len);
            lm->log_msg_index = 0;
            __sync_synchronize();
            ctx->last_log_index = 0;
            gcq_log_clear_saved_index(ctx);
        }
    }

    while (ctx->log_running) {
        if (ctx->gcq_payload_base_virt_addr &&
            ctx->state != GCQ_STATE_CLOSED) {
            dump_amc_log(ctx);
        }

        gcq_interruptible_sleep(&ctx->log_running, LOGGING_SLEEP_INTERVAL_MS);
        if (!ctx->log_running)
            break;
    }

    return NULL;
}

/**
 * Initialize GCQ driver (one-time global init)
 */
uint32_t gcq_userspace_init(void)
{
    pthread_mutex_lock(&g_init_lock);

    if (g_gcq_initialized) {
        pthread_mutex_unlock(&g_init_lock);
        return GCQ_ERRORS_DRIVER_IN_USE;
    }

    g_gcq_initialized = true;
    openlog(AMI_SYSLOG_IDENT, LOG_PID | LOG_NDELAY, LOG_USER);
    pthread_mutex_unlock(&g_init_lock);

    GCQ_DEBUG("GCQ userspace driver initialized\n");
    return GCQ_ERRORS_NONE;
}

/**
 * Tear down global GCQ driver state so the next open starts clean.
 */
uint32_t gcq_userspace_deinit(void)
{
    pthread_mutex_lock(&g_init_lock);
    g_gcq_initialized = false;
    closelog();
    pthread_mutex_unlock(&g_init_lock);

    GCQ_DEBUG("GCQ userspace driver deinitialized\n");
    return GCQ_ERRORS_NONE;
}

/**
 * Create GCQ context
 */
uint32_t gcq_userspace_create_context(gcq_metal_ctx **ctx_out, int uio_num, bool use_metal)
{
    gcq_metal_ctx *ctx;

    if (!ctx_out) {
        return GCQ_ERRORS_INVALID_ARG;
    }

    if (!g_gcq_initialized) {
        return GCQ_ERRORS_DRIVER_NOT_INITIALISED;
    }

    /* Allocate context */
    ctx = (gcq_metal_ctx *)calloc(1, sizeof(gcq_metal_ctx));
    if (!ctx) {
        return GCQ_ERRORS_INVALID_INSTANCE;
    }

    /* Initialize context */
    ctx->ulUpperFirewall  = GCQ_INSTANCE_UPPER_FIREWALL;
    ctx->ulLowerFirewall  = GCQ_INSTANCE_LOWER_FIREWALL;
    ctx->use_metal        = use_metal;
    ctx->bdf_str[0]       = '\0';
    ctx->uio_num          = uio_num;
    ctx->uio_fd           = -1;
    ctx->vfio_container_fd = -1;
    ctx->vfio_group_fd     = -1;
    ctx->vfio_device_fd    = -1;
    ctx->bar0_io.virt      = NULL;
    ctx->bar0_io.size      = 0;
    ctx->state             = GCQ_STATE_CLOSED;
    ctx->is_initialized    = false;
    ctx->consumer_attached = false;
    ctx->hb_running        = false;
    ctx->hb_paused         = false;
    ctx->gcq_halted        = false;
    ctx->log_running       = false;
    ctx->last_log_index    = 0;
    ctx->amc_wait_sec      = AMC_READY_MAX_WAIT_SEC;
    ctx->next_cid          = 1;    /* pending.count is 0 via calloc */

    /* Initialize mutex */
    if (pthread_mutex_init(&ctx->lock, NULL) != 0) {
        free(ctx);
        return GCQ_ERRORS_INVALID_INSTANCE;
    }
    if (pthread_mutex_init(&ctx->log_drain_lock, NULL) != 0) {
        pthread_mutex_destroy(&ctx->lock);
        free(ctx);
        return GCQ_ERRORS_INVALID_INSTANCE;
    }

    /* Set up ring pointers */
    ctx->producer = &ctx->sq;
    ctx->consumer = &ctx->cq;

    *ctx_out = ctx;

    GCQ_DEBUG("Created context (use_metal=%d, uio_num=%d)\n", use_metal, uio_num);
    return GCQ_ERRORS_NONE;
}

/**
 * Set the PCI BDF string for VFIO device open.
 */
/**
 * gcq_userspace_set_amc_timeout() - Override max seconds to wait for AMC ready.
 * Set to 0 to fail immediately when AMC is not ready (e.g. for device
 * enumeration / overview where a 60-second stall is unacceptable).
 */
uint32_t gcq_userspace_set_amc_timeout(gcq_metal_ctx *ctx, uint32_t seconds)
{
    if (!ctx)
        return GCQ_ERRORS_INVALID_HANDLE;
    ctx->amc_wait_sec = seconds;
    return GCQ_ERRORS_NONE;
}

/*
 * gcq_userspace_is_quick_open() - True for transient enumeration/overview opens
 * (amc_wait_sec == 0). Command paths use this to pick shorter timeouts so a
 * half-responsive card fails fast instead of stalling a status snapshot.
 */
bool gcq_userspace_is_quick_open(gcq_metal_ctx *ctx)
{
    return ctx && ctx->amc_wait_sec == 0;
}

uint32_t gcq_userspace_set_bdf(gcq_metal_ctx *ctx, const char *bdf)
{
    if (!ctx)
        return GCQ_ERRORS_INVALID_HANDLE;

    if (!bdf || bdf[0] == '\0') {
        ctx->bdf_str[0] = '\0';
        return GCQ_ERRORS_NONE;
    }

    strncpy(ctx->bdf_str, bdf, sizeof(ctx->bdf_str) - 1);
    ctx->bdf_str[sizeof(ctx->bdf_str) - 1] = '\0';

    GCQ_DEBUG("BDF set to: %s\n", ctx->bdf_str);
    return GCQ_ERRORS_NONE;
}

uint32_t gcq_userspace_vfio_pcie_reset(gcq_metal_ctx *ctx)
{
    if (!ctx)
        return GCQ_ERRORS_INVALID_HANDLE;

    if (!ctx->use_metal || ctx->vfio_device_fd < 0)
        return GCQ_ERRORS_NONE;

    if (ioctl(ctx->vfio_device_fd, VFIO_DEVICE_RESET) < 0) {
        fprintf(stderr,
            "[DEVICE_BOOT] VFIO_DEVICE_RESET failed: %s (continuing)\n",
            strerror(errno));
        return GCQ_ERRORS_NONE;
    }

    fprintf(stderr, "[DEVICE_BOOT] VFIO_DEVICE_RESET issued\n");
    return GCQ_ERRORS_NONE;
}

/**
 * gcq_close_bar0() - Backend-agnostic BAR0 cleanup helper.
 * Called from error paths inside gcq_userspace_open() (lock already held).
 */
static void gcq_close_bar0(gcq_metal_ctx *ctx)
{
    if (ctx->use_metal) {
        metal_vfio_device_close(&ctx->bar0_io,
                                ctx->vfio_device_fd,
                                ctx->vfio_group_fd,
                                ctx->vfio_container_fd);
        ctx->vfio_device_fd    = -1;
        ctx->vfio_group_fd     = -1;
        ctx->vfio_container_fd = -1;
    } else {
        if (ctx->bar_base && ctx->bar_base != MAP_FAILED)
            munmap(ctx->bar_base, ctx->bar_size);
        if (ctx->uio_fd >= 0) {
            close(ctx->uio_fd);
            ctx->uio_fd = -1;
        }
    }
    ctx->bar_base    = NULL;
    ctx->bar0_io.virt = NULL;
    ctx->bar0_io.size = 0;
}

/**
 * Open GCQ instance - EXACT old driver flow
 *
 * This follows the exact sequence from ami_amc_control.c and ami_gcq.c:
 * 1. Map BAR0
 * 2. gcq_payload_base = BAR0 + AMC_SHARED_MEM_OFFSET
 * 3. Read amc_shared_mem structure
 * 4. Validate AMC magic and status
 * 5. gcq_ring_buf_base = gcq_payload_base + ring_buf_off
 * 6. Configure GCQ rings
 * 7. Attach to consumer
 */
uint32_t gcq_userspace_open(gcq_metal_ctx *ctx, const GCQCfg *cfg)
{
    uint32_t amc_status;
    uint32_t waited;
    bool magic_ok;
    bool read_ok;
    uint32_t num_slots;
    uint64_t ullSQProduced, ullCQProduced;
    uint32_t amc_boot_waited = 0;
    GCQHeader header_preview;
    volatile uint8_t *preview_src;
    uint8_t *preview_dest;
    size_t i;
    int log_fd;
    uint32_t drained_count;
    uint8_t dummy_resp[512];

    if (!ctx) {
        return GCQ_ERRORS_INVALID_HANDLE;
    }

    /* cfg parameter is IGNORED - we discover everything from hardware */
    (void)cfg;

    pthread_mutex_lock(&ctx->lock);

    if (ctx->state != GCQ_STATE_CLOSED) {
        pthread_mutex_unlock(&ctx->lock);
        return GCQ_ERRORS_DRIVER_IN_USE;
    }

    GCQ_DEBUG("=== Opening GCQ ===\n");

    ctx->gcq_halted = false;

    /* ------------------------------------------------------------------
     * Steps 1-3: Open device and map BAR0.
     *
     * Two backends:
     *   use_metal == false  →  Linux UIO: /dev/uioX  + mmap
     *   use_metal == true   →  VFIO via metal_io_region abstraction
     *
     * Both paths populate ctx->bar0_io.  bar_base / bar_size are then
     * aliased from bar0_io so the rest of this function is unchanged.
     * ------------------------------------------------------------------ */
    if (ctx->use_metal) {
        /* VFIO / libmetal path */
        if (ctx->bdf_str[0] == '\0') {
            fprintf(stderr,
                    "[GCQ] use_metal=true but BDF not set — "
                    "call gcq_userspace_set_bdf() before gcq_userspace_open()\n");
            pthread_mutex_unlock(&ctx->lock);
            return GCQ_ERRORS_BDF_NOT_SET;
        }

        if (metal_vfio_device_open(ctx->bdf_str,
                                   &ctx->bar0_io,
                                   &ctx->vfio_device_fd,
                                   &ctx->vfio_group_fd,
                                   &ctx->vfio_container_fd) != 0) {
            gcq_log_open_failed(GCQ_ERRORS_VFIO_OPEN_FAILED,
                                "VFIO BAR0 unavailable (PCI COMMAND 0/0xffff or "
                                "endpoint not ready after POR)");
            pthread_mutex_unlock(&ctx->lock);
            return GCQ_ERRORS_VFIO_OPEN_FAILED;
        }
    } else {
        /* UIO / direct-mmap path */
        char uio_path[64];
        char sysfs_path[256];
        FILE *fp;
        uint64_t bar_size;
        void *mapped;

        snprintf(uio_path, sizeof(uio_path), "/dev/uio%d", ctx->uio_num);
        ctx->uio_fd = open(uio_path, O_RDWR | O_SYNC);
        if (ctx->uio_fd < 0) {
            GCQ_DEBUG("ERROR: Cannot open %s: %s\n", uio_path, strerror(errno));
            pthread_mutex_unlock(&ctx->lock);
            return GCQ_ERRORS_UIO_OPEN_FAILED;
        }
        GCQ_DEBUG("Opened %s (fd=%d)\n", uio_path, ctx->uio_fd);

        snprintf(sysfs_path, sizeof(sysfs_path),
                 "/sys/class/uio/uio%d/maps/map0/size", ctx->uio_num);
        fp = fopen(sysfs_path, "r");
        if (!fp || fscanf(fp, "0x%lx", &bar_size) != 1) {
            if (fp) fclose(fp);
            close(ctx->uio_fd);
            ctx->uio_fd = -1;
            pthread_mutex_unlock(&ctx->lock);
            return GCQ_ERRORS_MMAP_FAILED;
        }
        fclose(fp);

        mapped = mmap(NULL, bar_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, ctx->uio_fd, 0);
        if (mapped == MAP_FAILED) {
            GCQ_DEBUG("ERROR: mmap failed: %s\n", strerror(errno));
            close(ctx->uio_fd);
            ctx->uio_fd = -1;
            pthread_mutex_unlock(&ctx->lock);
            return GCQ_ERRORS_MMAP_FAILED;
        }

        ctx->bar0_io.virt = mapped;
        ctx->bar0_io.size = bar_size;
    }

    /* Alias bar_base / bar_size from bar0_io — used throughout the function */
    ctx->bar_base = ctx->bar0_io.virt;
    ctx->bar_size = ctx->bar0_io.size;
    GCQ_DEBUG("BAR0 mapped at %p (size=0x%zx)\n", ctx->bar_base, ctx->bar_size);

    /* Step 4: Calculate addresses (from ami_amc_control.c:915, 942) */
    ctx->gcq_base_virt_addr = ctx->bar_base;
    ctx->gcq_payload_base_virt_addr = (char *)ctx->gcq_base_virt_addr + XILINX_SGCQ_SIZE_BYTES;

    GCQ_DEBUG("gcq_base_virt_addr        = %p\n", ctx->gcq_base_virt_addr);
    GCQ_DEBUG("gcq_payload_base_virt_addr = %p\n", ctx->gcq_payload_base_virt_addr);

    /* Step 5–6: Read AMC shared memory; wait for VMR0 after FW boot / POR */
    waited = 0;
    magic_ok = false;
    read_ok = gcq_read_amc_shared_mem(ctx);

    while (waited <= ctx->amc_wait_sec) {
            if (read_ok &&
                gcq_amc_magic_valid(ctx->amc_shared_mem.amc_magic_no)) {
                magic_ok = true;
                break;
            }

            if (waited == 0 && ctx->amc_wait_sec > 0) {
                fprintf(stderr,
                        "[GCQ] AMC not ready at BAR0+AMC_SHARED_MEM_OFFSET (magic 0x%08x); "
                        "waiting up to %us for VMR0...\n",
                        ctx->amc_shared_mem.amc_magic_no,
                        ctx->amc_wait_sec);
            }

            if (waited >= ctx->amc_wait_sec)
                break;

            pthread_mutex_unlock(&ctx->lock);
            sleep(1);
            pthread_mutex_lock(&ctx->lock);

            if (ctx->state != GCQ_STATE_CLOSED) {
                pthread_mutex_unlock(&ctx->lock);
                return GCQ_ERRORS_DRIVER_IN_USE;
            }

            read_ok = gcq_read_amc_shared_mem(ctx);
            waited++;
        }

    GCQ_DEBUG("AMC Shared Memory:\n");
    GCQ_DEBUG("  Magic:           0x%08x\n", ctx->amc_shared_mem.amc_magic_no);
    GCQ_DEBUG("  Ring buf offset: 0x%x\n", ctx->amc_shared_mem.ring_buf.ring_buf_off);
    GCQ_DEBUG("  Ring buf length: 0x%x\n", ctx->amc_shared_mem.ring_buf.ring_buf_len);

    if (!magic_ok) {
        /* Suppress verbose errors for quick_open (overview/enumeration) */
        if (!gcq_userspace_is_quick_open(ctx)) {
            fprintf(stderr,
                    "[GCQ] AMC magic still invalid at BAR0+AMC_SHARED_MEM_OFFSET: 0x%08x "
                    "(expected 0x%08x)\n",
                    ctx->amc_shared_mem.amc_magic_no,
                    AMC_MAGIC_NUMBER);
            if (ctx->amc_shared_mem.amc_magic_no == 0xffffffffU) {
                fprintf(stderr,
                        "[GCQ] 0xffffffff usually means AMC firmware is down or BAR is not "
                        "mapped — cold/FW reboot the card, then PCI remove/rescan and "
                        "rebind to vfio-pci; wait ~60s before cfgmem_program.\n");
            }
        }
        gcq_log_open_failed(GCQ_ERRORS_CONSUMER_NOT_AVAILABLE,
                            "AMC magic invalid at BAR0+AMC_SHARED_MEM_OFFSET");
        gcq_close_bar0(ctx);
        pthread_mutex_unlock(&ctx->lock);
        return GCQ_ERRORS_CONSUMER_NOT_AVAILABLE;
    }

    if (waited > 0) {
        amc_boot_waited = waited;
        fprintf(stderr,
                "[GCQ] AMC ready after %us (magic 0x%08x)\n",
                waited, ctx->amc_shared_mem.amc_magic_no);
    }
    GCQ_DEBUG("✓ AMC Magic Number found (0x%08x)\n", ctx->amc_shared_mem.amc_magic_no);

    /* Step 7: Read AMC device status (from ami_amc_control.c:326-327) */
    amc_status = gcq_read32((char *)ctx->gcq_payload_base_virt_addr +
                            ctx->amc_shared_mem.status.amc_status_off);
    GCQ_DEBUG("AMC device status: 0x%x\n", amc_status);

    if (!amc_status) {
        fprintf(stderr,
                "[GCQ] AMC not ready: status=0 at BAR0+AMC_SHARED_MEM_OFFSET+0x%x "
                "(AMC may be in ERROR after failed program — power-cycle card)\n",
                ctx->amc_shared_mem.status.amc_status_off);
        GCQ_DEBUG("ERROR: AMC device not ready!\n");
        gcq_log_open_failed(GCQ_ERRORS_CONSUMER_NOT_AVAILABLE,
                            "AMC status=0 (proxy may be in ERROR)");
        gcq_close_bar0(ctx);
        pthread_mutex_unlock(&ctx->lock);
        return GCQ_ERRORS_CONSUMER_NOT_AVAILABLE;
    }
    GCQ_DEBUG("✓ AMC device ready\n");

    /* Step 8: Calculate GCQ ring buffer address (from ami_amc_control.c:372-374)
     * WORKAROUND: Firmware has bug writing shared memory structure, hardcode known offset
     * The GCQ header magic at BAR0+AMC_GCQ_RING_OFFSET is correct, so we know GCQ is there
     */
    ctx->gcq_ring_buf_base_virt_addr =
        (char *)ctx->gcq_payload_base_virt_addr + AMC_SHARED_MEM_OFFSET;  /* GCQ at BAR0+AMC_GCQ_RING_OFFSET */

    GCQ_DEBUG("gcq_ring_buf_base_virt_addr = %p (using hardcoded offset AMC_SHARED_MEM_OFFSET)\n",
              ctx->gcq_ring_buf_base_virt_addr);

    /* Step 9: Read number of slots from GCQ header (don't calculate!)
     * Use volatile reads like kernel driver's memcpy_fromio()
     */
    __sync_synchronize();
    preview_src = (volatile uint8_t *)ctx->gcq_ring_buf_base_virt_addr;
    preview_dest = (uint8_t *)&header_preview;
    for (i = 0; i < sizeof(header_preview); i++) {
        preview_dest[i] = preview_src[i];
    }
    __sync_synchronize();

    num_slots = header_preview.ulHdrNumSlots;
    GCQ_DEBUG("GCQ Header num_slots = %u\n", num_slots);

    if (num_slots < GCQ_MIN_NUM_SLOTS) {
        GCQ_DEBUG("ERROR: Too few slots: %u\n", num_slots);
        gcq_close_bar0(ctx);
        pthread_mutex_unlock(&ctx->lock);
        return GCQ_ERRORS_INVALID_NUM_SLOTS;
    }

    /* Step 10: Initialize SQ and CQ rings (from ami_gcq.c:454-473)
     *
     * CRITICAL: We are the HOST (producer), firmware is consumer
     * We write to SQ, read from CQ
     *
     * For pointer locations, we use HEADER fields (not registers)
     * because the old driver uses registers for its view as consumer,
     * but we are producer so we use the complementary addresses.
     */
    GCQ_DEBUG("Initializing rings...\n");

    /* CRITICAL: Pointer locations - matching old driver exactly
     * SQ: We are producer, firmware is consumer
     *     - We write produced pointer to REGISTER 0x0 (GCQ_PRODUCER_SQ_TAIL_POINTER)
     *     - Firmware writes consumed pointer to Header[6]
     * CQ: Firmware is producer, we are consumer
     *     - Firmware writes produced pointer to REGISTER 0x100 (CQ_Tail)
     *     - We write consumed pointer to Header[7]
     */
    ullSQProduced = (uint64_t)ctx->gcq_base_virt_addr + GCQ_PRODUCER_SQ_TAIL_POINTER;  /* Register 0x0 */
    ullCQProduced = (uint64_t)ctx->gcq_base_virt_addr + GCQ_CONSUMER_CQ_TAIL_POINTER;  /* Register 0x100 */

    GCQ_DEBUG("SQ Ring:\n");

    /* Log addresses to file for analysis */
    log_fd = open("/tmp/gcq_addresses.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (log_fd >= 0) {
        char logbuf[512];
        int len = snprintf(logbuf, sizeof(logbuf),
            "[INIT] BAR0=%p gcq_ring_buf_base=%p\n"
            "[INIT] SQ produced_addr=0x%lx\n"
            "[INIT] SQ slot_array=0x%lx\n"
            "[INIT] Expected slot at BAR0+0x202C, calculated offset=0x%lx\n",
            ctx->bar_base, ctx->gcq_ring_buf_base_virt_addr,
            ullSQProduced,
            (uint64_t)ctx->gcq_ring_buf_base_virt_addr + sizeof(GCQHeader),
            ((uint64_t)ctx->gcq_ring_buf_base_virt_addr + sizeof(GCQHeader)) - (uint64_t)ctx->bar_base);
        write(log_fd, logbuf, len);
        close(log_fd);
    }
    gcq_init_ring(&ctx->sq,
                  ullSQProduced,  /* Header[9] - where WE write our produced pointer */
                  (uint64_t)ctx->gcq_ring_buf_base_virt_addr + offsetof(GCQHeader, ulHdrSQConsumed),  /* Header[6] - where firmware writes */
                  (uint64_t)ctx->gcq_ring_buf_base_virt_addr + sizeof(GCQHeader),
                  num_slots,
                  AMC_PROXY_REQUEST_SIZE);

    GCQ_DEBUG("CQ Ring:\n");
    gcq_init_ring(&ctx->cq,
                  ullCQProduced,  /* Header[10] - where firmware writes its produced pointer */
                  (uint64_t)ctx->gcq_ring_buf_base_virt_addr + offsetof(GCQHeader, ulHdrCQConsumed),  /* Header[7] - where WE write */
                  (uint64_t)ctx->gcq_ring_buf_base_virt_addr + sizeof(GCQHeader) +
                      (num_slots * AMC_PROXY_REQUEST_SIZE),
                  num_slots,
                  AMC_PROXY_RESPONSE_SIZE);

    ctx->is_initialized = true;
    ctx->state = GCQ_STATE_OPENED;

    /* Step 11: Attach to consumer with retry (from ami_gcq.c:786-797) */
    uint32_t attempts = 0;
    uint32_t attach_result;

    GCQ_DEBUG("Attaching to consumer (with retry)...\n");
    while (attempts < 30) {
        attach_result = gcq_attach_consumer(ctx);
        if (attach_result == GCQ_ERRORS_NONE) {
            GCQ_DEBUG("✓ Attached on attempt %u\n", attempts + 1);
            break;
        }
        attempts++;
        if (attempts < 30) {
            usleep(50000);  /* 50ms between retries instead of 1s */
        }
    }

    if (attach_result != GCQ_ERRORS_NONE) {
        uint32_t gcq_magic = gcq_read32(ctx->gcq_ring_buf_base_virt_addr);

        fprintf(stderr,
                "[GCQ] Failed to attach to AMC consumer after %u s "
                "(attach error %u, GCQ magic=0x%08x)\n",
                attempts, attach_result, gcq_magic);
        GCQ_DEBUG("ERROR: Failed to attach after %u attempts\n", attempts);
        gcq_close_bar0(ctx);
        ctx->state = GCQ_STATE_CLOSED;
        ctx->is_initialized = false;
        pthread_mutex_unlock(&ctx->lock);
        return attach_result;
    }

    /* Step 12: Drain any old responses from the queue before returning
     * This ensures fresh start for new commands
     */
    GCQ_DEBUG("Draining old responses from CQ...\n");
    drained_count = 0;

    while (gcq_can_consume(&ctx->cq, ctx->bar_base)) {
        uint64_t slot_addr;
        uint32_t slot_index;

        /* Read and discard old response */
        slot_index = ctx->cq.ulRingConsumed & (ctx->cq.ulRingNumSlots - 1);
        slot_addr = ctx->cq.ullRingSlotAddr + (uint64_t)ctx->cq.ulRingSlotSize * slot_index;
        memcpy(dummy_resp, (void *)slot_addr, AMC_PROXY_RESPONSE_SIZE);
        __sync_synchronize();

        ctx->cq.ulRingConsumed++;
        gcq_write32((void *)ctx->cq.ullRingConsumedAddr, ctx->cq.ulRingConsumed);

        drained_count++;
        if (drained_count > 100) {
            GCQ_DEBUG("WARNING: Drained %u responses, stopping to avoid infinite loop\n", drained_count);
            break;
        }
    }

    if (drained_count > 0) {
        GCQ_DEBUG("Drained %u old responses from CQ\n", drained_count);
    }

    pthread_mutex_unlock(&ctx->lock);

    /* Resume log drain offset (legacy last_printed_msg_index). */
    if (gcq_log_flush_ring_enabled()) {
        ctx->last_log_index = 0;
        gcq_log_clear_saved_index(ctx);
    } else if (amc_boot_waited > 0) {
        gcq_log_clear_saved_index(ctx);
        ctx->last_log_index = 0;
    } else {
        gcq_log_load_index(ctx);
    }

    /* Logging thread only — legacy starts heartbeat after IDENTIFY/version. */
    /*
     * Skip the AMC log-forwarding thread on transient "quick" opens
     * (amc_wait_sec == 0, used by device enumeration / overview). Those are
     * read-only snapshots that never consume firmware logs, so the thread is
     * pure overhead — and its 500ms sleep would otherwise add up to ~0.5s of
     * teardown latency when the handle is closed moments later.
     */
    if (ctx->amc_wait_sec != 0) {
        ctx->log_running = true;
        pthread_create(&ctx->log_thread, NULL, gcq_log_thread, ctx);
    }

    GCQ_DEBUG("=== GCQ Opened Successfully ===\n");
    return GCQ_ERRORS_NONE;
}

/**
 * gcq_userspace_start_heartbeat() - Start heartbeat thread (legacy setup_amc).
 *
 * Legacy ami.ko spawned heartbeat_health_thread only after get_gcq_version()
 * and check_amc_supported_version() succeeded (non-compat mode).
 */
uint32_t gcq_userspace_start_heartbeat(gcq_metal_ctx *ctx)
{
    if (!ctx)
        return GCQ_ERRORS_INVALID_ARG;

    if (ctx->state != GCQ_STATE_ATTACHED)
        return GCQ_ERRORS_CONSUMER_NOT_ATTACHED;

    if (ctx->hb_running)
        return GCQ_ERRORS_NONE;

    ctx->gcq_halted = false;
    ctx->hb_running = true;
    if (pthread_create(&ctx->hb_thread, NULL, gcq_heartbeat_thread, ctx) != 0) {
        ctx->hb_running = false;
        return GCQ_ERRORS_INVALID_INSTANCE;
    }

    return GCQ_ERRORS_NONE;
}

/**
 * Close GCQ instance
 */
uint32_t gcq_userspace_close(gcq_metal_ctx *ctx)
{
    if (!ctx) {
        return GCQ_ERRORS_INVALID_HANDLE;
    }

    /*
     * Stop background threads BEFORE acquiring the lock.
     * The heartbeat thread calls gcq_userspace_write() which takes ctx->lock;
     * joining while holding the lock would deadlock.
     * The log thread accesses bar0 directly — join it before we unmap BAR0.
     */
    if (ctx->hb_running) {
        ctx->hb_running = false;
        pthread_join(ctx->hb_thread, NULL);
    }
    if (ctx->log_running) {
        ctx->log_running = false;
        pthread_join(ctx->log_thread, NULL);
        dump_amc_log(ctx);
        gcq_log_save_index(ctx);
    }

    pthread_mutex_lock(&ctx->lock);

    if (ctx->state == GCQ_STATE_CLOSED) {
        pthread_mutex_unlock(&ctx->lock);
        return GCQ_ERRORS_NONE;
    }

    /* Release BAR0 via the appropriate backend */
    if (ctx->use_metal) {
        metal_vfio_device_close(&ctx->bar0_io,
                                ctx->vfio_device_fd,
                                ctx->vfio_group_fd,
                                ctx->vfio_container_fd);
        ctx->vfio_device_fd    = -1;
        ctx->vfio_group_fd     = -1;
        ctx->vfio_container_fd = -1;
    } else {
        if (ctx->bar_base && ctx->bar_base != MAP_FAILED)
            munmap(ctx->bar_base, ctx->bar_size);
        if (ctx->uio_fd >= 0) {
            close(ctx->uio_fd);
            ctx->uio_fd = -1;
        }
    }

    ctx->bar_base    = NULL;
    ctx->bar0_io.virt = NULL;
    ctx->bar0_io.size = 0;

    ctx->state             = GCQ_STATE_CLOSED;
    ctx->is_initialized    = false;
    ctx->consumer_attached = false;

    pthread_mutex_unlock(&ctx->lock);

    GCQ_DEBUG("GCQ closed\n");
    return GCQ_ERRORS_NONE;
}

/**
 * Destroy context
 */
uint32_t gcq_userspace_destroy_context(gcq_metal_ctx *ctx)
{
    if (!ctx) {
        return GCQ_ERRORS_INVALID_HANDLE;
    }

    /* Close if still open */
    if (ctx->state != GCQ_STATE_CLOSED) {
        gcq_userspace_close(ctx);
    }

    /* Destroy mutex */
    pthread_mutex_destroy(&ctx->lock);
    pthread_mutex_destroy(&ctx->log_drain_lock);

    /* Free context */
    free(ctx);

    GCQ_DEBUG("Context destroyed\n");
    return GCQ_ERRORS_NONE;
}

/**
 * Write data to GCQ (producer operation) - from ami_gcq.c:676-740
 */
uint32_t gcq_userspace_write(gcq_metal_ctx *ctx,
                              const uint8_t *data,
                              uint32_t size,
                              uint32_t timeout_ms)
{
    GCQRing *ring;
    uint64_t slot_addr;
    uint32_t slot_index;
    struct timespec start, now;
    uint32_t elapsed_ms;

    if (!ctx || !data) {
        return GCQ_ERRORS_INVALID_ARG;
    }

    if (!ctx->is_initialized || ctx->state != GCQ_STATE_ATTACHED) {
        return GCQ_ERRORS_CONSUMER_NOT_ATTACHED;
    }

    if (ctx->gcq_halted) {
        return GCQ_ERRORS_CONSUMER_NOT_AVAILABLE;
    }

    if (size > AMC_PROXY_REQUEST_SIZE) {
        return GCQ_ERRORS_INVALID_ARG;
    }

    if (!CHECK_32BIT_ALIGNMENT(size)) {
        return GCQ_ERRORS_INVALID_ARG;
    }

    ring = ctx->producer;  /* SQ */

    /* Check if we can produce (with timeout) - poll WITHOUT holding mutex */
    clock_gettime(CLOCK_MONOTONIC, &start);
    bool warned = false;

    while (!gcq_can_produce(ring, ctx->bar_base)) {
        if (timeout_ms == 0) {
            return GCQ_ERRORS_PRODUCER_NO_FREE_SLOTS;
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 +
                     (now.tv_nsec - start.tv_nsec) / 1000000;

        /* Warn if we're waiting too long - indicates performance issue */
        if (!warned && elapsed_ms > 100) {
            uint32_t hw_consumed = gcq_read32((void *)ring->ullRingConsumedAddr);
            fprintf(stderr, "[GCQ] WARNING: Waiting for free SQ slot (produced=%u, hw_consumed=%u, elapsed=%lums)\n",
                    ring->ulRingProduced, hw_consumed, (unsigned long)elapsed_ms);
            warned = true;
        }

        if (elapsed_ms >= timeout_ms) {
            return GCQ_ERRORS_TIMEOUT;
        }

        usleep(10);  /* 10us instead of 1ms for faster submission */
    }

    /* Now lock mutex only for the actual write operation */
    pthread_mutex_lock(&ctx->lock);

    /* Double-check we can still produce (race condition protection) */
    if (!gcq_can_produce(ring, ctx->bar_base)) {
        pthread_mutex_unlock(&ctx->lock);
        return GCQ_ERRORS_PRODUCER_NO_FREE_SLOTS;
    }

    /* Get slot address */
    slot_index = ring->ulRingProduced & (ring->ulRingNumSlots - 1);
    slot_addr = ring->ullRingSlotAddr + (uint64_t)ring->ulRingSlotSize * slot_index;

    /* SAFETY CHECK: Verify we're not writing to shared memory region (BAR0+AMC_SHARED_MEM_OFFSET to BAR0+AMC_GCQ_RING_OFFSET) */
    uint64_t bar0_addr = (uint64_t)ctx->bar_base;
    uint64_t offset_from_bar0 = slot_addr - bar0_addr;
    if (offset_from_bar0 >= AMC_SHARED_MEM_OFFSET && offset_from_bar0 < AMC_GCQ_RING_OFFSET) {
        fprintf(stderr, "[GCQ_WRITE] CRITICAL ERROR: About to write to shared memory region!\n");
        fprintf(stderr, "[GCQ_WRITE] BAR0=%p slot_addr=0x%lx offset=0x%lx\n",
                ctx->bar_base, slot_addr, offset_from_bar0);
        pthread_mutex_unlock(&ctx->lock);
        return GCQ_ERRORS_INVALID_ARG;
    }

    GCQ_DEBUG("[GCQ_WRITE] Writing to slot_addr=0x%lx (BAR0+0x%lx)\n", slot_addr, offset_from_bar0);

    /* Write data to slot */
    memcpy((void *)slot_addr, data, size);
    __sync_synchronize();

    /* Update local produced counter */
    ring->ulRingProduced++;

    /* Notify firmware: write produced pointer */
    gcq_write32((void *)ring->ullRingProducedAddr, ring->ulRingProduced);

    pthread_mutex_unlock(&ctx->lock);

    GCQ_DEBUG("Wrote %u bytes to SQ slot %u\n", size, slot_index);
    return GCQ_ERRORS_NONE;
}

/**
 * Read data from GCQ (consumer operation) - from ami_gcq.c:615-674
 */
uint32_t gcq_userspace_read(gcq_metal_ctx *ctx,
                             uint8_t *data,
                             uint32_t *size,
                             uint32_t timeout_ms)
{
    GCQRing *ring;
    uint64_t slot_addr;
    uint32_t slot_index;
    struct timespec start, now;
    uint32_t elapsed_ms;

    if (!ctx || !data || !size) {
        return GCQ_ERRORS_INVALID_ARG;
    }

    if (!ctx->is_initialized || ctx->state != GCQ_STATE_ATTACHED) {
        return GCQ_ERRORS_CONSUMER_NOT_ATTACHED;
    }

    if (*size > AMC_PROXY_RESPONSE_SIZE) {
        return GCQ_ERRORS_INVALID_ARG;
    }

    pthread_mutex_lock(&ctx->lock);

    ring = ctx->consumer;  /* CQ */

    /* Check if data available (with timeout) */
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (!gcq_can_consume(ring, ctx->bar_base)) {
        if (timeout_ms == 0) {
            pthread_mutex_unlock(&ctx->lock);
            return GCQ_ERRORS_CONSUMER_NO_DATA_RECEIVED;
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 +
                     (now.tv_nsec - start.tv_nsec) / 1000000;

        if (elapsed_ms >= timeout_ms) {
            pthread_mutex_unlock(&ctx->lock);
            return GCQ_ERRORS_TIMEOUT;
        }

        usleep(10);  /* 10us instead of 1ms for faster response checking */
    }

    /* Get slot address */
    slot_index = ring->ulRingConsumed & (ring->ulRingNumSlots - 1);
    slot_addr = ring->ullRingSlotAddr + (uint64_t)ring->ulRingSlotSize * slot_index;

    /* Read data from slot */
    memcpy(data, (void *)slot_addr, *size);
    __sync_synchronize();

    /* Update local consumed counter */
    ring->ulRingConsumed++;

    /* Notify firmware: write consumed pointer */
    gcq_write32((void *)ring->ullRingConsumedAddr, ring->ulRingConsumed);

    pthread_mutex_unlock(&ctx->lock);

    GCQ_DEBUG("Read %u bytes from CQ slot %u\n", *size, slot_index);
    return GCQ_ERRORS_NONE;
}

/**
 * Pause or resume the background heartbeat thread.
 *
 * Used during multi-chunk PDI download/program (cfgmem_program, pdi_program).
 * Legacy ami.ko did not need this — gcq_cmd_lock serialized all GCQ traffic.
 * Userspace must pause explicitly so heartbeat completions do not fill the
 * 4-slot CQ while the host is not draining between chunks.
 */
uint32_t gcq_userspace_pause_heartbeat(gcq_metal_ctx *ctx, bool pause)
{
    if (!ctx)
        return GCQ_ERRORS_INVALID_ARG;

    ctx->hb_paused = pause;
    return GCQ_ERRORS_NONE;
}

/**
 * Discard all completions currently in the hardware CQ (non-blocking).
 */
/**
 * Read and discard completions visible in the CQ (non-blocking, no index snap).
 *
 * Unlike cq_purge(), this only consumes entries where produced != consumed.
 * Snapping consumed to the producer register can drop a valid completion
 * that was already in the ring (host then hangs; FW CQ fills -> proxy ERROR).
 */
uint32_t gcq_userspace_cq_drain(gcq_metal_ctx *ctx, uint32_t *drained_count)
{
    GCQRing *ring;
    uint32_t drained = 0;
    uint8_t dummy[AMC_PROXY_RESPONSE_SIZE];

    if (!ctx) {
        return GCQ_ERRORS_INVALID_ARG;
    }

    if (!ctx->is_initialized || ctx->state != GCQ_STATE_ATTACHED) {
        return GCQ_ERRORS_CONSUMER_NOT_ATTACHED;
    }

    pthread_mutex_lock(&ctx->lock);

    ring = ctx->consumer;

    while (drained < ring->ulRingNumSlots &&
           gcq_cq_pending(ring) > 0) {
        uint32_t slot_index = ring->ulRingConsumed & (ring->ulRingNumSlots - 1);
        uint64_t slot_addr = ring->ullRingSlotAddr +
                             (uint64_t)ring->ulRingSlotSize * slot_index;

        memcpy(dummy, (void *)slot_addr, sizeof(dummy));
        __sync_synchronize();
        ring->ulRingConsumed++;
        drained++;
    }

    if (drained > 0)
        gcq_write32((void *)ring->ullRingConsumedAddr, ring->ulRingConsumed);

    pthread_mutex_unlock(&ctx->lock);

    if (drained_count)
        *drained_count = drained;

    return GCQ_ERRORS_NONE;
}

uint32_t gcq_userspace_cq_purge(gcq_metal_ctx *ctx, uint32_t *purged_count)
{
    GCQRing *ring;
    uint32_t before;

    if (!ctx) {
        return GCQ_ERRORS_INVALID_ARG;
    }

    if (!ctx->is_initialized || ctx->state != GCQ_STATE_ATTACHED) {
        return GCQ_ERRORS_CONSUMER_NOT_ATTACHED;
    }

    pthread_mutex_lock(&ctx->lock);

    ring = ctx->consumer;
    before = ring->ulRingConsumed;
    gcq_cq_sync_consumer(ctx, ring);

    pthread_mutex_unlock(&ctx->lock);

    if (purged_count)
        *purged_count = ring->ulRingConsumed - before;

    return GCQ_ERRORS_NONE;
}

uint32_t gcq_userspace_wait_idle(gcq_metal_ctx *ctx, uint32_t timeout_ms)
{
    struct timespec start, now;
    GCQRing *sq;
    GCQRing *cq;

    if (!ctx) {
        return GCQ_ERRORS_INVALID_ARG;
    }

    if (!ctx->is_initialized || ctx->state != GCQ_STATE_ATTACHED) {
        return GCQ_ERRORS_CONSUMER_NOT_ATTACHED;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        uint32_t cq_pending;
        uint32_t sq_consumed;
        bool idle;

        pthread_mutex_lock(&ctx->lock);
        sq = ctx->producer;
        cq = ctx->consumer;
        cq_pending = gcq_cq_pending(cq);
        sq_consumed = gcq_read32((void *)sq->ullRingConsumedAddr);
        idle = (cq_pending == 0) && (sq_consumed == sq->ulRingProduced);
        pthread_mutex_unlock(&ctx->lock);

        if (idle)
            return GCQ_ERRORS_NONE;

        clock_gettime(CLOCK_MONOTONIC, &now);
        uint64_t elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 +
                              (now.tv_nsec - start.tv_nsec) / 1000000;

        if (elapsed_ms >= timeout_ms)
            return GCQ_ERRORS_TIMEOUT;

        usleep(2000);
    }
}

/**
 * Get version
 */
uint32_t gcq_userspace_get_version(GCQVersion *version)
{
    if (!version) {
        return GCQ_ERRORS_INVALID_ARG;
    }

    version->major = GCQ_VER_MAJOR;
    version->minor = GCQ_VER_MINOR;
    version->patch = GCQ_VER_PATCH;

    return GCQ_ERRORS_NONE;
}

/**
 * gcq_userspace_get_bar0() - Get BAR0 base address from context
 *
 * Returns the BAR0 mmap base pointer for direct memory access
 */
void *gcq_userspace_get_bar0(gcq_metal_ctx *ctx)
{
    struct gcq_metal_ctx_internal *pxGCQInstance = (struct gcq_metal_ctx_internal *)ctx;

    if (!pxGCQInstance || pxGCQInstance->ulUpperFirewall != GCQ_INSTANCE_UPPER_FIREWALL) {
        return NULL;
    }

    return pxGCQInstance->bar_base;
}

/**
 * gcq_userspace_get_amc_version_from_log() - Parse AMC firmware version from log ring.
 *
 * Scans the AMC firmware log ring buffer for the first "X.Y.Z" decimal version
 * string (X, Y, Z each < 100).  The firmware announces its version as one of
 * its first boot messages (e.g. "AMC: 3.0.0"), so this reliably returns the
 * real firmware version when the IDENTIFY GCQ command is unavailable.
 *
 * @ctx:   GCQ context (BAR0 must be mapped; does not require GCQ_STATE_ATTACHED)
 * @major: Output: major version number
 * @minor: Output: minor version number
 * @patch: Output: patch version number
 *
 * Return: GCQ_ERRORS_NONE on success,
 *         GCQ_ERRORS_CONSUMER_NOT_AVAILABLE if log not yet populated or no
 *         version string found,
 *         GCQ_ERRORS_INVALID_ARG / GCQ_ERRORS_INVALID_HANDLE on bad inputs.
 */
uint32_t gcq_userspace_get_amc_version_from_log(gcq_metal_ctx *ctx,
                                                  uint8_t *major,
                                                  uint8_t *minor,
                                                  uint8_t *patch)
{
    struct gcq_metal_ctx_internal *pxCtx = (struct gcq_metal_ctx_internal *)ctx;
    volatile struct amc_log_msg *lm;
    uint32_t log_idx, buf_off;
    uint32_t i;

    if (!pxCtx || !major || !minor || !patch)
        return GCQ_ERRORS_INVALID_ARG;

    if (!pxCtx->gcq_payload_base_virt_addr)
        return GCQ_ERRORS_INVALID_HANDLE;

    lm = (volatile struct amc_log_msg *)
         ((uint8_t *)pxCtx->gcq_payload_base_virt_addr +
          offsetof(struct amc_shared_mem, log_msg));

    __sync_synchronize();
    log_idx = lm->log_msg_index;
    buf_off = lm->log_msg_buf_off;
    __sync_synchronize();

    /* Log not yet populated or corrupted */
    if (log_idx == 0 || log_idx > AMC_LOG_MAX_RECS || buf_off == 0)
        return GCQ_ERRORS_CONSUMER_NOT_AVAILABLE;

    /*
     * Scan messages BACKWARDS from most recent (log_idx-1) to oldest (0)
     * to find the LATEST version announcement. This ensures we get the
     * current firmware version, not stale version strings from previous
     * firmware loads that may still be in the circular log buffer.
     */
    for (i = log_idx; i > 0; i--) {
        volatile uint8_t *rec_src;
        char msg[AMC_LOG_ENTRY_SIZE + 1];
        const char *p;
        int j;
        unsigned int maj_u, min_u, pat_u;

        rec_src = (volatile uint8_t *)pxCtx->gcq_payload_base_virt_addr +
                  buf_off + ((i - 1) * AMC_LOG_ENTRY_SIZE);

        __sync_synchronize();
        for (j = 0; j < AMC_LOG_ENTRY_SIZE; j++)
            msg[j] = rec_src[j];
        __sync_synchronize();
        msg[AMC_LOG_ENTRY_SIZE] = '\0';

        /* Walk every position in this record looking for "AMC" followed by version */
        for (p = msg; *p != '\0'; p++) {
            maj_u = 0;
            min_u = 0;
            pat_u = 0;

            /* Look for "AMC" prefix (case-insensitive) followed by optional separator and version.
             * Matches: "AMC: 3.0.0", "AMC 3.0.0", "AMC v3.0.0", etc. */
            if ((p[0] == 'A' || p[0] == 'a') &&
                (p[1] == 'M' || p[1] == 'm') &&
                (p[2] == 'C' || p[2] == 'c')) {
                const char *ver_start = p + 3;

                /* Skip optional separator characters (space, colon, 'v', etc.) */
                while (*ver_start == ' ' || *ver_start == ':' ||
                       *ver_start == 'v' || *ver_start == 'V')
                    ver_start++;

                if (sscanf(ver_start, "%u.%u.%u", &maj_u, &min_u, &pat_u) == 3 &&
                    maj_u < 100 && min_u < 100 && pat_u < 100) {
                    *major = (uint8_t)maj_u;
                    *minor = (uint8_t)min_u;
                    *patch = (uint8_t)pat_u;
                    return GCQ_ERRORS_NONE;
                }
            }
        }
    }

    return GCQ_ERRORS_CONSUMER_NOT_AVAILABLE;
}

/**
 * gcq_userspace_pause_logging() - Pause the logging thread to avoid errors during resets
 * @ctx: GCQ context
 *
 * Return: GCQ_ERRORS_NONE on success
 */
uint32_t gcq_userspace_pause_logging(gcq_metal_ctx *ctx)
{
    struct gcq_metal_ctx_internal *pxCtx = (struct gcq_metal_ctx_internal *)ctx;

    if (!pxCtx)
        return GCQ_ERRORS_INVALID_ARG;

    pxCtx->log_running = false;
    return GCQ_ERRORS_NONE;
}

/**
 * gcq_userspace_resume_logging() - Resume the logging thread after reset
 * @ctx: GCQ context
 *
 * Return: GCQ_ERRORS_NONE on success
 */
uint32_t gcq_userspace_resume_logging(gcq_metal_ctx *ctx)
{
    struct gcq_metal_ctx_internal *pxCtx = (struct gcq_metal_ctx_internal *)ctx;

    if (!pxCtx)
        return GCQ_ERRORS_INVALID_ARG;

    pxCtx->log_running = true;
    return GCQ_ERRORS_NONE;
}
