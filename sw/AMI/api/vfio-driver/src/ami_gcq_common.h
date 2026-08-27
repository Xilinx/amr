/**
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Common GCQ command structures for AMI userspace
 *
 * Defines AMC proxy command/response structures matching the kernel driver
 *
 * @file ami_gcq_common.h
 */

#ifndef _AMI_GCQ_COMMON_H_
#define _AMI_GCQ_COMMON_H_

#include <stdint.h>
#include <stdbool.h>
#include "../include/ami.h"  /* For ami_event_handler */

#ifndef MD5_SIZE
#define MD5_SIZE 16
#endif

/**
 * AMC Proxy Command Opcodes
 */
#define AMC_PROXY_CMD_OPCODE_BOOT_SEL           0x0  /* Boot partition select + trigger POR */
#define AMC_PROXY_CMD_OPCODE_IDENTIFY           0x202  /* AMI_CMD_OPCODE_IDENTIFY_REQ in firmware */
#define AMC_PROXY_CMD_OPCODE_HEARTBEAT          0x2
#define AMC_PROXY_CMD_OPCODE_EEPROM_READ_WRITE  0x3
#define AMC_PROXY_CMD_OPCODE_MODULE_READ_WRITE  0x4
#define AMC_PROXY_CMD_OPCODE_DEBUG_VERBOSITY    0x5
#define AMC_PROXY_CMD_OPCODE_FPT_FLAGS          0x6  /* FPT flags update (does NOT trigger reboot) */
#define AMC_PROXY_CMD_OPCODE_FPT_PARTITION      0x8
#define AMC_PROXY_CMD_OPCODE_PDI_DOWNLOAD       0xA  /* PDI download (flash programming) */
#define AMC_PROXY_CMD_OPCODE_PDI_PROGRAM        0xB  /* FW: separate program opcode (host uses 0xA + flags) */
#define AMC_PROXY_CMD_OPCODE_SENSOR             0xC
#define AMC_PROXY_CMD_OPCODE_PARTITION_COPY     0xD  /* Partition copy */
#define AMC_PROXY_CMD_OPCODE_DEVICE_BOOT        AMC_PROXY_CMD_OPCODE_BOOT_SEL  /* Alias */

/**
 * AMC Proxy Result Codes
 */
#define AMC_PROXY_RESULT_SUCCESS                    0
#define AMC_PROXY_RESULT_FAILURE                    1
#define AMC_PROXY_RESULT_INVALID_VALUE              2
#define AMC_PROXY_RESULT_GET_REQUEST_FAILED         3
#define AMC_PROXY_RESULT_PROCESS_REQUEST_FAILED     4
#define AMC_PROXY_RESULT_ALREADY_IN_PROGRESS        5
#define AMC_PROXY_RESULT_INVALID_CONFIGURATION      6

/**
 * Sensor Request Types
 */
#define AMC_PROXY_CMD_SENSOR_REQUEST_GET_SIZE       0x1
#define AMC_PROXY_CMD_SENSOR_REQUEST_GET_SDR        0x2
#define AMC_PROXY_CMD_SENSOR_REQUEST_GET_SINGLE_SDR 0x3
#define AMC_PROXY_CMD_SENSOR_REQUEST_ALL_SDR        0x4

/**
 * Sensor Repository Types
 */
#define AMC_PROXY_CMD_SENSOR_REPO_GET_SIZE      0
#define AMC_PROXY_CMD_SENSOR_REPO_BDINFO        1
#define AMC_PROXY_CMD_SENSOR_REPO_TEMP          2
#define AMC_PROXY_CMD_SENSOR_REPO_VOLTAGE       3
#define AMC_PROXY_CMD_SENSOR_REPO_CURRENT       4
#define AMC_PROXY_CMD_SENSOR_REPO_POWER         5
#define AMC_PROXY_CMD_SENSOR_REPO_TOTAL_POWER   6
#define AMC_PROXY_CMD_SENSOR_REPO_QSFP          7
#define AMC_PROXY_CMD_SENSOR_REPO_FPT           8
#define AMC_PROXY_CMD_SENSOR_REPO_ALL           9

