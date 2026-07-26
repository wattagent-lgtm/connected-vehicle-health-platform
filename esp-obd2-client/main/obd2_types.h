#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    OBD_QUALITY_GOOD = 0,
    OBD_QUALITY_STALE,
    OBD_QUALITY_UNSUPPORTED,
    OBD_QUALITY_ERROR
} obd_quality_t;

typedef struct {
    float engine_rpm;
    float vehicle_speed_kph;
    float engine_load_pct;
    float throttle_pct;
    float coolant_c;
    float intake_air_c;
    float fuel_level_pct;
    float control_module_voltage_v;
    float engine_runtime_s;
    float ambient_air_c;
    float short_fuel_trim_b1_pct;
    float long_fuel_trim_b1_pct;
    float intake_manifold_kpa;
    float maf_g_s;
    float distance_mil_km;
    float distance_since_clear_km;
    float engine_fuel_rate_l_h;
    float fuel_system_status;
    float obd_standard;
    float fuel_type;
    bool mil_on;
    uint8_t dtc_count;
    uint32_t valid_mask;
    int64_t sampled_at_ms;
} obd_snapshot_t;

#define OBD_VALID_RPM          (1U << 0)
#define OBD_VALID_SPEED        (1U << 1)
#define OBD_VALID_LOAD         (1U << 2)
#define OBD_VALID_THROTTLE     (1U << 3)
#define OBD_VALID_COOLANT      (1U << 4)
#define OBD_VALID_INTAKE       (1U << 5)
#define OBD_VALID_FUEL         (1U << 6)
#define OBD_VALID_VOLTAGE      (1U << 7)
#define OBD_VALID_RUNTIME      (1U << 8)
#define OBD_VALID_AMBIENT      (1U << 9)
#define OBD_VALID_MIL          (1U << 10)
#define OBD_VALID_STFT_B1      (1U << 11)
#define OBD_VALID_LTFT_B1      (1U << 12)
#define OBD_VALID_MAP          (1U << 13)
#define OBD_VALID_MAF          (1U << 14)
#define OBD_VALID_FUEL_SYSTEM  (1U << 15)
#define OBD_VALID_OBD_STANDARD (1U << 16)
#define OBD_VALID_DIST_MIL     (1U << 17)
#define OBD_VALID_DIST_CLEAR   (1U << 18)
#define OBD_VALID_FUEL_TYPE    (1U << 19)
#define OBD_VALID_FUEL_RATE    (1U << 20)

#define OBD_MAX_DTC 12

typedef struct {
    char stored[OBD_MAX_DTC][6];
    char pending[OBD_MAX_DTC][6];
    char permanent[OBD_MAX_DTC][6];
    uint8_t stored_count;
    uint8_t pending_count;
    uint8_t permanent_count;
    bool mil_on;
    uint8_t ecu_dtc_count;
    int64_t sampled_at_ms;
} obd_dtc_report_t;
