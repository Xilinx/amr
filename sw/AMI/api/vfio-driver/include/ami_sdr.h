/**
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * SDR (Sensor Data Record) structures and functions for userspace
 *
 * @file ami_sdr.h
 */

#ifndef _AMI_SDR_H_
#define _AMI_SDR_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SDR constants from old driver */
#define SDR_VALUE_MAX_LEN 128
#define SDR_THRESHOLD_MAX_LEN 4
#define SDR_TYPE_POS 4
#define SDR_LENGTH_MASK 0x0F
#define SDR_EOR 0x454F52  /* "EOR" marker */
#define SDR_END 0x454E44  /* "END" marker - alternative ending used by ASDM */
#define SDR_RESP_LEN (64 * 1024)  /* 64KB response buffer */

/* Threshold support bitmask */
#define THRESHOLD_LOWER_FATAL_MASK    0x01
#define THRESHOLD_LOWER_CRITICAL_MASK 0x02
#define THRESHOLD_LOWER_WARNING_MASK  0x04
#define THRESHOLD_UPPER_FATAL_MASK    0x08
#define THRESHOLD_UPPER_CRITICAL_MASK 0x10
#define THRESHOLD_UPPER_WARNING_MASK  0x20
#define THRESHOLD_SENSOR_AVG_MASK     0x40
#define THRESHOLD_SENSOR_MAX_MASK     0x80

/* SDR repo types (from amc_proxy.h) */
enum sdr_repo_type {
    SDR_TYPE_UNKNOWN = 0,
    SDR_TYPE_BDINFO = 1,
    SDR_TYPE_TEMP = 2,
    SDR_TYPE_VOLTAGE = 3,
    SDR_TYPE_CURRENT = 4,
    SDR_TYPE_POWER = 5,
    SDR_TYPE_TOTAL_POWER = 6,
    SDR_TYPE_QSFP = 7,
    SDR_TYPE_FPT = 8,
};

/* SDR value types */
enum sdr_value_type {
    SDR_VALUE_TYPE_BINARY = 0,
    SDR_VALUE_TYPE_ASCII = 1,
};

/* Sensor status values */
enum sensor_status {
    SENSOR_NOT_PRESENT = 0,
    SENSOR_PRESENT_AND_VALID = 1,
    DATA_NOT_AVAILABLE = 2,
    SENSOR_STATUS_NOT_AVAILABLE = 3,
};

/**
 * struct sdr_record - A single sensor data record
 */
struct sdr_record {
    uint8_t id;
    uint8_t name_type;
    uint8_t name_len;
    uint8_t name[SDR_VALUE_MAX_LEN];
    uint8_t value_type;
    uint8_t value_len;
    uint8_t value[SDR_VALUE_MAX_LEN];
    uint8_t unit_type;
    uint8_t unit_len;
    uint8_t unit[SDR_VALUE_MAX_LEN];
    char    unit_mod;
    uint8_t threshold_support;
    uint8_t lower_fatal_limit[SDR_THRESHOLD_MAX_LEN];
    uint8_t lower_crit_limit[SDR_THRESHOLD_MAX_LEN];
    uint8_t lower_warn_limit[SDR_THRESHOLD_MAX_LEN];
    uint8_t upper_warn_limit[SDR_THRESHOLD_MAX_LEN];
    uint8_t upper_crit_limit[SDR_THRESHOLD_MAX_LEN];
    uint8_t upper_fatal_limit[SDR_THRESHOLD_MAX_LEN];
    uint8_t sensor_status;
    uint8_t min[SDR_THRESHOLD_MAX_LEN];
    uint8_t max[SDR_THRESHOLD_MAX_LEN];
    uint8_t avg[SDR_THRESHOLD_MAX_LEN];
};

/**
 * struct fpt_header - Flash Partition Table header
 */
struct fpt_header {
    uint8_t version;
    uint8_t header_size;
    uint8_t entry_size;
    uint8_t num_entries;
};

/**
 * struct fpt_partition_entry - Flash Partition Table entry
 */
struct fpt_partition_entry {
    uint16_t type;           /* Partition type */
    uint32_t base_addr;      /* Base address in flash */
    uint32_t size;           /* Partition size */
    uint8_t  md5[16];        /* MD5 checksum */
    uint32_t pdi_size;       /* PDI size */
    uint32_t flags;          /* Partition flags */
} __attribute__((packed));

/**
 * struct field_info - A field in the board info
 */
struct field_info {
    uint8_t type;
    uint8_t len;
    uint8_t bytes[SDR_VALUE_MAX_LEN];
};

/**
 * struct bd_info_record - The board info record (manufacturing data)
 */
struct bd_info_record {
    struct field_info eeprom_version;
    struct field_info product_name;
    struct field_info board_rev;
    struct field_info board_serial;
    struct field_info mac_addr_count;
    struct field_info first_mac_addr;
    struct field_info active_state;
    struct field_info config_mode;
    struct field_info manufacturing_date;
    struct field_info part_number;
    struct field_info uuid;
    struct field_info pcie_id;
    struct field_info power_mode;
    struct field_info memory_size;
    struct field_info oem_id;
    struct field_info capability;
    struct field_info mfg_part_number;
};

/**
 * struct sdr_repo - A repository of SDR records
 */
struct sdr_repo {
    uint8_t repo_type;
    uint8_t repo_ver;
    uint8_t num_records;
    uint16_t size;

    /* Union for different repo types */
    union {
        struct sdr_record *records;  /* For sensor repos (TEMP, VOLTAGE, CURRENT, POWER) */

        /* Board info (when repo_type == SDR_TYPE_BDINFO) */
        struct bd_info_record *bd_info;

        /* FPT-specific data (when repo_type == SDR_TYPE_FPT) */
        struct {
            struct fpt_header hdr_primary;
            struct fpt_partition_entry *partitions_primary;
            struct fpt_header hdr_secondary;
            struct fpt_partition_entry *partitions_secondary;
        } fpt;
    };
};

/**
 * Parse SDR data from firmware response
 * @param sdr_buf Buffer containing SDR data from firmware
 * @param repo Output structure to store parsed records
 * @return 0 on success, negative on error
 */
int ami_parse_sdr(uint8_t *sdr_buf, struct sdr_repo *repo);

/**
 * Free SDR repository memory
 * @param repo Repository to free
 */
void ami_free_sdr_repo(struct sdr_repo *repo);

/**
 * Find an SDR record by ID in a repository
 * @param repo Repository to search
 * @param sensor_id ID to find
 * @return Pointer to record or NULL if not found
 */
struct sdr_record *ami_find_sdr_record(struct sdr_repo *repo, uint8_t sensor_id);

/**
 * Convert binary value to long
 * @param type Value type
 * @param size Value size in bytes
 * @param data Pointer to value data
 * @return Converted value
 */
long ami_make_val(enum sdr_value_type type, uint8_t size, uint8_t *data);

/**
 * Get repo type name
 * @param repo_type Repo type enum
 * @return Human-readable string
 */
const char *ami_sdr_repo_type_to_str(enum sdr_repo_type repo_type);

#ifdef __cplusplus
}
#endif

#endif /* _AMI_SDR_H_ */