/**
 * Read/Write Request Types
 */
#define AMC_PROXY_CMD_RW_REQUEST_READ   0
#define AMC_PROXY_CMD_RW_REQUEST_WRITE  1

/**
 * struct amc_proxy_cmd_request_hdr - GCQ submission queue entry header
 */
struct amc_proxy_cmd_request_hdr {
    uint32_t opcode:16;     /* [15:0] Command opcode */
    uint32_t count:15;      /* [30:16] Payload byte count */
    uint32_t state:1;       /* [31] New entry flag (set to 1) */
    uint16_t cid;           /* Command ID */
    uint16_t rsvd;          /* Reserved */
} __attribute__((packed));

/**
 * struct amc_proxy_cmd_response_hdr - GCQ completion queue entry header
 */
struct amc_proxy_cmd_response_hdr {
    uint16_t cid;           /* Command ID (matches request) */
    uint16_t cstate:14;     /* Command state */
    uint16_t specific:1;    /* Command-specific info in result */
    uint16_t state:1;       /* New entry flag */
} __attribute__((packed));

/**
 * struct amc_proxy_sensor_request_payload - Sensor command payload
 * CRITICAL: MUST match AMIProxyCmdReqSensorPayload in fw/AMC/src/proxy_drivers/ami/ami_proxy_driver.c
 */
struct amc_proxy_sensor_request_payload {
    uint64_t address;       /* ullAddress - FIRST! */
    uint32_t size;          /* ulSize */
    uint32_t offset;        /* ulOffset */
    uint32_t aid:8;         /* ulAID - bitfield */
    uint32_t sid:8;         /* ulSID - bitfield */
    uint32_t addr_type:3;   /* ulAddrType - bitfield */
    uint32_t sensor_id:8;   /* ulSensorId - bitfield */
    uint32_t reserved:5;    /* ulResvd - bitfield */
    uint32_t pad;           /* ulPad */
} __attribute__((packed));  /* Total: 24 bytes */

/**
 * struct amc_proxy_pdi_download_request_payload - PDI download / boot / copy payload
 *
 * CRITICAL: byte layout MUST match firmware AMI_CMD_DATA_PAYLOAD
 * (fw/AMC/src/proxy_drivers/ami/ami_proxy_driver.c) and the proven kernel
 * driver struct amc_proxy_cmd_data_payload. This struct is NOT packed so the
 * compiler inserts the same 2-byte pad after the {last_chunk,chunk} uint16
 * word, landing chunk_size at offset 24 (firmware reads ulPacketSize there).
 */
struct amc_proxy_pdi_download_request_payload {
    uint64_t address;                /* offset 0:  Shared memory address */
    uint32_t size;                   /* offset 8:  Chunk size in bytes */
    uint32_t remain_size;            /* offset 12: Remaining size */
    uint32_t src_partition:4;        /* offset 16 word: Source partition (copy) */
    uint32_t dest_partition:4;       /*            Destination partition (copy) */
    uint32_t partition_sel:4;        /*            Partition to flash/boot */
    uint32_t update_fpt:1;           /*            1 = image contains FPT */
    uint32_t program_pdi:1;          /*            1 = programming PDI */
    uint32_t boot_device:1;          /*            Boot device selection */
    uint32_t src_device:1;           /*            Source device (copy) */
    uint32_t dest_device:1;          /*            Dest device (copy) */
    uint32_t program_apu_pdi:1;      /*            1 = live APU PDI (reserved bit on this FW) */
    uint32_t program_rpu_pdi:1;      /*            1 = live RPU PDI (reserved bit on this FW) */
    uint32_t partition_resvd:13;     /*            Reserved */
    uint16_t last_chunk:1;           /* offset 20 word: 1 = last chunk */
    uint16_t chunk:15;               /*            Current chunk number */
    /* 2-byte natural-alignment pad here (offset 22) */
    uint32_t chunk_size;             /* offset 24: Packet size in KB (firmware ulPacketSize) */
    uint8_t  pdi_md5[MD5_SIZE];      /* offset 28: MD5 checksum of PDI file (16 bytes) */
    uint32_t pdi_size;               /* offset 44: Size of PDI file in bytes */
};

