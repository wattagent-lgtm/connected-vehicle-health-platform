#include "obd2_source.h"

#include <math.h>
#include <string.h>
#include "driver/twai.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "obd2_pid.h"
#include "sdkconfig.h"

static const char *TAG = "obd2_source";
static uint32_t s_supported_mask;
static uint32_t s_success_count;
static uint32_t s_error_count;

#if CONFIG_OBD_SIMULATOR

static const double TWO_PI = 6.28318530717958647692;

static float wave(float center, float amplitude, float period_s)
{
    const double t = esp_timer_get_time() / 1000000.0;
    return center + amplitude * sin((TWO_PI * t) / period_s);
}

esp_err_t obd2_source_init(void)
{
    s_supported_mask = OBD_VALID_RPM | OBD_VALID_SPEED | OBD_VALID_LOAD |
        OBD_VALID_THROTTLE | OBD_VALID_COOLANT | OBD_VALID_INTAKE |
        OBD_VALID_FUEL | OBD_VALID_VOLTAGE | OBD_VALID_RUNTIME |
        OBD_VALID_AMBIENT | OBD_VALID_MIL;
    s_supported_mask |= OBD_VALID_STFT_B1 | OBD_VALID_LTFT_B1 |
        OBD_VALID_MAP | OBD_VALID_MAF | OBD_VALID_FUEL_SYSTEM |
        OBD_VALID_OBD_STANDARD | OBD_VALID_DIST_MIL |
        OBD_VALID_DIST_CLEAR | OBD_VALID_FUEL_TYPE | OBD_VALID_FUEL_RATE;
    ESP_LOGW(TAG, "SIMULATOR mode; no vehicle CAN traffic is generated");
    return ESP_OK;
}

esp_err_t obd2_source_read_fast(obd_snapshot_t *s)
{
    if (s == NULL) return ESP_ERR_INVALID_ARG;
    s->engine_rpm = wave(1800.0f, 650.0f, 12.0f);
    s->vehicle_speed_kph = wave(62.0f, 18.0f, 20.0f);
    s->engine_load_pct = wave(38.0f, 12.0f, 9.0f);
    s->throttle_pct = wave(24.0f, 10.0f, 7.0f);
    s->valid_mask |= OBD_VALID_RPM | OBD_VALID_SPEED |
        OBD_VALID_LOAD | OBD_VALID_THROTTLE;
    s->sampled_at_ms = esp_timer_get_time() / 1000;
    return ESP_OK;
}

esp_err_t obd2_source_read_slow(obd_snapshot_t *s)
{
    if (s == NULL) return ESP_ERR_INVALID_ARG;
    s->coolant_c = wave(88.0f, 3.0f, 60.0f);
    s->intake_air_c = wave(34.0f, 2.0f, 45.0f);
    s->fuel_level_pct = wave(68.0f, 0.2f, 300.0f);
    s->control_module_voltage_v = wave(14.1f, 0.15f, 30.0f);
    s->engine_runtime_s = esp_timer_get_time() / 1000000;
    s->ambient_air_c = wave(32.0f, 1.0f, 120.0f);
    s->mil_on = false;
    s->dtc_count = 0;
    s->valid_mask |= OBD_VALID_COOLANT | OBD_VALID_INTAKE | OBD_VALID_FUEL |
        OBD_VALID_VOLTAGE | OBD_VALID_RUNTIME | OBD_VALID_AMBIENT |
        OBD_VALID_MIL;
    s->sampled_at_ms = esp_timer_get_time() / 1000;
    return ESP_OK;
}

esp_err_t obd2_source_read_extended(obd_snapshot_t *s)
{
    if (s == NULL) return ESP_ERR_INVALID_ARG;
    s->short_fuel_trim_b1_pct = wave(1.0f, 3.0f, 11.0f);
    s->long_fuel_trim_b1_pct = wave(2.0f, 1.0f, 80.0f);
    s->intake_manifold_kpa = wave(45.0f, 12.0f, 9.0f);
    s->maf_g_s = wave(9.0f, 3.0f, 8.0f);
    s->distance_mil_km = 0;
    s->distance_since_clear_km = 1250;
    s->engine_fuel_rate_l_h = wave(2.1f, 0.8f, 10.0f);
    s->fuel_system_status = 2;
    s->obd_standard = 6;
    s->fuel_type = 1;
    s->valid_mask |= OBD_VALID_STFT_B1 | OBD_VALID_LTFT_B1 |
        OBD_VALID_MAP | OBD_VALID_MAF | OBD_VALID_FUEL_SYSTEM |
        OBD_VALID_OBD_STANDARD | OBD_VALID_DIST_MIL |
        OBD_VALID_DIST_CLEAR | OBD_VALID_FUEL_TYPE | OBD_VALID_FUEL_RATE;
    s->sampled_at_ms = esp_timer_get_time() / 1000;
    return ESP_OK;
}

