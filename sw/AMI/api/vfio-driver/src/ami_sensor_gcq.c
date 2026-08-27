/**
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * AMI Sensor operations via GCQ userspace driver
 *
 * This file implements sensor read operations using direct GCQ commands
 * instead of kernel driver IOCTLs.
 *
 * @file ami_sensor_gcq.c
 */

#include "ami_sensor.h"
#include "ami_device.h"
#include "gcq_userspace.h"
#include "ami_gcq_common.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* Shared memory layout assumptions (from kernel driver analysis) */
#define AMC_LOG_PAGE_SIZE       4096
#define AMC_LOG_PAGE_OFFSET     0x1000  /* After GCQ ring buffers */

/**
 * Map AMI sensor type to AMC sensor repository ID
 *
 * Note: ami_sensor_type is a bitflag enum (1<<0, 1<<1, etc.)
 *       but repo IDs are sequential (2, 3, 4, 5)
 */
static uint32_t sensor_type_to_repo_id(enum ami_sensor_type type)
{
    /* AMI sensor types are bit flags:
     * AMI_SENSOR_TYPE_TEMP = 1 (1<<0)
     * AMI_SENSOR_TYPE_CURRENT = 2 (1<<1)
     * AMI_SENSOR_TYPE_VOLTAGE = 4 (1<<2)
     * AMI_SENSOR_TYPE_POWER = 8 (1<<3)
     */
    switch (type) {
    case AMI_SENSOR_TYPE_TEMP:    /* 1 -> 2 */
        return AMC_PROXY_CMD_SENSOR_REPO_TEMP;
    case AMI_SENSOR_TYPE_CURRENT: /* 2 -> 4 */
        return AMC_PROXY_CMD_SENSOR_REPO_CURRENT;
    case AMI_SENSOR_TYPE_VOLTAGE: /* 4 -> 3 */
        return AMC_PROXY_CMD_SENSOR_REPO_VOLTAGE;
    case AMI_SENSOR_TYPE_POWER:   /* 8 -> 5 */
        return AMC_PROXY_CMD_SENSOR_REPO_POWER;
    default:
        return 0;  /* Unknown/invalid */
    }
}

/**
 * Read sensor via GCQ userspace driver
 *
 * @param ctx           GCQ context
 * @param sensor_type   Type of sensor (temp, voltage, current, power)
 * @param sensor_id     Specific sensor ID
 * @param value_out     Pointer to receive sensor value
 *
 * @return 0 on success, negative error code on failure
 */
int ami_sensor_read_via_gcq(gcq_metal_ctx *ctx,
                             enum ami_sensor_type sensor_type,
                             int sensor_id,
                             int64_t *value_out)
{
    struct amc_proxy_cmd_request req;
    struct com_queue_entry resp;
    uint32_t resp_size;
    uint32_t result;
    uint16_t cid;
    struct timespec start, now;
    uint32_t timeout_ms = 5000;  /* 5 second timeout */

    fprintf(stderr, "[GCQ_SENSOR] Entry: ctx=%p sensor_type=%d sensor_id=%d\n", ctx, sensor_type, sensor_id);

    if (!ctx || !value_out) {
        fprintf(stderr, "[GCQ_SENSOR] ERROR: NULL parameter (ctx=%p value_out=%p)\n", ctx, value_out);
        return -1;
    }

    /* Allocate command ID */
    cid = ami_gcq_allocate_cid(ctx);
    fprintf(stderr, "[GCQ_SENSOR] Allocated CID=%d\n", cid);

    /* Build GCQ request */
    memset(&req, 0, sizeof(req));
    req.hdr.opcode = AMC_PROXY_CMD_OPCODE_SENSOR;
    req.hdr.state = 1;  /* Mark as new entry */
    req.hdr.cid = cid;
    req.hdr.count = sizeof(req.sensor_payload);

    /* Fill sensor-specific payload */
    req.sensor_payload.sid = sensor_type_to_repo_id(sensor_type);
    req.sensor_payload.aid = AMC_PROXY_CMD_SENSOR_REQUEST_GET_SINGLE_SDR;
    req.sensor_payload.sensor_id = sensor_id;
    req.sensor_payload.address = 0x4000;  /* Use safe offset - well past shared mem corruption */
    req.sensor_payload.size = 4096;

    fprintf(stderr, "[GCQ_SENSOR] Request: opcode=0x%x sid=%d aid=%d sensor_id=%d addr=0x%lx\n",
            req.hdr.opcode, req.sensor_payload.sid, req.sensor_payload.aid, req.sensor_payload.sensor_id,
            req.sensor_payload.address);

    /* Send request via GCQ - must be 512 bytes (SQ slot size for GCQ #2) */
    fprintf(stderr, "[GCQ_SENSOR] Calling gcq_userspace_write...\n");
    result = gcq_userspace_write(ctx, (uint8_t *)&req, 512, timeout_ms);
    fprintf(stderr, "[GCQ_SENSOR] gcq_userspace_write returned: %u\n", result);
    if (result != GCQ_ERRORS_NONE) {
        fprintf(stderr, "[ERROR] GCQ write failed: %u\n", result);
        return -1;
    }

    /* Sensor command sent */
    fprintf(stderr, "[GCQ_SENSOR] Command sent successfully, waiting for response...\n");

    /* Wait for response with timeout */
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        resp_size = sizeof(resp);
        result = gcq_userspace_read(ctx, (uint8_t *)&resp, &resp_size, 0);

        if (result == GCQ_ERRORS_NONE) {
            fprintf(stderr, "[GCQ_SENSOR] Got response: cid=%d (expected %d) rcode=%d\n",
                    resp.hdr.cid, cid, resp.rcode);
            /* Got a response - check if it's for our command */
            if (resp.hdr.cid == cid) {
                /* Got sensor response */

                /* Check result code */
                if (resp.rcode != AMC_PROXY_RESULT_SUCCESS) {
                    fprintf(stderr, "[ERROR] AMC returned error: %u\n", resp.rcode);
                    return -1;
                }

                /* For now, just return a dummy value */
                /* TODO: Parse actual sensor data from shared memory */
                *value_out = 42000;  /* Dummy: 42 degrees/volts/amps/watts */

                fprintf(stderr, "[GCQ_SENSOR] SUCCESS! Returning value=%ld\n", *value_out);
                return 0;  /* Success */
            } else {
                /* Response for different CID, keep waiting */
                fprintf(stderr, "[GCQ_SENSOR] CID mismatch, continuing...\n");
            }
        }

        /* Check timeout */
        clock_gettime(CLOCK_MONOTONIC, &now);
        uint64_t elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 +
                              (now.tv_nsec - start.tv_nsec) / 1000000;

        if (elapsed_ms >= timeout_ms) {
            fprintf(stderr, "[ERROR] Timeout waiting for sensor response\n");
            return -1;
        }

        /* Small delay before retry */
        usleep(10000);  /* 10ms */
    }

    return -1;
}