/**
 * struct amc_proxy_eeprom_rw_request_payload - EEPROM read/write payload
 * CRITICAL: Must match AMIProxyCmdEepromPayload in firmware
 */
struct amc_proxy_eeprom_rw_request_payload {
    uint64_t address;       /* Shared memory address - MUST BE FIRST! */
    uint32_t req_type:1;    /* 0=read, 1=write */
    uint32_t len:8;         /* Number of bytes */
    uint32_t offset:8;      /* EEPROM offset */
    uint32_t resvd:15;      /* Reserved */
} __attribute__((packed));

/**
 * struct amc_proxy_module_rw_request_payload - Module (QSFP) read/write payload
 */
struct amc_proxy_module_rw_request_payload {
    uint32_t req_type:1;    /* 0=read, 1=write */
    uint32_t resvd:31;      /* Reserved */
    uint64_t address;       /* Shared memory address */
    uint8_t  device_id;     /* Module device ID */
    uint8_t  page;          /* Page number */
    uint8_t  offset;        /* Byte offset within page */
    uint8_t  len;           /* Number of bytes */
} __attribute__((packed));

/**
 * struct amc_proxy_fpt_partition_payload - FPT partition read/write payload
 */
struct amc_proxy_fpt_partition_payload {
    uint32_t req_type;           /* 0 = Read, 1 = Write */
    uint32_t boot_device;        /* 0 = Primary, 1 = Secondary */
    uint32_t partition;          /* Partition index */
    uint32_t type;               /* Partition type */
    uint32_t base_addr;          /* Partition base address */
    uint32_t size;               /* Partition size */
    uint8_t  pdi_md5[MD5_SIZE];  /* MD5 checksum */
    uint32_t pdi_size;           /* PDI size */
    uint32_t flags;              /* Flags value */
} __attribute__((packed));

/**
 * struct amc_proxy_cmd_request - Complete GCQ submission queue entry
 *
 * This is the full 512-byte structure sent to the GCQ SQ (matches firmware AMI_PROXY_REQUEST_SIZE)
 */
struct amc_proxy_cmd_request {
    struct amc_proxy_cmd_request_hdr hdr;   /* 8 bytes */

    union {
        struct amc_proxy_sensor_request_payload sensor_payload;
        struct amc_proxy_pdi_download_request_payload pdi_payload;  /* PDI download / boot / copy */
        struct amc_proxy_eeprom_rw_request_payload eeprom_payload;
        struct amc_proxy_module_rw_request_payload module_payload;
        struct amc_proxy_fpt_partition_payload fpt_partition_payload;
        uint8_t raw_payload[504];            /* Pad to 512 bytes total (8 + 504) */
    };
} __attribute__((packed));

/**
 * struct com_queue_entry - GCQ completion queue entry
 *
 * This is the 16-byte structure received from the GCQ CQ.
 * CRITICAL: layout MUST match firmware AMI_CMD_RESPONSE
 * (fw/AMC/src/proxy_drivers/ami/ami_proxy_driver.c): the result code follows
 * the payload, it does NOT sit immediately after the header. Reading rcode at
 * the wrong offset makes commands that return a payload (e.g. IDENTIFY) look
 * like failures because the payload word is non-zero.
 */
