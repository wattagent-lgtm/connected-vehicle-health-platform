#include "obd2_pid.h"

esp_err_t obd2_decode_mode01(uint8_t pid, const uint8_t *data, size_t length,
                            float *value)
{
    if (data == NULL || value == NULL || length < 1) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (pid) {
    case 0x03: *value = (float)data[0]; return ESP_OK;
    case 0x04: *value = data[0] * 100.0f / 255.0f; return ESP_OK;
    case 0x05: *value = (float)data[0] - 40.0f; return ESP_OK;
    case 0x06:
    case 0x07: *value = (data[0] * 100.0f / 128.0f) - 100.0f; return ESP_OK;
    case 0x0B: *value = (float)data[0]; return ESP_OK;
    case 0x0C:
        if (length < 2) return ESP_ERR_INVALID_SIZE;
        *value = ((data[0] * 256.0f) + data[1]) / 4.0f;
        return ESP_OK;
    case 0x0D: *value = (float)data[0]; return ESP_OK;
    case 0x0F: *value = (float)data[0] - 40.0f; return ESP_OK;
    case 0x10:
        if (length < 2) return ESP_ERR_INVALID_SIZE;
        *value = ((data[0] << 8) | data[1]) / 100.0f; return ESP_OK;
    case 0x11: *value = data[0] * 100.0f / 255.0f; return ESP_OK;
    case 0x2F: *value = data[0] * 100.0f / 255.0f; return ESP_OK;
    case 0x1F:
        if (length < 2) return ESP_ERR_INVALID_SIZE;
        *value = (float)((data[0] << 8) | data[1]); return ESP_OK;
    case 0x1C: *value = (float)data[0]; return ESP_OK;
    case 0x21:
    case 0x31:
        if (length < 2) return ESP_ERR_INVALID_SIZE;
        *value = (float)((data[0] << 8) | data[1]); return ESP_OK;
    case 0x42:
        if (length < 2) return ESP_ERR_INVALID_SIZE;
        *value = ((data[0] << 8) | data[1]) / 1000.0f; return ESP_OK;
    case 0x46: *value = (float)data[0] - 40.0f; return ESP_OK;
    case 0x51: *value = (float)data[0]; return ESP_OK;
    case 0x5E:
        if (length < 2) return ESP_ERR_INVALID_SIZE;
        *value = ((data[0] << 8) | data[1]) / 20.0f; return ESP_OK;
    default: return ESP_ERR_NOT_SUPPORTED;
    }
}

const char *obd2_pid_name(uint8_t pid)
{
    switch (pid) {
    case 0x03: return "fuel_system_status";
    case 0x04: return "engine_load_pct";
    case 0x05: return "coolant_c";
    case 0x06: return "short_fuel_trim_b1_pct";
    case 0x07: return "long_fuel_trim_b1_pct";
    case 0x0B: return "intake_manifold_kpa";
    case 0x0C: return "engine_rpm";
    case 0x0D: return "vehicle_speed_kph";
    case 0x0F: return "intake_air_c";
    case 0x10: return "maf_g_s";
    case 0x11: return "throttle_pct";
    case 0x2F: return "fuel_level_pct";
    case 0x1F: return "engine_runtime_s";
    case 0x1C: return "obd_standard";
    case 0x21: return "distance_mil_km";
    case 0x31: return "distance_since_clear_km";
    case 0x42: return "control_module_voltage_v";
    case 0x46: return "ambient_air_c";
    case 0x51: return "fuel_type";
    case 0x5E: return "engine_fuel_rate_l_h";
    default: return "unknown";
    }
}

const char *obd2_pid_unit(uint8_t pid)
{
    switch (pid) {
    case 0x0C: return "rpm";
    case 0x0D: return "km/h";
    case 0x05:
    case 0x0F: return "degC";
    case 0x0B: return "kPa";
    case 0x10: return "g/s";
    case 0x04:
    case 0x06:
    case 0x07:
    case 0x11:
    case 0x2F: return "pct";
    case 0x1F: return "s";
    case 0x42: return "V";
    case 0x46: return "degC";
    case 0x21:
    case 0x31: return "km";
    case 0x5E: return "L/h";
    default: return "";
    }
}
