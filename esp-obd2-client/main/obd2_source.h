#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "obd2_types.h"

esp_err_t obd2_source_init(void);
esp_err_t obd2_source_read_fast(obd_snapshot_t *snapshot);
esp_err_t obd2_source_read_slow(obd_snapshot_t *snapshot);
esp_err_t obd2_source_read_extended(obd_snapshot_t *snapshot);
esp_err_t obd2_source_read_dtc(obd_dtc_report_t *report);
uint32_t obd2_source_supported_mask(void);
const char *obd2_source_mode(void);
uint32_t obd2_source_success_count(void);
uint32_t obd2_source_error_count(void);