struct com_queue_entry {
    struct amc_proxy_cmd_response_hdr hdr;  /* 4 bytes, offset 0 */
    uint32_t payload[2];                    /* offset 4: response payload (e.g. IDENTIFY version) */
    uint32_t rcode;                         /* offset 12: result code (fw ulRCode) */
} __attribute__((packed));

/* Forward declarations */
struct sdr_repo;
typedef struct gcq_metal_ctx_internal gcq_metal_ctx;

/*
 * Per-context pending GCQ response queue. Stored inside each gcq_metal_ctx so
 * that command IDs and out-of-order completions never leak between devices —
 * this is what makes concurrent per-device probing (e.g. parallel overview)
 * safe. Kept here (not in the opaque ctx) so ami_gcq_common.c can operate on it
 * via gcq_userspace_pending().
 */
#define GCQ_MAX_PENDING_RESPONSES 256
struct gcq_pending_queue {
    struct com_queue_entry responses[GCQ_MAX_PENDING_RESPONSES];
    int count;
};

/* Accessors into the opaque gcq_metal_ctx (implemented in gcq_userspace.c). */
struct gcq_pending_queue *gcq_userspace_pending(gcq_metal_ctx *ctx);
uint16_t *gcq_userspace_cid_counter(gcq_metal_ctx *ctx);
bool gcq_userspace_is_quick_open(gcq_metal_ctx *ctx);

/**
 * ami_gcq_get_sdr() - Fetch SDR (Sensor Data Records) from firmware
 * @ctx: GCQ context
 * @repo_type: Type of sensor repository to fetch (TEMP, VOLTAGE, etc.)
 * @repo_out: Output structure to store parsed SDR data
 *
 * Sends GET_SDR command to firmware and parses the response.
 * Response data is written by firmware to shared memory.
 *
 * Return: 0 on success, negative on error
 */
int ami_gcq_get_sdr(gcq_metal_ctx *ctx, uint8_t repo_type, struct sdr_repo *repo_out);

int ami_gcq_send_identify(gcq_metal_ctx *ctx,
                          uint8_t *amc_ver_major, uint8_t *amc_ver_minor, uint8_t *amc_ver_patch);

/* Helper functions */
uint16_t ami_gcq_allocate_cid(gcq_metal_ctx *ctx);
void ami_gcq_free_cid(uint16_t cid);
int ami_gcq_wait_response(gcq_metal_ctx *ctx, uint16_t expected_cid,
                          struct com_queue_entry *resp_out, uint32_t timeout_ms);
int ami_gcq_wait_response_pdi(gcq_metal_ctx *ctx, uint16_t expected_cid,
                              struct com_queue_entry *resp_out, uint32_t timeout_ms);
void ami_gcq_flush_pending_responses(gcq_metal_ctx *ctx);

/** Reset host-side GCQ session state (CID counter, pending queue) for a ctx. */
void ami_gcq_reset_session(gcq_metal_ctx *ctx);

/**
 * ami_gcq_purge_cq_completions() - Drop stale GCQ CQ entries before a PDI transfer
 * @ctx: GCQ context
 */
void ami_gcq_purge_cq_completions(gcq_metal_ctx *ctx);
void ami_gcq_drain_after_chunk(gcq_metal_ctx *ctx);

/**
 * ami_gcq_eeprom_read() - Read from EEPROM via GCQ
 * @ctx: GCQ context
 * @offset: EEPROM offset to read from
 * @len: Number of bytes to read
 * @data_out: Buffer to store read data
 *
 * Return: 0 on success, negative on error
 */
int ami_gcq_eeprom_read(gcq_metal_ctx *ctx, uint8_t offset, uint8_t len, uint8_t *data_out);

/**
 * ami_gcq_eeprom_write() - Write to EEPROM via GCQ
 * @ctx: GCQ context
 * @offset: EEPROM offset to write to
 * @len: Number of bytes to write
 * @data_in: Data to write
 *
 * Return: 0 on success, negative on error
 */
