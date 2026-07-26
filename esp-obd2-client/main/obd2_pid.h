#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t obd2_decode_mode01(uint8_t pid, const uint8_t *data, size_t length,
                            float *value);
const char *obd2_pid_name(uint8_t pid);
const char *obd2_pid_unit(uint8_t pid);
