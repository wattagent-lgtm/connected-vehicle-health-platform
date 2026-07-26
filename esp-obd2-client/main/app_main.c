#include <math.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "gateway_client.h"
#include "obd2_source.h"
#include "wifi_manager.h"
#include "sdkconfig.h"

static const char *TAG = "obd_client";
static uint32_t s_sequence;
static obd_snapshot_t s_latest;
static obd_snapshot_t s_last_sent;
static int64_t s_last_obd_success_ms;

static bool changed(float current, float previous, float deadband)
{
    return !isfinite(previous) || fabsf(current - previous) >= deadband;
}

static cJSON *base_message(const char *message_type, const char *data_class)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;
    cJSON_AddNumberToObject(root, "schema_version", 1);
    cJSON_AddStringToObject(root, "protocol", "iiot-edge-json");
    cJSON_AddNumberToObject(root, "protocol_version", 1);
    cJSON_AddStringToObject(root, "device_id", CONFIG_OBD_DEVICE_ID);
    cJSON_AddStringToObject(root, "device_name", CONFIG_OBD_DEVICE_NAME);
    cJSON_AddStringToObject(root, "device_type", "obd2_vehicle");
    cJSON_AddStringToObject(root, "site", "mobile");
    cJSON_AddStringToObject(root, "area", "vehicle");
    cJSON_AddStringToObject(root, "asset", "car01");
    cJSON_AddStringToObject(root, "message_type", message_type);
    cJSON_AddStringToObject(root, "data_class", data_class);
    cJSON_AddStringToObject(root, "source_mode", obd2_source_mode());
    cJSON_AddNumberToObject(root, "sequence", ++s_sequence);
    char message_id[64];
    snprintf(message_id, sizeof(message_id), "%s-%lu", CONFIG_OBD_DEVICE_ID,
             (unsigned long)s_sequence);
    cJSON_AddStringToObject(root, "message_id", message_id);
    cJSON_AddNumberToObject(root, "uptime_ms", esp_timer_get_time() / 1000);
    return root;
}

static esp_err_t send_json(cJSON *root)
{
    if (root == NULL) return ESP_ERR_NO_MEM;
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (text == NULL) return ESP_ERR_NO_MEM;
    const esp_err_t result = gateway_client_send(text);
    cJSON_free(text);
    return result;
}

static void add_if_valid(cJSON *data, const char *name, float value,
                         uint32_t mask, uint32_t bit)
{
    if (mask & bit) cJSON_AddNumberToObject(data, name, value);
}

static esp_err_t publish_fast(void)
{
    cJSON *root = base_message("telemetry", "fast");
    if (root == NULL) return ESP_ERR_NO_MEM;
    cJSON *data = cJSON_AddObjectToObject(root, "data");
    add_if_valid(data, "engine_rpm", s_latest.engine_rpm,
                 s_latest.valid_mask, OBD_VALID_RPM);
    add_if_valid(data, "vehicle_speed_kph", s_latest.vehicle_speed_kph,
                 s_latest.valid_mask, OBD_VALID_SPEED);
    add_if_valid(data, "engine_load_pct", s_latest.engine_load_pct,
                 s_latest.valid_mask, OBD_VALID_LOAD);
    add_if_valid(data, "throttle_pct", s_latest.throttle_pct,
                 s_latest.valid_mask, OBD_VALID_THROTTLE);
    return send_json(root);
}

