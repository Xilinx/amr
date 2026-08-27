/**
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * SDR (Sensor Data Record) parsing implementation for userspace
 * Based on ami_sensor.c from kernel driver
 *
 * @file ami_sdr.c
 */

#include "ami_sdr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *ami_sdr_repo_type_to_str(enum sdr_repo_type sdr)
{
    switch (sdr) {
        case SDR_TYPE_BDINFO:      return "board info";
        case SDR_TYPE_TEMP:        return "temperature";
        case SDR_TYPE_VOLTAGE:     return "voltage";
        case SDR_TYPE_CURRENT:     return "current";
        case SDR_TYPE_POWER:       return "power";
        case SDR_TYPE_TOTAL_POWER: return "total power";
        case SDR_TYPE_QSFP:        return "qsfp";
        case SDR_TYPE_FPT:         return "fpt";
        default:                   return "unknown";
    }
}

long ami_make_val(enum sdr_value_type type, uint8_t size, uint8_t *data)
{
    if ((type != SDR_VALUE_TYPE_BINARY) || !data)
        return 0;

    switch (size) {
        case 1: return (long)(*(uint8_t *)data);
        case 2: return (long)(*(uint16_t *)data);
        case 4: return (long)(*(uint32_t *)data);
        default: return 0;
    }
}