esp_err_t obd2_source_read_dtc(obd_dtc_report_t *r)
{
    if (r == NULL) return ESP_ERR_INVALID_ARG;
    memset(r, 0, sizeof(*r));
    r->sampled_at_ms = esp_timer_get_time() / 1000;
    return ESP_OK;
}

#else

static esp_err_t query_pid(uint8_t pid, float *value)
{
    const twai_message_t request = {
        .identifier = 0x7DF,
        .data_length_code = 8,
        .data = {0x02, 0x01, pid, 0, 0, 0, 0, 0}
    };
    esp_err_t tx = twai_transmit(&request, pdMS_TO_TICKS(100));
    if (tx != ESP_OK) {
        ++s_error_count;
        ESP_LOGW(TAG, "PID 0x%02X TX failed: %s", pid, esp_err_to_name(tx));
        return tx;
    }

    const int64_t deadline = esp_timer_get_time() + 250000;
    while (esp_timer_get_time() < deadline) {
        twai_message_t response;
        if (twai_receive(&response, pdMS_TO_TICKS(20)) != ESP_OK) continue;
        if (response.identifier < 0x7E8 || response.identifier > 0x7EF) continue;
        if (response.data_length_code < 4) continue;
        if (response.data[1] != 0x41 || response.data[2] != pid) continue;
        esp_err_t decoded = obd2_decode_mode01(pid, &response.data[3],
                                               response.data_length_code - 3, value);
        if (decoded == ESP_OK) ++s_success_count; else ++s_error_count;
        return decoded;
    }
    ++s_error_count;
    return ESP_ERR_TIMEOUT;
}

static bool supported(uint32_t bit)
{
    return (s_supported_mask & bit) != 0;
}

static void read_one(obd_snapshot_t *s, uint8_t pid, float *target,
                     uint32_t valid_bit)
{
    if (supported(valid_bit) && query_pid(pid, target) == ESP_OK)
        s->valid_mask |= valid_bit;
}

