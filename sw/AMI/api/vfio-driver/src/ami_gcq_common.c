/**
 * Copyright (C) 2023 - 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Common AMI GCQ operations
 *
 * Implements common GCQ commands like IDENTIFY, HEARTBEAT used across all AMI functions
 *
 * @file ami_gcq_common.c
 */

#include "gcq_userspace.h"
#include "ami_sdr.h"
#include "ami_amc_shared_mem.h"
#include "ami_gcq_common.h"
#include "ami_ioctl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

/* Shared memory magic number from AMC firmware */
#define AMC_SHARED_MEM_MAGIC    0x564D5230  /* "VMR0" */

/*
 * The pending response queue and CID allocator are per-context (stored in
 * gcq_metal_ctx, reached via gcq_userspace_pending()/gcq_userspace_cid_counter()).
 * Keeping them per-device means command IDs and out-of-order completions never
 * cross between devices, so separate devices can be probed on separate threads
 * concurrently without a shared lock. Within a single context the ordering is
 * unchanged from the previous global implementation.
 */

static void add_to_pending_queue(gcq_metal_ctx *ctx, struct com_queue_entry *resp);
static int ami_gcq_drain_cq_hardware(gcq_metal_ctx *ctx);

static int pending_queue_has_cid(struct gcq_pending_queue *pq, uint16_t cid)
{
    for (int i = 0; i < pq->count; i++) {
        if (pq->responses[i].hdr.cid == cid)
            return 1;
    }
    return 0;
}

/* Drop responses for commands that already completed (cid <= max_cid). */
static void purge_pending_cid_le(gcq_metal_ctx *ctx, uint16_t max_cid)
{
    struct gcq_pending_queue *pq = gcq_userspace_pending(ctx);
    int w = 0;

    if (!pq)
        return;

    for (int i = 0; i < pq->count; i++) {
        if (pq->responses[i].hdr.cid > max_cid)
            pq->responses[w++] = pq->responses[i];
    }
    pq->count = w;
}

/* Read every completion currently in the hardware CQ into the pending queue. */
static void drain_hardware_cq_to_pending(gcq_metal_ctx *ctx)
{
    struct com_queue_entry resp;
    uint32_t resp_size;
    uint32_t result;

    if (!ctx)
        return;

    while (1) {
        resp_size = sizeof(resp);
        result = gcq_userspace_read(ctx, (uint8_t *)&resp, &resp_size, 0);
        if (result != GCQ_ERRORS_NONE)
            break;
        add_to_pending_queue(ctx, &resp);
    }
}

uint16_t ami_gcq_allocate_cid(gcq_metal_ctx *ctx)
{
    uint16_t *counter = gcq_userspace_cid_counter(ctx);

    if (!counter)
        return 1;

    /* Atomic: within a context the heartbeat thread and the caller thread can
     * both allocate CIDs; keep the increment race-free. */
    return __atomic_fetch_add(counter, 1, __ATOMIC_RELAXED);
}

void ami_gcq_free_cid(uint16_t cid)
{
    /* Simple implementation - just increment counter */
    (void)cid;
}

/**
 * Check if a response with the given CID exists in the pending queue
 * If found, copy it to resp_out and remove it from the queue
 */
static int check_pending_queue(gcq_metal_ctx *ctx, uint16_t expected_cid,
                               struct com_queue_entry *resp_out)
{
    struct gcq_pending_queue *pq = gcq_userspace_pending(ctx);

    if (!pq)
        return -1;

    for (int i = 0; i < pq->count; i++) {
        if (pq->responses[i].hdr.cid == expected_cid) {
            /* Found matching response - copy it out */
            memcpy(resp_out, &pq->responses[i], sizeof(*resp_out));

            /* Remove from queue by shifting remaining entries */
            for (int j = i; j < pq->count - 1; j++) {
                pq->responses[j] = pq->responses[j + 1];
            }
            pq->count--;

            return 0;  /* Success */
        }
    }
    return -1;  /* Not found */
}

/**
 * Add a response to the pending queue (ignore duplicate CIDs).
 */
static void add_to_pending_queue(gcq_metal_ctx *ctx, struct com_queue_entry *resp)
{
    struct gcq_pending_queue *pq = gcq_userspace_pending(ctx);

    if (!pq || !resp)
        return;

    /* Firmware may post the same completion more than once; keep the first copy. */
    if (pending_queue_has_cid(pq, resp->hdr.cid))
        return;

    /* Background heartbeat completions are fire-and-forget — never queue them */
    if (resp->hdr.cid == GCQ_HEARTBEAT_CID)
        return;

    if (pq->count < GCQ_MAX_PENDING_RESPONSES) {
        memcpy(&pq->responses[pq->count], resp, sizeof(*resp));
        pq->count++;
        return;
    }

    /* Queue full: discard stale low-CID entries, then append. */
    if (pq->count > 0) {
        uint16_t min_cid = pq->responses[0].hdr.cid;

        for (int i = 1; i < pq->count; i++) {
            if (pq->responses[i].hdr.cid < min_cid)
                min_cid = pq->responses[i].hdr.cid;
        }
        purge_pending_cid_le(ctx, min_cid);
    }

    if (pq->count < GCQ_MAX_PENDING_RESPONSES) {
        memcpy(&pq->responses[pq->count], resp, sizeof(*resp));
        pq->count++;
    } else {
        fprintf(stderr, "[GCQ] WARNING: Pending queue full (%d), dropping response CID %u\n",
                pq->count, resp->hdr.cid);
    }
}

/**
 * Wait for GCQ response with specific CID
 *
 * CRITICAL FIX: This function now properly consumes ALL responses from the GCQ,
 * not just the expected one. Non-matching responses are stored in a pending queue
 * to prevent the firmware's completion queue from filling up.
 */
int ami_gcq_wait_response(gcq_metal_ctx *ctx, uint16_t expected_cid,
                          struct com_queue_entry *resp_out, uint32_t timeout_ms)
{
    struct timespec start, now;
    uint32_t result;
    struct com_queue_entry resp;
    uint32_t resp_size;
    uint64_t elapsed_ms;