int ami_parse_sdr(uint8_t *sdr_buf, struct sdr_repo *repo)
{
    int i = 0;
    int buf_index = 0;
    struct sdr_record *rec = NULL;

    if (!sdr_buf || !repo)
        return -1;

    memset(repo, 0, sizeof(*repo));

    /* Parse SDR header (5 bytes) */
    repo->repo_type = sdr_buf[buf_index++];
    repo->repo_ver = sdr_buf[buf_index++];
    repo->num_records = sdr_buf[buf_index++];
    repo->size = sdr_buf[buf_index] | (sdr_buf[buf_index + 1] << 8);
    buf_index += 2;


    if (!repo->size)
        return -1;

    /* Handle empty SDR (just header + EoR) */
    if (repo->num_records == 0) {
        return -2;  /* ENODATA */
    }

    /* Special handling for BDINFO type - fixed field order, no IDs
     * ASDM uses 0xC0 for BDINFO, AMI proxy uses 1 */
    if (repo->repo_type == SDR_TYPE_BDINFO || repo->repo_type == 0xC0) {
        /* Allocate board info structure */
        repo->bd_info = (struct bd_info_record *)calloc(1, sizeof(struct bd_info_record));
        if (!repo->bd_info)
            return -1;

        /* Fields in fixed order: type(1) + len(1) + data(len) */
        struct field_info *fields[] = {
            &repo->bd_info->eeprom_version,
            &repo->bd_info->product_name,
            &repo->bd_info->board_rev,
            &repo->bd_info->board_serial,
            &repo->bd_info->mac_addr_count,
            &repo->bd_info->first_mac_addr,
            &repo->bd_info->active_state,
            &repo->bd_info->config_mode,
            &repo->bd_info->manufacturing_date,
            &repo->bd_info->part_number,
            &repo->bd_info->uuid,
            &repo->bd_info->pcie_id,
            &repo->bd_info->power_mode,
            &repo->bd_info->memory_size,
            &repo->bd_info->oem_id,
            &repo->bd_info->capability,
            &repo->bd_info->mfg_part_number
        };

        for (i = 0; i < 17; i++) {
            fields[i]->type = sdr_buf[buf_index++];
            fields[i]->len = sdr_buf[buf_index++];
            if (fields[i]->len > 0 && fields[i]->len <= SDR_VALUE_MAX_LEN) {
                memcpy(fields[i]->bytes, &sdr_buf[buf_index], fields[i]->len);
                buf_index += fields[i]->len;
            }
        }

        return 0;
    }

    /* Special handling for FPT type - firmware uses 0xF0, we define as 8 */
    if (repo->repo_type == SDR_TYPE_FPT || repo->repo_type == 0xF0) {
        /* Parse primary FPT header */
        repo->fpt.hdr_primary.version = sdr_buf[buf_index++];
        repo->fpt.hdr_primary.header_size = sdr_buf[buf_index++];
        repo->fpt.hdr_primary.entry_size = sdr_buf[buf_index++];
        repo->fpt.hdr_primary.num_entries = sdr_buf[buf_index++];

        /* Parse primary partition entries */
        if (repo->fpt.hdr_primary.num_entries > 0) {
            repo->fpt.partitions_primary = (struct fpt_partition_entry *)calloc(
                repo->fpt.hdr_primary.num_entries, sizeof(struct fpt_partition_entry));
            if (repo->fpt.partitions_primary) {
                for (int i = 0; i < repo->fpt.hdr_primary.num_entries; i++) {
                    /* Parse entry: type(2) + reserved(2) + base_addr(4) + size(4) + md5(16) + pdi_size(4) + flags(4) = 36 bytes */
                    /* Data is in little-endian format */

                    /* Type (2 bytes) */
                    repo->fpt.partitions_primary[i].type = *(uint16_t *)&sdr_buf[buf_index];
                    buf_index += 2;

                    /* Skip 2 reserved bytes */
                    buf_index += 2;

                    /* Base address (4 bytes LE) */
                    repo->fpt.partitions_primary[i].base_addr = *(uint32_t *)&sdr_buf[buf_index];
                    buf_index += 4;

                    /* Size (4 bytes LE) */
                    repo->fpt.partitions_primary[i].size = *(uint32_t *)&sdr_buf[buf_index];
                    buf_index += 4;

                    /* MD5 (16 bytes) */
                    memcpy(repo->fpt.partitions_primary[i].md5, &sdr_buf[buf_index], 16);
                    buf_index += 16;

                    /* PDI size (4 bytes LE) */
                    repo->fpt.partitions_primary[i].pdi_size = *(uint32_t *)&sdr_buf[buf_index];
                    buf_index += 4;

                    /* Flags (4 bytes LE) */
                    repo->fpt.partitions_primary[i].flags = *(uint32_t *)&sdr_buf[buf_index];
                    buf_index += 4;
                }
            }
        } else {
            repo->fpt.partitions_primary = NULL;
        }

        /* Parse secondary FPT header */
        repo->fpt.hdr_secondary.version = sdr_buf[buf_index++];
        repo->fpt.hdr_secondary.header_size = sdr_buf[buf_index++];
        repo->fpt.hdr_secondary.entry_size = sdr_buf[buf_index++];
        repo->fpt.hdr_secondary.num_entries = sdr_buf[buf_index++];

        /* Parse secondary partition entries */
        if (repo->fpt.hdr_secondary.num_entries > 0) {
            repo->fpt.partitions_secondary = (struct fpt_partition_entry *)calloc(
                repo->fpt.hdr_secondary.num_entries, sizeof(struct fpt_partition_entry));
            if (repo->fpt.partitions_secondary) {
                for (int i = 0; i < repo->fpt.hdr_secondary.num_entries; i++) {
                    /* Parse entry: type(2) + reserved(2) + base_addr(4) + size(4) + md5(16) + pdi_size(4) + flags(4) = 36 bytes */
                    repo->fpt.partitions_secondary[i].type = *(uint16_t *)&sdr_buf[buf_index];
                    buf_index += 2;

                    buf_index += 2;  /* Skip reserved */

                    repo->fpt.partitions_secondary[i].base_addr = *(uint32_t *)&sdr_buf[buf_index];
                    buf_index += 4;

                    repo->fpt.partitions_secondary[i].size = *(uint32_t *)&sdr_buf[buf_index];
                    buf_index += 4;

                    memcpy(repo->fpt.partitions_secondary[i].md5, &sdr_buf[buf_index], 16);
                    buf_index += 16;

                    repo->fpt.partitions_secondary[i].pdi_size = *(uint32_t *)&sdr_buf[buf_index];
                    buf_index += 4;

                    repo->fpt.partitions_secondary[i].flags = *(uint32_t *)&sdr_buf[buf_index];
                    buf_index += 4;
                }
            }
        } else {
            repo->fpt.partitions_secondary = NULL;
        }

        return 0;
    }

    /* Handle sensor types - ASDM uses different values than AMI proxy!
     * ASDM: TEMP=0xC1, VOLTAGE=0xC2, CURRENT=0xC3, POWER=0xC4, TOTAL_POWER=0xC6
     * Old AMI: TEMP=2, VOLTAGE=3, CURRENT=4, POWER=5, TOTAL_POWER=6
     * BDINFO is repo_type=1, skip sensor validation for it
     */
    if (repo->repo_type != SDR_TYPE_BDINFO &&
        repo->repo_type != 0xC1 && repo->repo_type != 0xC2 &&
        repo->repo_type != 0xC3 && repo->repo_type != 0xC4 &&
        repo->repo_type != 0xC6 &&
        repo->repo_type < SDR_TYPE_TEMP && repo->repo_type > SDR_TYPE_TOTAL_POWER) {
        return -1;
    }

    /* Allocate records array */
    repo->records = (struct sdr_record *)calloc(repo->num_records, sizeof(struct sdr_record));
    if (!repo->records)
        return -1;

    /* Parse each record */
    for (i = 0; i < repo->num_records; i++) {
        rec = &repo->records[i];

        /* ID */
        rec->id = sdr_buf[buf_index++];

        /* Name */
        rec->name_type = sdr_buf[buf_index] >> SDR_TYPE_POS;
        rec->name_len = sdr_buf[buf_index] & SDR_LENGTH_MASK;
        buf_index++;
        if (rec->name_len > 0 && rec->name_len < SDR_VALUE_MAX_LEN) {
            memcpy(rec->name, &sdr_buf[buf_index], rec->name_len);
            buf_index += rec->name_len;
        }

        /* Value */
        rec->value_type = sdr_buf[buf_index] >> SDR_TYPE_POS;
        rec->value_len = sdr_buf[buf_index] & SDR_LENGTH_MASK;
        buf_index++;
        if (rec->value_len > 0 && rec->value_len <= SDR_VALUE_MAX_LEN) {
            memcpy(rec->value, &sdr_buf[buf_index], rec->value_len);
            buf_index += rec->value_len;
        }

        /* Unit */
        rec->unit_type = sdr_buf[buf_index] >> SDR_TYPE_POS;
        rec->unit_len = sdr_buf[buf_index] & SDR_LENGTH_MASK;
        buf_index++;
        if (rec->unit_len > 0 && rec->unit_len <= SDR_VALUE_MAX_LEN) {
            memcpy(rec->unit, &sdr_buf[buf_index], rec->unit_len);
            buf_index += rec->unit_len;
        }

        rec->unit_mod = sdr_buf[buf_index++];
        rec->threshold_support = sdr_buf[buf_index++];

        /* Lower fatal */
        if (rec->threshold_support & THRESHOLD_LOWER_FATAL_MASK) {
            memcpy(rec->lower_fatal_limit, &sdr_buf[buf_index], rec->value_len);
            buf_index += rec->value_len;
        }

        /* Lower critical */
        if (rec->threshold_support & THRESHOLD_LOWER_CRITICAL_MASK) {
            memcpy(rec->lower_crit_limit, &sdr_buf[buf_index], rec->value_len);
            buf_index += rec->value_len;
        }

        /* Lower warning */
        if (rec->threshold_support & THRESHOLD_LOWER_WARNING_MASK) {
            memcpy(rec->lower_warn_limit, &sdr_buf[buf_index], rec->value_len);
            buf_index += rec->value_len;
        }

        /* Upper fatal */
        if (rec->threshold_support & THRESHOLD_UPPER_FATAL_MASK) {
            memcpy(rec->upper_fatal_limit, &sdr_buf[buf_index], rec->value_len);
            buf_index += rec->value_len;
        }

        /* Upper critical */
        if (rec->threshold_support & THRESHOLD_UPPER_CRITICAL_MASK) {
            memcpy(rec->upper_crit_limit, &sdr_buf[buf_index], rec->value_len);
            buf_index += rec->value_len;
        }

        /* Upper warning */
        if (rec->threshold_support & THRESHOLD_UPPER_WARNING_MASK) {
            memcpy(rec->upper_warn_limit, &sdr_buf[buf_index], rec->value_len);
            buf_index += rec->value_len;
        }

        rec->sensor_status = sdr_buf[buf_index++];

        /* Average */
        if (rec->threshold_support & THRESHOLD_SENSOR_AVG_MASK) {
            memcpy(rec->avg, &sdr_buf[buf_index], rec->value_len);
            buf_index += rec->value_len;
        }

        /* Max */
        if (rec->threshold_support & THRESHOLD_SENSOR_MAX_MASK) {
            memcpy(rec->max, &sdr_buf[buf_index], rec->value_len);
            buf_index += rec->value_len;
        }

    }

    /* Verify End of Repo (EoR) marker for sensor types - ASDM uses "END" (0x454E44) not "EOR" (0x454F52)
     * BDINFO doesn't have end marker, so skip validation for it (0xC0 or 1)
     * FPT (type 8 or 0xF0) also doesn't have end marker, skip validation */
    if (repo->repo_type != SDR_TYPE_BDINFO && repo->repo_type != 0xC0 &&
        repo->repo_type != SDR_TYPE_FPT && repo->repo_type != 0xF0) {
        uint32_t eor_marker = (sdr_buf[buf_index] << 16) | (sdr_buf[buf_index + 1] << 8) | sdr_buf[buf_index + 2];
        if (eor_marker != SDR_EOR && eor_marker != SDR_END) {
            /* Invalid marker - skip this repo silently, don't fail */
            ami_free_sdr_repo(repo);
            return -1;
        }
    }

    return 0;
}

void ami_free_sdr_repo(struct sdr_repo *repo)
{
    if (!repo)
        return;

    if ((repo->repo_type == SDR_TYPE_BDINFO || repo->repo_type == 0xC0) && repo->bd_info) {
        free(repo->bd_info);
        repo->bd_info = NULL;
    } else if ((repo->repo_type == SDR_TYPE_FPT || repo->repo_type == 0xF0)) {
        /* Free FPT partition arrays */
        if (repo->fpt.partitions_primary) {
            free(repo->fpt.partitions_primary);
            repo->fpt.partitions_primary = NULL;
        }
        if (repo->fpt.partitions_secondary) {
            free(repo->fpt.partitions_secondary);
            repo->fpt.partitions_secondary = NULL;
        }
    } else if (repo->records) {
        free(repo->records);
        repo->records = NULL;
    }

    repo->num_records = 0;
}

struct sdr_record *ami_find_sdr_record(struct sdr_repo *repo, uint8_t sensor_id)
{
    if (!repo || !repo->records)
        return NULL;

    for (int i = 0; i < repo->num_records; i++) {
        if (repo->records[i].id == sensor_id)
            return &repo->records[i];
    }

    return NULL;
}