static esp_err_t publish_slow(void)
{
    cJSON *root = base_message("telemetry", "slow");
    if (root == NULL) return ESP_ERR_NO_MEM;
    cJSON *data = cJSON_AddObjectToObject(root, "data");
    add_if_valid(data, "coolant_c", s_latest.coolant_c,
                 s_latest.valid_mask, OBD_VALID_COOLANT);
    add_if_valid(data, "intake_air_c", s_latest.intake_air_c,
                 s_latest.valid_mask, OBD_VALID_INTAKE);
    add_if_valid(data, "fuel_level_pct", s_latest.fuel_level_pct,
                 s_latest.valid_mask, OBD_VALID_FUEL);
    add_if_valid(data, "control_module_voltage_v",
                 s_latest.control_module_voltage_v,
                 s_latest.valid_mask, OBD_VALID_VOLTAGE);
    add_if_valid(data, "engine_runtime_s", s_latest.engine_runtime_s,
                 s_latest.valid_mask, OBD_VALID_RUNTIME);
    add_if_valid(data, "ambient_air_c", s_latest.ambient_air_c,
                 s_latest.valid_mask, OBD_VALID_AMBIENT);
    add_if_valid(data, "short_fuel_trim_b1_pct", s_latest.short_fuel_trim_b1_pct,
                 s_latest.valid_mask, OBD_VALID_STFT_B1);
    add_if_valid(data, "long_fuel_trim_b1_pct", s_latest.long_fuel_trim_b1_pct,
                 s_latest.valid_mask, OBD_VALID_LTFT_B1);
    add_if_valid(data, "intake_manifold_kpa", s_latest.intake_manifold_kpa,
                 s_latest.valid_mask, OBD_VALID_MAP);
    add_if_valid(data, "maf_g_s", s_latest.maf_g_s,
                 s_latest.valid_mask, OBD_VALID_MAF);
    add_if_valid(data, "distance_mil_km", s_latest.distance_mil_km,
                 s_latest.valid_mask, OBD_VALID_DIST_MIL);
    add_if_valid(data, "distance_since_clear_km", s_latest.distance_since_clear_km,
                 s_latest.valid_mask, OBD_VALID_DIST_CLEAR);
    add_if_valid(data, "engine_fuel_rate_l_h", s_latest.engine_fuel_rate_l_h,
                 s_latest.valid_mask, OBD_VALID_FUEL_RATE);
    if (s_latest.valid_mask & OBD_VALID_FUEL_SYSTEM)
        cJSON_AddNumberToObject(data, "fuel_system_status", s_latest.fuel_system_status);
    if (s_latest.valid_mask & OBD_VALID_OBD_STANDARD)
        cJSON_AddNumberToObject(data, "obd_standard", s_latest.obd_standard);
    if (s_latest.valid_mask & OBD_VALID_FUEL_TYPE)
        cJSON_AddNumberToObject(data, "fuel_type", s_latest.fuel_type);
    return send_json(root);
}