    if (!ctx || !resp_out) {
        return -1;
    }

    /* First check if the response is already in the pending queue */
    if (check_pending_queue(ctx, expected_cid, resp_out) == 0) {
        purge_pending_cid_le(ctx, expected_cid);
        return 0;
    }

    /* Pull in any completions already sitting in the hardware CQ */
    drain_hardware_cq_to_pending(ctx);
    if (check_pending_queue(ctx, expected_cid, resp_out) == 0) {
        purge_pending_cid_le(ctx, expected_cid);
        return 0;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        /*
         * Drain every completion currently in the hardware CQ (4 slots).
         * Never sleep while CQ still has data — that was wedging AMI Proxy
         * (BIM ERROR 0x03030000) mid-transfer around packet 11+.
         */
        for (;;) {
            resp_size = sizeof(resp);
            result = gcq_userspace_read(ctx, (uint8_t *)&resp, &resp_size, 0);

            if (result != GCQ_ERRORS_NONE)
                break;

            if (resp.hdr.cid == expected_cid) {
                memcpy(resp_out, &resp, sizeof(*resp_out));
                purge_pending_cid_le(ctx, expected_cid);
                return 0;
            }

            if (resp.hdr.cid == GCQ_HEARTBEAT_CID)
                continue;

            add_to_pending_queue(ctx, &resp);
        }

        if (check_pending_queue(ctx, expected_cid, resp_out) == 0) {
            purge_pending_cid_le(ctx, expected_cid);
            return 0;
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 +
                     (now.tv_nsec - start.tv_nsec) / 1000000;

        if (elapsed_ms >= timeout_ms) {
            ami_gcq_drain_cq_hardware(ctx);
            return -1;
        }

        usleep(100);  /* Poll every 100us instead of 1ms for faster response */
    }

    return -1;
}

/**
 * Wait for one PDI/GCQ response (strict sequential CIDs).
 *
 * Does not use the pending queue — a stale queued CID could make the host
 * think a chunk completed while completions still occupy the 4-slot CQ.
 */
int ami_gcq_wait_response_pdi(gcq_metal_ctx *ctx, uint16_t expected_cid,
                              struct com_queue_entry *resp_out, uint32_t timeout_ms)
{
    struct timespec start, now;
    struct com_queue_entry resp;
    uint32_t resp_size;
    uint32_t result;
    uint16_t last_stale_cid = 0;
    uint64_t elapsed_ms;

    if (!ctx || !resp_out)
        return -1;

    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        resp_size = sizeof(resp);
        result = gcq_userspace_read(ctx, (uint8_t *)&resp, &resp_size, 500);

        if (result == GCQ_ERRORS_NONE) {
            if (resp.hdr.cid == expected_cid) {
                memcpy(resp_out, &resp, sizeof(*resp_out));
                gcq_userspace_cq_drain(ctx, NULL);
                return 0;
            }

            /*
             * Consume stale completions (already advanced consumed on read).
             * Never cq_purge() here — that can skip the matching CID still in
             * the ring and wedge AMI proxy (BIM ERROR 0x03030000).
             */
            if (resp.hdr.cid != last_stale_cid) {
                fprintf(stderr,
                        "[GCQ] PDI: ignoring stale CID %u (waiting for %u)\n",
                        resp.hdr.cid, expected_cid);
                last_stale_cid = resp.hdr.cid;
            }
            continue;
        }