static esp_err_t discover_supported_pids(void)
{
    const struct { uint8_t pid; uint32_t bit; } pids[] = {
        {0x03, OBD_VALID_FUEL_SYSTEM},
        {0x0C, OBD_VALID_RPM}, {0x0D, OBD_VALID_SPEED},
        {0x04, OBD_VALID_LOAD}, {0x11, OBD_VALID_THROTTLE},
        {0x05, OBD_VALID_COOLANT}, {0x0F, OBD_VALID_INTAKE},
        {0x06, OBD_VALID_STFT_B1}, {0x07, OBD_VALID_LTFT_B1},
        {0x0B, OBD_VALID_MAP}, {0x10, OBD_VALID_MAF},
        {0x1C, OBD_VALID_OBD_STANDARD}, {0x21, OBD_VALID_DIST_MIL},
        {0x1F, OBD_VALID_RUNTIME}, {0x2F, OBD_VALID_FUEL},
        {0x31, OBD_VALID_DIST_CLEAR}, {0x42, OBD_VALID_VOLTAGE},
        {0x46, OBD_VALID_AMBIENT}, {0x51, OBD_VALID_FUEL_TYPE},
        {0x5E, OBD_VALID_FUEL_RATE}
    };
    s_supported_mask = OBD_VALID_MIL;
    const uint8_t bases[] = {0x00, 0x20, 0x40};
    for (size_t b = 0; b < sizeof(bases); ++b) {
        const uint8_t base = bases[b];
        const twai_message_t request = {
            .identifier = 0x7DF, .data_length_code = 8,
            .data = {0x02, 0x01, base, 0, 0, 0, 0, 0}
        };
        if (twai_transmit(&request, pdMS_TO_TICKS(100)) != ESP_OK) continue;
        const int64_t deadline = esp_timer_get_time() + 300000;
        while (esp_timer_get_time() < deadline) {
            twai_message_t r;
            if (twai_receive(&r, pdMS_TO_TICKS(20)) != ESP_OK) continue;
            if (r.identifier < 0x7E8 || r.identifier > 0x7EF ||
                r.data_length_code < 7 || r.data[1] != 0x41 ||
                r.data[2] != base) continue;
            const uint32_t bitmap = ((uint32_t)r.data[3] << 24) |
                ((uint32_t)r.data[4] << 16) |
                ((uint32_t)r.data[5] << 8) | r.data[6];
            for (size_t i = 0; i < sizeof(pids) / sizeof(pids[0]); ++i) {
                if (pids[i].pid <= base || pids[i].pid > base + 32) continue;
                const uint8_t offset = pids[i].pid - base;
                if (bitmap & (1UL << (32 - offset)))
                    s_supported_mask |= pids[i].bit;
            }
            break;
        }
    }
    ESP_LOGI(TAG, "Validated PID mask: 0x%08lx",
             (unsigned long)s_supported_mask);
    return s_supported_mask != OBD_VALID_MIL ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t obd2_source_init(void)
{
    const twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
        CONFIG_OBD_CAN_TX_GPIO, CONFIG_OBD_CAN_RX_GPIO, TWAI_MODE_NORMAL);
#if CONFIG_OBD_CAN_250K
    const twai_timing_config_t timing = TWAI_TIMING_CONFIG_250KBITS();
#else
    const twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
#endif
    const twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    ESP_RETURN_ON_ERROR(twai_driver_install(&general, &timing, &filter), TAG,
                        "TWAI install failed");
    ESP_RETURN_ON_ERROR(twai_start(), TAG, "TWAI start failed");
    discover_supported_pids();
    ESP_LOGW(TAG, "CAN mode is diagnostic Mode 01 only; TX=%d RX=%d",
             CONFIG_OBD_CAN_TX_GPIO, CONFIG_OBD_CAN_RX_GPIO);
    return ESP_OK;
}