static esp_err_t publish_diagnostic(void)
{
    cJSON *root = base_message("diagnostic", "diagnostic");
    if (root == NULL) return ESP_ERR_NO_MEM;
    cJSON *data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddNumberToObject(data, "free_heap_bytes",
                            heap_caps_get_free_size(MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(data, "supported_pid_mask",
                            obd2_source_supported_mask());
    cJSON_AddBoolToObject(data, "read_only", true);
    cJSON_AddNumberToObject(data, "obd_success_count",
                            obd2_source_success_count());
    cJSON_AddNumberToObject(data, "obd_error_count",
                            obd2_source_error_count());
    const int64_t age = s_last_obd_success_ms > 0 ?
        (esp_timer_get_time() / 1000 - s_last_obd_success_ms) : -1;
    cJSON_AddNumberToObject(data, "obd_data_age_ms", age);
    cJSON_AddStringToObject(data, "vehicle_link_state",
        age < 0 ? "STARTING" : age <= 5000 ? "ONLINE" :
        age <= 15000 ? "STALE" : "OFFLINE");
    return send_json(root);
}

static esp_err_t publish_dtc(void)
{
    obd_dtc_report_t report;
    if (obd2_source_read_dtc(&report) != ESP_OK) return ESP_FAIL;
    cJSON *root = base_message("dtc_event", "event");
    if (root == NULL) return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(root, "read_only", true);
    cJSON *data = cJSON_AddObjectToObject(root, "data");
    cJSON *stored = cJSON_AddArrayToObject(data, "stored_dtc");
    cJSON *pending = cJSON_AddArrayToObject(data, "pending_dtc");
    cJSON *permanent = cJSON_AddArrayToObject(data, "permanent_dtc");
    for (int i = 0; i < report.stored_count; ++i)
        cJSON_AddItemToArray(stored, cJSON_CreateString(report.stored[i]));
    for (int i = 0; i < report.pending_count; ++i)
        cJSON_AddItemToArray(pending, cJSON_CreateString(report.pending[i]));
    for (int i = 0; i < report.permanent_count; ++i)
        cJSON_AddItemToArray(permanent, cJSON_CreateString(report.permanent[i]));
    cJSON_AddStringToObject(data, "safety_policy",
                            "read_only_no_mode04");
    return send_json(root);
}

static void telemetry_task(void *arg)
{
    (void)arg;
    memset(&s_latest, 0, sizeof(s_latest));
    memset(&s_last_sent, 0, sizeof(s_last_sent));
    s_last_sent.engine_rpm = NAN;
    s_last_sent.vehicle_speed_kph = NAN;
    s_last_sent.engine_load_pct = NAN;
    s_last_sent.throttle_pct = NAN;
    s_last_sent.coolant_c = NAN;
    s_last_sent.intake_air_c = NAN;
    s_last_sent.fuel_level_pct = NAN;
    s_last_sent.control_module_voltage_v = NAN;
    s_last_sent.engine_runtime_s = NAN;
    s_last_sent.ambient_air_c = NAN;

    TickType_t fast_wake = xTaskGetTickCount();
    int slow_count = 0;
    int extended_count = 0;
    int diagnostic_count = 0;
    while (true) {
        if (obd2_source_read_fast(&s_latest) == ESP_OK)
            s_last_obd_success_ms = esp_timer_get_time() / 1000;
        const bool fast_due =
            changed(s_latest.engine_rpm, s_last_sent.engine_rpm, 25.0f) ||
            changed(s_latest.vehicle_speed_kph, s_last_sent.vehicle_speed_kph, 1.0f) ||
            changed(s_latest.engine_load_pct, s_last_sent.engine_load_pct, 1.0f) ||
            changed(s_latest.throttle_pct, s_last_sent.throttle_pct, 1.0f);
        if (fast_due && publish_fast() == ESP_OK) {
            s_last_sent.engine_rpm = s_latest.engine_rpm;
            s_last_sent.vehicle_speed_kph = s_latest.vehicle_speed_kph;
            s_last_sent.engine_load_pct = s_latest.engine_load_pct;
            s_last_sent.throttle_pct = s_latest.throttle_pct;
        }

        if (++slow_count >= 5) {
            slow_count = 0;
            if (obd2_source_read_slow(&s_latest) == ESP_OK)
                s_last_obd_success_ms = esp_timer_get_time() / 1000;
            const bool slow_due =
                changed(s_latest.coolant_c, s_last_sent.coolant_c, 0.5f) ||
                changed(s_latest.intake_air_c, s_last_sent.intake_air_c, 0.5f) ||
                changed(s_latest.fuel_level_pct, s_last_sent.fuel_level_pct, 0.5f);
            const bool slow_extra_due =
                changed(s_latest.control_module_voltage_v,
                        s_last_sent.control_module_voltage_v, 0.1f) ||
                changed(s_latest.ambient_air_c,
                        s_last_sent.ambient_air_c, 1.0f);
            if ((slow_due || slow_extra_due) && publish_slow() == ESP_OK) {
                s_last_sent.coolant_c = s_latest.coolant_c;
                s_last_sent.intake_air_c = s_latest.intake_air_c;
                s_last_sent.fuel_level_pct = s_latest.fuel_level_pct;
                s_last_sent.control_module_voltage_v =
                    s_latest.control_module_voltage_v;
                s_last_sent.engine_runtime_s = s_latest.engine_runtime_s;
                s_last_sent.ambient_air_c = s_latest.ambient_air_c;
            }
        }
        if (++extended_count >= 30) {
            extended_count = 0;
            if (obd2_source_read_extended(&s_latest) == ESP_OK) {
                s_last_obd_success_ms = esp_timer_get_time() / 1000;
                publish_slow();
            }
        }
        if (++diagnostic_count >= 60) {
            diagnostic_count = 0;
            publish_diagnostic();
            publish_dtc();
        }
        vTaskDelayUntil(&fast_wake, pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);
    ESP_ERROR_CHECK(wifi_manager_start());
    ESP_ERROR_CHECK(wifi_manager_wait_connected());
    ESP_ERROR_CHECK(obd2_source_init());
    ESP_LOGI(TAG, "Starting %s client -> %s:%d", obd2_source_mode(),
             CONFIG_OBD_GATEWAY_HOST, CONFIG_OBD_GATEWAY_PORT);
    xTaskCreate(telemetry_task, "obd_telemetry", 6144, NULL, 5, NULL);
}