        if (result != GCQ_ERRORS_CONSUMER_NO_DATA_RECEIVED &&
            result != GCQ_ERRORS_TIMEOUT) {
            fprintf(stderr,
                    "[GCQ] PDI: read error %u waiting for CID %u\n",
                    result, expected_cid);
            return -1;
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 +
                     (now.tv_nsec - start.tv_nsec) / 1000000;

        if (elapsed_ms >= timeout_ms) {
            fprintf(stderr,
                    "[GCQ] PDI timeout waiting for CID %u\n",
                    expected_cid);
            gcq_userspace_cq_drain(ctx, NULL);
            return -1;
        }
    }
}

/**
 * Clear stale GCQ responses left in the pending queue after chunked transfers.
 */
void ami_gcq_flush_pending_responses(gcq_metal_ctx *ctx)
{
    struct gcq_pending_queue *pq = gcq_userspace_pending(ctx);

    if (pq && pq->count > 0) {
        fprintf(stderr, "[GCQ] Flushing %d stale pending response(s)\n",
                pq->count);
        pq->count = 0;
    }
}

void ami_gcq_reset_session(gcq_metal_ctx *ctx)
{
    uint16_t *counter = gcq_userspace_cid_counter(ctx);

    if (counter)
        *counter = 1;

    ami_gcq_flush_pending_responses(ctx);
}

void ami_gcq_drain_after_chunk(gcq_metal_ctx *ctx)
{
    gcq_userspace_cq_drain(ctx, NULL);
}

/* Drain hardware CQ so a failed/aborted transfer cannot wedge the 4-slot ring. */
static int ami_gcq_drain_cq_hardware(gcq_metal_ctx *ctx)
{
    struct com_queue_entry resp;
    uint32_t resp_size;
    uint32_t result;
    int drained = 0;

    if (!ctx)
        return 0;

    while (drained < 64) {
        resp_size = sizeof(resp);
        result = gcq_userspace_read(ctx, (uint8_t *)&resp, &resp_size, 0);
        if (result != GCQ_ERRORS_NONE)
            break;
        drained++;
    }

    return drained;
}

void ami_gcq_purge_cq_completions(gcq_metal_ctx *ctx)
{
    int total = 0;
    int round;

    if (!ctx)
        return;

    ami_gcq_flush_pending_responses(ctx);
    for (round = 0; round < 8; round++) {
        total += ami_gcq_drain_cq_hardware(ctx);
        gcq_userspace_cq_drain(ctx, NULL);
    }
    (void)total; /* Suppress unused warning */
}

/**
 * Send IDENTIFY command to AMC firmware
 *
 * This must be called first to initialize communication with AMC
 */
int ami_gcq_send_identify(gcq_metal_ctx *ctx,
                          uint8_t *amc_ver_major, uint8_t *amc_ver_minor, uint8_t *amc_ver_patch)
{
    struct amc_proxy_cmd_request req;
    struct com_queue_entry resp;
    uint32_t result;
    uint16_t cid;

    if (!ctx) {
        return -1;
    }

    /* Pause heartbeat to avoid GCQ contention - heartbeat interferes with IDENTIFY response time */
    extern uint32_t gcq_userspace_pause_heartbeat(gcq_metal_ctx *ctx, bool pause);
    gcq_userspace_pause_heartbeat(ctx, true);

    /* Allocate command ID */
    cid = ami_gcq_allocate_cid(ctx);

    /* Build IDENTIFY request */
    memset(&req, 0, sizeof(req));
    req.hdr.opcode = AMC_PROXY_CMD_OPCODE_IDENTIFY;
    req.hdr.state = 1;  /* Mark as new entry */
    req.hdr.cid = cid;
    req.hdr.count = 0;  /* No payload for IDENTIFY */

    /* Send request - MUST send full 512-byte slot size */
    result = gcq_userspace_write(ctx, (uint8_t *)&req, 512, 5000);

    if (result != GCQ_ERRORS_NONE) {
        fprintf(stderr, "[GCQ] IDENTIFY write failed: %u\n", result);
        gcq_userspace_pause_heartbeat(ctx, false);
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Wait for response */
    if (ami_gcq_wait_response(ctx, cid, &resp, 5000) != 0) {
        gcq_userspace_pause_heartbeat(ctx, false);
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Resume heartbeat */
    gcq_userspace_pause_heartbeat(ctx, false);

    /* Check result */
    if (resp.rcode != AMC_PROXY_RESULT_SUCCESS) {
        fprintf(stderr, "[GCQ] IDENTIFY failed with rcode %u\n", resp.rcode);
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Firmware packs version in ulPayload[0] (see ami_proxy_driver.c) */
    if (amc_ver_major)
        *amc_ver_major = (uint8_t)(resp.payload[0] & 0xFF);
    if (amc_ver_minor)
        *amc_ver_minor = (uint8_t)((resp.payload[0] >> 8) & 0xFF);
    if (amc_ver_patch)
        *amc_ver_patch = (uint8_t)((resp.payload[0] >> 16) & 0xFF);

    ami_gcq_free_cid(cid);
    return 0;
}

/**
 * Send HEARTBEAT command to AMC firmware
 */
int ami_gcq_send_heartbeat(gcq_metal_ctx *ctx)
{
    struct amc_proxy_cmd_request req;
    struct com_queue_entry resp;
    uint32_t result;
    uint16_t cid;

    if (!ctx) {
        return -1;
    }

    cid = ami_gcq_allocate_cid(ctx);

    /* Build HEARTBEAT request (legacy amc_proxy_request_heartbeat) */
    memset(&req, 0, sizeof(req));
    req.hdr.opcode = AMC_PROXY_CMD_OPCODE_HEARTBEAT;
    req.hdr.state = 1;
    req.hdr.cid = cid;
    req.hdr.count = 1;
    req.raw_payload[0] = cid & 0xFF;

    /* Send request — full 512-byte SQ slot like legacy gcq_write() */
    result = gcq_userspace_write(ctx, (uint8_t *)&req, 512, 1000);
    if (result != GCQ_ERRORS_NONE) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Wait for response */
    if (ami_gcq_wait_response(ctx, cid, &resp, 1000) != 0) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    ami_gcq_free_cid(cid);
    return (resp.rcode == AMC_PROXY_RESULT_SUCCESS) ? 0 : -1;
}

/**
 * ami_gcq_get_sdr() - Fetch SDR (Sensor Data Records) from firmware
 *
 * Sends GET_SDR GCQ command and parses the response into sdr_repo structure.
 * The firmware writes SDR data to shared memory at the specified offset.
 */
int ami_gcq_get_sdr(gcq_metal_ctx *ctx, uint8_t repo_type, struct sdr_repo *repo_out)
{
    struct amc_proxy_cmd_request req = {0};
    struct com_queue_entry resp;
    uint8_t *sdr_buffer = NULL;
    uint16_t cid;
    uint32_t result;
    int ret = -1;

    /* Forward declare from ami_sdr.h */
    extern int ami_parse_sdr(uint8_t *sdr_buf, struct sdr_repo *repo);

    if (!ctx || !repo_out) {
        return -1;
    }

    /* Allocate response buffer (64KB) */
    #define SDR_RESP_LEN (64 * 1024)
    sdr_buffer = (uint8_t *)malloc(SDR_RESP_LEN);
    if (!sdr_buffer) {
        fprintf(stderr, "[GET_SDR] ERROR: Failed to allocate SDR buffer\n");
        return -1;
    }

    /* Get CID */
    cid = ami_gcq_allocate_cid(ctx);


    /* Build GET_SDR request
     * CRITICAL: Each repo type needs its OWN buffer offset so responses don't overwrite each other!
     * Use repo_type as an offset multiplier: base + (repo_type * 16KB)
     */
    uint32_t response_offset = 0x32D4 + (repo_type * 0x4000);  /* 16KB per repo type */

    req.hdr.opcode = AMC_PROXY_CMD_OPCODE_SENSOR;
    req.hdr.state = 1;  /* New entry */
    req.hdr.cid = cid;
    req.hdr.count = sizeof(req.sensor_payload);

    req.sensor_payload.sid = repo_type;  /* Repo type: TEMP=2, VOLTAGE=3, etc. */
    req.sensor_payload.aid = AMC_PROXY_CMD_SENSOR_REQUEST_GET_SDR;  /* Use GET_SDR = 2 */
    req.sensor_payload.sensor_id = 0;
    req.sensor_payload.address = response_offset;
    req.sensor_payload.size = SDR_RESP_LEN;

    /* On transient enumeration/overview opens, fail faster (2.5s) than the full
     * 5s so a half-responsive card doesn't stall a status snapshot. Kept well
     * above the measured healthy BDINFO time (~20ms) to avoid ever timing out a
     * slow-but-healthy card and mislabelling it NO_AMC. */
    uint32_t sdr_timeout = gcq_userspace_is_quick_open(ctx) ? 2500 : 5000;

    /* Send command (512 bytes for GCQ slot size) */
    result = gcq_userspace_write(ctx, (uint8_t *)&req, 512, sdr_timeout);
    if (result != GCQ_ERRORS_NONE) {
        fprintf(stderr, "[GET_SDR] ERROR: gcq_userspace_write failed: %d\n", result);
        free(sdr_buffer);
        ami_gcq_free_cid(cid);
        return -1;
    }


    /* Wait for response */
    if (ami_gcq_wait_response(ctx, cid, &resp, sdr_timeout) != 0) {
        fprintf(stderr, "[GET_SDR] ERROR: No response from firmware\n");
        free(sdr_buffer);
        ami_gcq_free_cid(cid);
        return -1;
    }


    /* Check response code */
    if (resp.rcode != AMC_PROXY_RESULT_SUCCESS) {
        /* Repo not supported/available on this hardware - this is expected/normal */
        if (resp.rcode == AMC_PROXY_RESULT_INVALID_VALUE ||
            resp.rcode == AMC_PROXY_RESULT_GET_REQUEST_FAILED) {
            free(sdr_buffer);
            ami_gcq_free_cid(cid);
            return 1;  /* Return 1 to indicate "not supported" (not an error) */
        }
        /* Genuine firmware error */
        fprintf(stderr, "[GET_SDR] ERROR: Firmware returned error code %d (repo_type=%d)\n", resp.rcode, repo_type);
        free(sdr_buffer);
        ami_gcq_free_cid(cid);
        return -1;
    }


    /* Read SDR data from shared memory
     * CRITICAL: Address 0x32D4 is RELATIVE to shared memory base (BAR0+AMC_SHARED_MEM_OFFSET)
     * Firmware writes to: BAR0 + AMC_SHARED_MEM_OFFSET + 0x32D4 = BAR0 + 0x42D4
     * So we read from: BAR0 + AMC_SHARED_MEM_OFFSET + 0x32D4
     * Must use volatile reads like memcpy_fromio() to get fresh data!
     */
    void *bar0 = gcq_userspace_get_bar0(ctx);
    if (!bar0) {
        fprintf(stderr, "[GET_SDR] ERROR: Cannot get BAR0 pointer\n");
        free(sdr_buffer);
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Use volatile reads to bypass cache
     * Read from the SAME offset we told firmware to write to
     */
    __sync_synchronize();
    volatile uint8_t *src = (volatile uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET + response_offset;
    for (int i = 0; i < SDR_RESP_LEN; i++) {
        sdr_buffer[i] = src[i];
    }
    __sync_synchronize();


    /* First byte is completion code */
    uint8_t completion_code = sdr_buffer[0];

    #define SDR_CODE_OP_SUCCESS 0x01
    #define SDR_CODE_NOT_AVAILABLE 0x00

    if (completion_code != SDR_CODE_OP_SUCCESS) {  /* 1 = success */
        fprintf(stderr, "[GET_SDR] ERROR: SDR completion code %d (expected 1)\n", completion_code);
        free(sdr_buffer);
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Parse SDR data
     * Byte 0: Completion code (already checked)
     * Byte 1+: SDR header starts here (repo_type will be 0xC1, 0xC2, etc.)
     */
    ret = ami_parse_sdr(sdr_buffer + 1, repo_out);  /* Skip only completion code */

    free(sdr_buffer);
    ami_gcq_free_cid(cid);
    return ret;
}

/**
 * ami_gcq_eeprom_read() - Read from EEPROM via GCQ
 */
int ami_gcq_eeprom_read(gcq_metal_ctx *ctx, uint8_t offset, uint8_t len, uint8_t *data_out)
{
    struct amc_proxy_cmd_request req = {0};
    struct com_queue_entry resp;
    uint16_t cid;
    uint32_t result;
    /* Use DATA region offset like old driver: amc_data_start + (1MB * 1) = 0x32D4 + 0x100000 = 0x1032D4 */
    uint32_t response_offset = 0x32D4 + 0x100000;  /* DATA buffer region */

    if (!ctx || !data_out) {
        return -1;
    }

    cid = ami_gcq_allocate_cid(ctx);

    /* Build EEPROM read request */
    req.hdr.opcode = AMC_PROXY_CMD_OPCODE_EEPROM_READ_WRITE;
    req.hdr.state = 1;
    req.hdr.cid = cid;
    req.hdr.count = sizeof(req.eeprom_payload);

    req.eeprom_payload.address = response_offset;  /* Offset from shared mem base, firmware adds base */
    req.eeprom_payload.req_type = AMC_PROXY_CMD_RW_REQUEST_READ;  /* 0 = read */
    req.eeprom_payload.len = len;
    req.eeprom_payload.offset = offset;
    req.eeprom_payload.resvd = 0;

    /* Send command */
    result = gcq_userspace_write(ctx, (uint8_t *)&req, 512, 5000);
    if (result != GCQ_ERRORS_NONE) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Wait for response */
    if (ami_gcq_wait_response(ctx, cid, &resp, 5000) != 0) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    if (resp.rcode != AMC_PROXY_RESULT_SUCCESS) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Read EEPROM data from shared memory */
    void *bar0 = gcq_userspace_get_bar0(ctx);
    if (!bar0) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Use volatile reads */
    __sync_synchronize();
    volatile uint8_t *src = (volatile uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET + response_offset;
    for (int i = 0; i < len; i++) {
        data_out[i] = src[i];
    }
    __sync_synchronize();

    ami_gcq_free_cid(cid);
    return 0;
}

/**
 * ami_gcq_eeprom_write() - Write to EEPROM via GCQ
 */
int ami_gcq_eeprom_write(gcq_metal_ctx *ctx, uint8_t offset, uint8_t len, uint8_t *data_in)
{
    struct amc_proxy_cmd_request req = {0};
    struct com_queue_entry resp;
    uint16_t cid;
    uint32_t result;
    /* Use DATA region offset like old driver: amc_data_start + (1MB * 1) */
    uint32_t data_offset = 0x32D4 + 0x100000;

    if (!ctx || !data_in) {
        return -1;
    }

    /* First, write data to shared memory */
    void *bar0 = gcq_userspace_get_bar0(ctx);
    if (!bar0) {
        return -1;
    }

    /* Write data to shared memory (volatile writes) */
    __sync_synchronize();
    volatile uint8_t *dest = (volatile uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET + data_offset;
    for (int i = 0; i < len; i++) {
        dest[i] = data_in[i];
    }
    __sync_synchronize();

    cid = ami_gcq_allocate_cid(ctx);

    /* Build EEPROM write request */
    req.hdr.opcode = AMC_PROXY_CMD_OPCODE_EEPROM_READ_WRITE;
    req.hdr.state = 1;
    req.hdr.cid = cid;
    req.hdr.count = sizeof(req.eeprom_payload);

    req.eeprom_payload.address = data_offset;  /* Offset from shared mem base, firmware adds base */
    req.eeprom_payload.req_type = AMC_PROXY_CMD_RW_REQUEST_WRITE;  /* 1 = write */
    req.eeprom_payload.len = len;
    req.eeprom_payload.offset = offset;
    req.eeprom_payload.resvd = 0;

    /* Send command */
    result = gcq_userspace_write(ctx, (uint8_t *)&req, 512, 5000);
    if (result != GCQ_ERRORS_NONE) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Wait for response */
    if (ami_gcq_wait_response(ctx, cid, &resp, 5000) != 0) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    ami_gcq_free_cid(cid);
    return (resp.rcode == AMC_PROXY_RESULT_SUCCESS) ? 0 : -1;
}

/**
 * ami_gcq_module_read() - Read from module (QSFP) via GCQ
 */
int ami_gcq_module_read(gcq_metal_ctx *ctx, uint8_t device_id, uint8_t page,
                        uint8_t offset, uint8_t len, uint8_t *data_out)
{
    struct amc_proxy_cmd_request req = {0};
    struct com_queue_entry resp;
    uint16_t cid;
    uint32_t result;
    uint32_t response_offset = 0x32D4 + 0x200000;  /* DATA buffer region offset 2MB */

    if (!ctx || !data_out) {
        return -1;
    }

    cid = ami_gcq_allocate_cid(ctx);

    /* Build module read request */
    req.hdr.opcode = AMC_PROXY_CMD_OPCODE_MODULE_READ_WRITE;
    req.hdr.state = 1;
    req.hdr.cid = cid;
    req.hdr.count = sizeof(req.module_payload);

    req.module_payload.req_type = AMC_PROXY_CMD_RW_REQUEST_READ;
    req.module_payload.address = response_offset;
    req.module_payload.device_id = device_id;
    req.module_payload.page = page;
    req.module_payload.offset = offset;
    req.module_payload.len = len;
    req.module_payload.resvd = 0;

    /* Send command */
    result = gcq_userspace_write(ctx, (uint8_t *)&req, 512, 5000);
    if (result != GCQ_ERRORS_NONE) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Wait for response */
    if (ami_gcq_wait_response(ctx, cid, &resp, 5000) != 0) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    if (resp.rcode != AMC_PROXY_RESULT_SUCCESS) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Read module data from shared memory */
    void *bar0 = gcq_userspace_get_bar0(ctx);
    if (!bar0) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Use volatile reads */
    __sync_synchronize();
    volatile uint8_t *src = (volatile uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET + response_offset;
    for (int i = 0; i < len; i++) {
        data_out[i] = src[i];
    }
    __sync_synchronize();

    ami_gcq_free_cid(cid);
    return 0;
}

/**
 * ami_gcq_module_write() - Write to module (QSFP) via GCQ
 */
int ami_gcq_module_write(gcq_metal_ctx *ctx, uint8_t device_id, uint8_t page,
                         uint8_t offset, uint8_t len, uint8_t *data_in)
{
    struct amc_proxy_cmd_request req = {0};
    struct com_queue_entry resp;
    uint16_t cid;
    uint32_t result;
    uint32_t data_offset = 0x32D4 + 0x200000;

    if (!ctx || !data_in) {
        return -1;
    }

    /* First, write data to shared memory */
    void *bar0 = gcq_userspace_get_bar0(ctx);
    if (!bar0) {
        return -1;
    }

    /* Write data to shared memory (volatile writes) */
    __sync_synchronize();
    volatile uint8_t *dest = (volatile uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET + data_offset;
    for (int i = 0; i < len; i++) {
        dest[i] = data_in[i];
    }
    __sync_synchronize();

    cid = ami_gcq_allocate_cid(ctx);

    /* Build module write request */
    req.hdr.opcode = AMC_PROXY_CMD_OPCODE_MODULE_READ_WRITE;
    req.hdr.state = 1;
    req.hdr.cid = cid;
    req.hdr.count = sizeof(req.module_payload);

    req.module_payload.req_type = AMC_PROXY_CMD_RW_REQUEST_WRITE;
    req.module_payload.address = data_offset;
    req.module_payload.device_id = device_id;
    req.module_payload.page = page;
    req.module_payload.offset = offset;
    req.module_payload.len = len;
    req.module_payload.resvd = 0;

    /* Send command */
    result = gcq_userspace_write(ctx, (uint8_t *)&req, 512, 5000);
    if (result != GCQ_ERRORS_NONE) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Wait for response */
    if (ami_gcq_wait_response(ctx, cid, &resp, 5000) != 0) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    ami_gcq_free_cid(cid);
    return (resp.rcode == AMC_PROXY_RESULT_SUCCESS) ? 0 : -1;
}

/**
 * ami_gcq_device_boot() - Trigger device boot via GCQ
 */
int ami_gcq_device_boot(gcq_metal_ctx *ctx, uint32_t partition)
{
    struct amc_proxy_cmd_request req = {0};
    uint16_t cid;
    uint32_t result;

    if (!ctx) {
        fprintf(stderr, "[DEVICE_BOOT] ERROR: ctx is NULL\n");
        return -1;
    }

    cid = ami_gcq_allocate_cid(ctx);

    /* Build device boot request - Use opcode 0x0 (BOOT_SEL) to trigger POR */
    req.hdr.opcode = AMC_PROXY_CMD_OPCODE_BOOT_SEL;
    req.hdr.state = 1;
    req.hdr.cid = cid;
    req.hdr.count = sizeof(req.pdi_payload);

    /* Initialize all fields to zero */
    memset(&req.pdi_payload, 0, sizeof(req.pdi_payload));

    /* Only set partition_sel field (like kernel driver does) */
    req.pdi_payload.partition_sel = partition;

    /* Send command */
    result = gcq_userspace_write(ctx, (uint8_t *)&req, 512, 5000);
    if (result != GCQ_ERRORS_NONE) {
        fprintf(stderr, "[DEVICE_BOOT] ERROR: gcq_write failed: %u\n", result);
        ami_gcq_free_cid(cid);
        return -1;
    }

    /*
     * CRITICAL: Do NOT wait for response after BOOT_SEL!
     *
     * BOOT_SEL triggers AMC firmware to reboot immediately:
     * - GCQ becomes unavailable as firmware reboots
     * - PCIe link may drop during reboot
     * - Any response would be stale/invalid
     * - Waiting for response would cause timeout errors
     *
     * The caller (ami_prog_device_boot) will close the GCQ session immediately
     * after this returns to stop background threads before PCIe link drops.
     */

    ami_gcq_free_cid(cid);

    return 0;
}

/**
 * ami_gcq_pdi_chunk_common() - Send PDI chunk via GCQ (opcode 0xA or 0xB)
 *
 * This is a common implementation for both PDI download (flash) and PDI program (live-load).
 * The opcode and partition magic determine the operation type.
 */
static int ami_gcq_pdi_chunk_common(gcq_metal_ctx *ctx,
                                     uint8_t opcode,
                                     const uint8_t *chunk_data,
                                     uint32_t chunk_len,
                                     uint32_t chunk_num,
                                     uint32_t chunk_size_kb,
                                     bool is_last_chunk,
                                     uint32_t boot_device,
                                     uint32_t partition,
                                     const uint8_t *pdi_md5,
                                     uint32_t total_pdi_size,
                                     ami_event_handler progress_cb,
                                     void *cb_data)
{
    struct amc_proxy_cmd_request req = {0};
    uint16_t cid;
    uint32_t result;
    uint32_t data_offset;
    uint32_t buffer_size;
    uint32_t xfer_len = chunk_len;
    uint32_t stride;
    uint32_t after_this;
    bool is_live_load = (partition == AMI_IOC_PDI_PROGRAM_MAGIC ||
                         partition == AMI_IOC_PDI_APU_PROGRAM_MAGIC ||
                         partition == AMI_IOC_PDI_RPU_PROGRAM_MAGIC);
    const char *op_name = is_live_load ? "PDI_PROGRAM" : "PDI_DOWNLOAD";

    if (!ctx || !chunk_data || !pdi_md5) {
        return -1;
    }

    /* Get shared memory data buffer info */
    void *bar0 = gcq_userspace_get_bar0(ctx);
    if (!bar0) {
        return -1;
    }
    struct amc_shared_mem *shm = (struct amc_shared_mem *)((uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET);
    uint32_t data_buffer_start = shm->data.amc_data_start;
    buffer_size = shm->data.amc_data_end - shm->data.amc_data_start;

    /* Validate chunk fits in buffer */
    if (chunk_len > buffer_size) {
        fprintf(stderr, "[%s] ERROR: Chunk size %u exceeds buffer size %u (start=0x%X)\n",
                op_name, chunk_len, buffer_size, data_buffer_start);
        return -1;
    }

    /*
     * CRITICAL: Use offset 0 for 6MB chunks (fits in 8MB buffer).
     * We write to: bar0 + AMC_SHARED_MEM_OFFSET + data_buffer_start
     * Firmware reads from: shared_mem_base + data_buffer_start
     * All chunks overwrite the same buffer, so firmware MUST copy to DDR
     * before next chunk arrives (hence all the wait_idle delays).
     */
    data_offset = data_buffer_start;

    /*
     * OSPI programs full pages (256 B). APC passes ulImageSize to HAL_FLUSH
     * and the flash driver, which reads ceil(len/256) pages from shared memory.
     */
    /* OSPI flash only: round up to 256 B pages. Live-load uses actual size. */
    if (!is_live_load && opcode == AMC_PROXY_CMD_OPCODE_PDI_DOWNLOAD &&
        (chunk_len % 256U) != 0)
        xfer_len = (chunk_len + 255U) & ~255U;

    /* Write chunk data to shared memory data buffer */
    {
        void *dest = (void *)((uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET + data_buffer_start);

        memcpy(dest, chunk_data, chunk_len);
        if (xfer_len > chunk_len)
            memset((uint8_t *)dest + chunk_len, 0, xfer_len - chunk_len);
        __sync_synchronize();
        msync(dest, xfer_len, MS_SYNC);
    }

    cid = ami_gcq_allocate_cid(ctx);

    /* Build PDI request - use BIT-PACKED pdi_payload structure (matches kernel driver!) */
    req.hdr.opcode = opcode;
    req.hdr.state = 1;
    req.hdr.cid = cid;
    req.hdr.count = sizeof(req.pdi_payload);

    /* Initialize payload */
    memset(&req.pdi_payload, 0, sizeof(req.pdi_payload));

    /* Fill in PDI download payload (bit-packed structure matching kernel driver) */
    stride = chunk_size_kb * 1024U;
    after_this = (chunk_num * stride) + chunk_len;

    req.pdi_payload.address = data_offset;
    req.pdi_payload.size = xfer_len;

    if (total_pdi_size > after_this)
        req.pdi_payload.remain_size = total_pdi_size - after_this;
    else
        req.pdi_payload.remain_size = 0;
    req.pdi_payload.boot_device = boot_device;

    /* Live-load type is encoded in program_* flags (ioctl partition magic), not partition_sel. */
    req.pdi_payload.update_fpt = (partition == AMI_IOC_FPT_UPDATE_MAGIC) ? 1 : 0;
    req.pdi_payload.program_pdi = (partition == AMI_IOC_PDI_PROGRAM_MAGIC) ? 1 : 0;
    req.pdi_payload.program_apu_pdi = (partition == AMI_IOC_PDI_APU_PROGRAM_MAGIC) ? 1 : 0;
    req.pdi_payload.program_rpu_pdi = (partition == AMI_IOC_PDI_RPU_PROGRAM_MAGIC) ? 1 : 0;

    if (req.pdi_payload.program_pdi || req.pdi_payload.program_apu_pdi ||
        req.pdi_payload.program_rpu_pdi) {
        req.pdi_payload.partition_sel = 0;
    } else if (partition == AMI_IOC_FPT_UPDATE_MAGIC) {
        req.pdi_payload.partition_sel = 0;
    } else {
        req.pdi_payload.partition_sel = partition & 0xF;
    }

    req.pdi_payload.last_chunk = is_last_chunk ? 1 : 0;
    req.pdi_payload.chunk = chunk_num;
    /*
     * ulPacketSize in firmware is in KB (see APC_BASE_PACKET_SIZE).
     * Must match the host transfer stride so OSPI dest = packet_num * (KB * 1024).
     * Using 6144 here caused 6 MB offsets while sending 256 KB chunks.
     */
    if (chunk_size_kb == 0) {
        fprintf(stderr, "[%s] ERROR: invalid chunk_size_kb (0)\n", op_name);
        ami_gcq_free_cid(cid);
        return -1;
    }
    req.pdi_payload.chunk_size = chunk_size_kb;

    /* Populate PDI metadata fields (required by firmware for FPT update) */
    if (pdi_md5) {
        memcpy(req.pdi_payload.pdi_md5, pdi_md5, MD5_SIZE);
    } else {
        memset(req.pdi_payload.pdi_md5, 0, MD5_SIZE);
    }
    req.pdi_payload.pdi_size = total_pdi_size;

    if (is_live_load && opcode != AMC_PROXY_CMD_OPCODE_PDI_DOWNLOAD) {
        fprintf(stderr,
                "[%s] ERROR: live-load requires GCQ opcode 0xA (got 0x%X); "
                "rebuild and install libami.so\n",
                op_name, (unsigned)opcode);
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Drain CQ periodically to prevent queue overflow - do this BEFORE write, no delays */
    if (chunk_num % 4 == 0) {
        gcq_userspace_cq_drain(ctx, NULL);
    }

    /* Send command with extended timeout for programming operations */
    uint32_t write_timeout_ms = is_last_chunk ? 300000 : 60000;  /* 5 min for last chunk, 1 min otherwise */
    result = gcq_userspace_write(ctx, (uint8_t *)&req, 512, write_timeout_ms);
    if (result != GCQ_ERRORS_NONE) {
        fprintf(stderr, "[%s] ERROR: gcq_write failed for chunk %u: %u\n", op_name, chunk_num, result);
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Wait for response with longer timeout for programming operations */
    struct com_queue_entry resp;
    if (ami_gcq_wait_response_pdi(ctx, cid, &resp, 180000) != 0) {
        fprintf(stderr, "[%s] ERROR: Timeout waiting for chunk %u (CID %u)\n",
                op_name, chunk_num, cid);
        gcq_userspace_cq_drain(ctx, NULL);
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Check response code - accept both 0 and 1 as success */
    if (resp.rcode != 0 && resp.rcode != 1) {
        if (resp.rcode == 4U)
            fprintf(stderr,
                    "[%s] AMC rejected request (GET_REQUEST_FAILED) — "
                    "wrong opcode, stale CQ, or AMI proxy ERROR; "
                    "recover card (FW reboot / PCI rescan / modprobe ami_uio)\n",
                    op_name);
        else
            fprintf(stderr, "[%s] ERROR: Chunk %u failed with rcode=%u\n",
                    op_name, chunk_num, resp.rcode);
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Drain at most one lap of the 4-slot CQ; never snap producer index here */
    gcq_userspace_cq_drain(ctx, NULL);

    /*
     * Pace APC work — legacy kernel driver blocked inside one ioctl per chunk.
     * Live-load must let in_band run iAPC_PdiProgram before the next chunk.
     *
     * CRITICAL: All these delays are necessary for reliability. Each chunk writes
     * to the SAME 8MB buffer (start=0x32D4), so firmware MUST finish copying chunk N
     * to DDR before we send chunk N+1, or we get data corruption and boot failures.
     */
    if (is_live_load) {
        (void)gcq_userspace_wait_idle(ctx, 120000);
        if (is_last_chunk)
            usleep(1000000);
        else
            usleep(200000);
    } else if (chunk_num == 0) {
        usleep(250000);
    } else if (chunk_num >= 10 || chunk_len != req.pdi_payload.size) {
        usleep(300000);  /* tail chunk / late OSPI */
    } else {
        usleep(150000);
    }

    (void)gcq_userspace_wait_idle(ctx, 60000);

    ami_gcq_free_cid(cid);

    /* Call progress callback if provided */
    if (progress_cb) {
        progress_cb(AMI_EVENT_STATUS_OK, chunk_len, cb_data);
    }

    /* Success - chunk was submitted to GCQ */
    return 0;
}

/**
 * ami_gcq_pdi_download_chunk() - Download PDI chunk to flash via GCQ (opcode 0xA)
 *
 * This function sends one chunk of a PDI file for flash programming.
 * Call multiple times for large files.
 */
int ami_gcq_pdi_download_chunk(gcq_metal_ctx *ctx,
                                const uint8_t *chunk_data,
                                uint32_t chunk_len,
                                uint32_t chunk_num,
                                uint32_t chunk_size_kb,
                                bool is_last_chunk,
                                uint32_t boot_device,
                                uint32_t partition,
                                const uint8_t *pdi_md5,
                                uint32_t total_pdi_size,
                                ami_event_handler progress_cb,
                                void *cb_data)
{
    return ami_gcq_pdi_chunk_common(ctx, AMC_PROXY_CMD_OPCODE_PDI_DOWNLOAD,
                                    chunk_data, chunk_len, chunk_num, chunk_size_kb,
                                    is_last_chunk, boot_device, partition,
                                    pdi_md5, total_pdi_size, progress_cb, cb_data);
}

/**
 * ami_gcq_pdi_program_chunk() - Live-load PDI chunk via GCQ (opcode 0xB)
 *
 * This function sends one chunk of a PDI file for live-loading (in-memory).
 * Call multiple times for large files.
 */
int ami_gcq_pdi_program_chunk(gcq_metal_ctx *ctx,
                               const uint8_t *chunk_data,
                               uint32_t chunk_len,
                               uint32_t chunk_num,
                               uint32_t chunk_size_kb,
                               bool is_last_chunk,
                               uint32_t boot_device,
                               uint32_t partition,
                               const uint8_t *pdi_md5,
                               uint32_t total_pdi_size,
                               ami_event_handler progress_cb,
                               void *cb_data)
{
    /*
     * Firmware live-load path (in_band_telemetry): AMI_CMD_OPCODE_PDI_DOWNLOAD_REQ
     * with program_pdi / program_apu_pdi / program_rpu_pdi set → iAPC_PdiProgram().
     * Opcode 0xB raises PDI_PROGRAM_START and targets flash download instead.
     */
    return ami_gcq_pdi_chunk_common(ctx, AMC_PROXY_CMD_OPCODE_PDI_DOWNLOAD,
                                    chunk_data, chunk_len, chunk_num, chunk_size_kb,
                                    is_last_chunk, boot_device, partition,
                                    pdi_md5, total_pdi_size, progress_cb, cb_data);
}

/**
 * ami_gcq_partition_copy() - Copy partition via GCQ
 */
int ami_gcq_partition_copy(gcq_metal_ctx *ctx, uint32_t src_device, uint32_t src_part,
                            uint32_t dest_device, uint32_t dest_part, uint32_t src_partition_size)
{
    struct amc_proxy_cmd_request req = {0};
    struct com_queue_entry resp;
    uint16_t cid;
    uint32_t result;
    uint32_t data_addr, buffer_size;

    if (!ctx) {
        return -1;
    }

    /* Get shared data memory region from AMC shared memory structure */
    void *bar0 = gcq_userspace_get_bar0(ctx);
    if (!bar0) {
        return -1;
    }

    struct amc_shared_mem *shm = (struct amc_shared_mem *)((uint8_t *)bar0 + AMC_SHARED_MEM_OFFSET);
    data_addr = shm->data.amc_data_start;
    buffer_size = shm->data.amc_data_end - shm->data.amc_data_start;

    if (buffer_size == 0) {
        fprintf(stderr, "[PARTITION_COPY] ERROR: No data buffer available (size=0)\n");
        return -1;
    }

    cid = ami_gcq_allocate_cid(ctx);

    /* Build partition copy request - use pdi_payload structure like kernel driver */
    req.hdr.opcode = AMC_PROXY_CMD_OPCODE_PARTITION_COPY;
    req.hdr.state = 1;
    req.hdr.cid = cid;
    req.hdr.count = sizeof(req.pdi_payload);

    /* Initialize payload */
    memset(&req.pdi_payload, 0, sizeof(req.pdi_payload));

    /* Set partition copy fields in bit-packed structure */
    req.pdi_payload.src_partition = src_part;
    req.pdi_payload.dest_partition = dest_part;
    req.pdi_payload.src_device = src_device;
    req.pdi_payload.dest_device = dest_device;

    /* CRITICAL: address = shared buffer offset, size = SOURCE PARTITION SIZE (not buffer size!)
     * Firmware will chunk the copy operation internally using the shared buffer */
    req.pdi_payload.address = data_addr;
    req.pdi_payload.size = src_partition_size;  /* This is the partition size, not buffer size */

    /* Send command */
    result = gcq_userspace_write(ctx, (uint8_t *)&req, 512, 30000);  /* Long timeout for copy */
    if (result != GCQ_ERRORS_NONE) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Wait for response - partition copy can take several minutes */
    if (ami_gcq_wait_response(ctx, cid, &resp, 600000) != 0) {  /* 10 minute timeout */
        ami_gcq_free_cid(cid);
        return -1;
    }

    ami_gcq_free_cid(cid);
    return (resp.rcode == AMC_PROXY_RESULT_SUCCESS) ? 0 : -1;
}

/**
 * ami_gcq_fpt_partition() - FPT partition operation via GCQ
 */
int ami_gcq_fpt_partition(gcq_metal_ctx *ctx, uint32_t boot_device)
{
    struct amc_proxy_cmd_request req = {0};
    struct com_queue_entry resp;
    uint16_t cid;
    uint32_t result;

    if (!ctx) {
        return -1;
    }

    cid = ami_gcq_allocate_cid(ctx);

    /* Build FPT partition request */
    req.hdr.opcode = AMC_PROXY_CMD_OPCODE_FPT_PARTITION;
    req.hdr.state = 1;
    req.hdr.cid = cid;
    req.hdr.count = sizeof(uint32_t);

    memcpy(req.raw_payload, &boot_device, sizeof(uint32_t));

    /* Send command */
    result = gcq_userspace_write(ctx, (uint8_t *)&req, 512, 30000);
    if (result != GCQ_ERRORS_NONE) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    /* Wait for response */
    if (ami_gcq_wait_response(ctx, cid, &resp, 60000) != 0) {
        ami_gcq_free_cid(cid);
        return -1;
    }

    ami_gcq_free_cid(cid);
    return (resp.rcode == AMC_PROXY_RESULT_SUCCESS) ? 0 : -1;
}