esp_err_t obd2_source_read_fast(obd_snapshot_t *s)
{
    if (s == NULL) return ESP_ERR_INVALID_ARG;
    read_one(s, 0x0C, &s->engine_rpm, OBD_VALID_RPM);
    read_one(s, 0x0D, &s->vehicle_speed_kph, OBD_VALID_SPEED);
    read_one(s, 0x04, &s->engine_load_pct, OBD_VALID_LOAD);
    read_one(s, 0x11, &s->throttle_pct, OBD_VALID_THROTTLE);
    s->sampled_at_ms = esp_timer_get_time() / 1000;
    return s->valid_mask ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t obd2_source_read_slow(obd_snapshot_t *s)
{
    if (s == NULL) return ESP_ERR_INVALID_ARG;
    read_one(s, 0x05, &s->coolant_c, OBD_VALID_COOLANT);
    read_one(s, 0x0F, &s->intake_air_c, OBD_VALID_INTAKE);
    read_one(s, 0x2F, &s->fuel_level_pct, OBD_VALID_FUEL);
    read_one(s, 0x42, &s->control_module_voltage_v, OBD_VALID_VOLTAGE);
    read_one(s, 0x1F, &s->engine_runtime_s, OBD_VALID_RUNTIME);
    read_one(s, 0x46, &s->ambient_air_c, OBD_VALID_AMBIENT);
    s->sampled_at_ms = esp_timer_get_time() / 1000;
    return s->valid_mask ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t obd2_source_read_extended(obd_snapshot_t *s)
{
    if (s == NULL) return ESP_ERR_INVALID_ARG;
    read_one(s, 0x03, &s->fuel_system_status, OBD_VALID_FUEL_SYSTEM);
    read_one(s, 0x06, &s->short_fuel_trim_b1_pct, OBD_VALID_STFT_B1);
    read_one(s, 0x07, &s->long_fuel_trim_b1_pct, OBD_VALID_LTFT_B1);
    read_one(s, 0x0B, &s->intake_manifold_kpa, OBD_VALID_MAP);
    read_one(s, 0x10, &s->maf_g_s, OBD_VALID_MAF);
    read_one(s, 0x1C, &s->obd_standard, OBD_VALID_OBD_STANDARD);
    read_one(s, 0x21, &s->distance_mil_km, OBD_VALID_DIST_MIL);
    read_one(s, 0x31, &s->distance_since_clear_km, OBD_VALID_DIST_CLEAR);
    read_one(s, 0x51, &s->fuel_type, OBD_VALID_FUEL_TYPE);
    read_one(s, 0x5E, &s->engine_fuel_rate_l_h, OBD_VALID_FUEL_RATE);
    s->sampled_at_ms = esp_timer_get_time() / 1000;
    return s->valid_mask ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void format_dtc(uint8_t a, uint8_t b, char out[6])
{
    static const char systems[] = "PCBU";
    out[0] = systems[(a >> 6) & 3];
    out[1] = '0' + ((a >> 4) & 3);
    const char hex[] = "0123456789ABCDEF";
    out[2] = hex[a & 0x0F];
    out[3] = hex[b >> 4];
    out[4] = hex[b & 0x0F];
    out[5] = '\0';
}

static bool append_unique_dtc(char output[][6], uint8_t *count,
                              uint8_t a, uint8_t b)
{
    /* 0000 means no DTC. FF bytes are common unused-frame padding. */
    if ((a == 0x00 && b == 0x00) || a == 0xFF || b == 0xFF)
        return false;
    char code[6];
    format_dtc(a, b, code);
    for (uint8_t i = 0; i < *count; ++i) {
        if (strcmp(output[i], code) == 0)
            return false;
    }
    if (*count >= OBD_MAX_DTC)
        return false;
    memcpy(output[*count], code, sizeof(code));
    ++(*count);
    return true;
}

static esp_err_t query_dtc_mode(uint8_t mode, char output[][6], uint8_t *count)
{
    const twai_message_t request = {
        .identifier = 0x7DF, .data_length_code = 8,
        .data = {0x01, mode, 0, 0, 0, 0, 0, 0}
    };
    *count = 0;
    esp_err_t err = twai_transmit(&request, pdMS_TO_TICKS(100));
    if (err != ESP_OK) return err;
    const int64_t deadline = esp_timer_get_time() + 500000;
    while (esp_timer_get_time() < deadline && *count < OBD_MAX_DTC) {
        twai_message_t r;
        if (twai_receive(&r, pdMS_TO_TICKS(20)) != ESP_OK) continue;
        if (r.identifier < 0x7E8 || r.identifier > 0x7EF ||
            r.data_length_code < 3) continue;
        const uint8_t pci_type = r.data[0] >> 4;
        if (pci_type != 0) {
            /* Do not misdecode ISO-TP first/consecutive frames as DTCs.
             * Multi-frame assembly will be added separately when required. */
            ESP_LOGW(TAG, "DTC mode %02X multi-frame response ignored safely",
                     mode);
            continue;
        }
        const uint8_t payload_len = r.data[0] & 0x0F;
        if (payload_len < 1 || payload_len > 7 ||
            r.data[1] != (uint8_t)(0x40 + mode)) continue;
        const int end = 1 + payload_len;
        for (int i = 2; i + 1 <= end && i + 1 < r.data_length_code; i += 2) {
            append_unique_dtc(output, count, r.data[i], r.data[i + 1]);
        }
    }
    return ESP_OK;
}

esp_err_t obd2_source_read_dtc(obd_dtc_report_t *r)
{
    if (r == NULL) return ESP_ERR_INVALID_ARG;
    memset(r, 0, sizeof(*r));
    query_dtc_mode(0x03, r->stored, &r->stored_count);
    query_dtc_mode(0x07, r->pending, &r->pending_count);
    query_dtc_mode(0x0A, r->permanent, &r->permanent_count);
    r->sampled_at_ms = esp_timer_get_time() / 1000;
    return ESP_OK;
}

#endif

uint32_t obd2_source_supported_mask(void) { return s_supported_mask; }
uint32_t obd2_source_success_count(void) { return s_success_count; }
uint32_t obd2_source_error_count(void) { return s_error_count; }
const char *obd2_source_mode(void)
{
#if CONFIG_OBD_SIMULATOR
    return "simulator";
#else
    return "obd2_can";
#endif
}