int ami_gcq_eeprom_write(gcq_metal_ctx *ctx, uint8_t offset, uint8_t len, uint8_t *data_in);

/**
 * ami_gcq_module_read() - Read from module (QSFP) via GCQ
 * @ctx: GCQ context
 * @device_id: Module device ID
 * @page: Page number
 * @offset: Byte offset within page
 * @len: Number of bytes to read
 * @data_out: Buffer to store read data
 *
 * Return: 0 on success, negative on error
 */
int ami_gcq_module_read(gcq_metal_ctx *ctx, uint8_t device_id, uint8_t page,
                        uint8_t offset, uint8_t len, uint8_t *data_out);

/**
 * ami_gcq_module_write() - Write to module (QSFP) via GCQ
 * @ctx: GCQ context
 * @device_id: Module device ID
 * @page: Page number
 * @offset: Byte offset within page
 * @len: Number of bytes to write
 * @data_in: Data to write
 *
 * Return: 0 on success, negative on error
 */
int ami_gcq_module_write(gcq_metal_ctx *ctx, uint8_t device_id, uint8_t page,
                         uint8_t offset, uint8_t len, uint8_t *data_in);

/**
 * ami_gcq_device_boot() - Trigger device boot via GCQ
 * @ctx: GCQ context
 * @boot_device: Boot device (primary=0, secondary=1)
 *
 * Return: 0 on success, negative on error
 */
int ami_gcq_device_boot(gcq_metal_ctx *ctx, uint32_t boot_device);

/**
 * ami_gcq_pdi_download_chunk() - Download PDI chunk via GCQ
 * @ctx: GCQ context
 * @chunk_data: Pointer to chunk data
 * @chunk_len: Length of this chunk
 * @chunk_num: Chunk number (0-based)
 * @chunk_size_kb: Chunk size in KB
 * @is_last_chunk: True if this is the last chunk
 * @boot_device: Target boot device
 * @partition: Target partition number
 * @pdi_md5: MD5 checksum of entire PDI file
 * @total_pdi_size: Total size of PDI file
 * @progress_cb: Optional progress callback
 * @cb_data: Data passed to progress callback
 *
 * Return: 0 on success, negative on error
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
                                void *cb_data);

/**
 * ami_gcq_pdi_program_chunk() - Live-load PDI chunk via GCQ (opcode 0xA + program flags)
 * @ctx: GCQ context
 * @chunk_data: Pointer to chunk data
 * @chunk_len: Length of this chunk
 * @chunk_num: Chunk number (0-based)
 * @chunk_size_kb: Chunk size in KB
 * @is_last_chunk: True if this is the last chunk
 * @boot_device: Target boot device
 * @partition: Target partition number (usually magic number for live-load type)
 * @pdi_md5: MD5 checksum of entire PDI file
 * @total_pdi_size: Total size of PDI file
 * @progress_cb: Optional progress callback
 * @cb_data: Data passed to progress callback
 *
 * Return: 0 on success, negative on error
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
                               void *cb_data);

/**
 * ami_gcq_partition_copy() - Copy partition via GCQ
 * @ctx: GCQ context
 * @src_device: Source boot device
 * @src_part: Source partition
 * @dest_device: Destination boot device
 * @dest_part: Destination partition
 *
 * Return: 0 on success, negative on error
 */
int ami_gcq_partition_copy(gcq_metal_ctx *ctx, uint32_t src_device, uint32_t src_part,
                            uint32_t dest_device, uint32_t dest_part, uint32_t src_partition_size);

/**
 * ami_gcq_fpt_partition() - FPT partition operation via GCQ
 * @ctx: GCQ context
 * @boot_device: Target boot device
 *
 * Return: 0 on success, negative on error
 */
int ami_gcq_fpt_partition(gcq_metal_ctx *ctx, uint32_t boot_device);

#endif /* _AMI_GCQ_COMMON_H_ */
