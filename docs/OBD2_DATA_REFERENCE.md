# OBD-II Data and Payload Reference

## Acquisition principle

The client discovers supported Mode 01 PIDs, polls only supported values, and
converts ECU bytes to engineering units before creating JSON. A timeout,
malformed reply, or unsupported PID is omitted; it is never converted to a
plausible-looking zero.

The implementation uses ISO 15765-4 functional request ID `0x7DF` and accepts
standard diagnostic response IDs `0x7E8-0x7EF`. Manufacturer-specific Nissan
PIDs are outside the current portable, read-only baseline.

## Implemented standard PIDs

`A` and `B` are unsigned response bytes.

| PID | Field | Decode | Unit |
|---:|---|---|---|
| `03` | fuel system status | `A` | enum/bit field |
| `04` | calculated engine load | `A × 100 / 255` | % |
| `05` | coolant temperature | `A - 40` | °C |
| `06` | short fuel trim bank 1 | `A × 100 / 128 - 100` | % |
| `07` | long fuel trim bank 1 | `A × 100 / 128 - 100` | % |
| `0B` | intake manifold pressure | `A` | kPa |
| `0C` | engine speed | `(256A + B) / 4` | rpm |
| `0D` | vehicle speed | `A` | km/h |
| `0F` | intake air temperature | `A - 40` | °C |
| `10` | mass air flow | `(256A + B) / 100` | g/s |
| `11` | throttle position | `A × 100 / 255` | % |
| `1C` | OBD standard | `A` | enum |
| `1F` | engine runtime | `256A + B` | s |
| `21` | distance with MIL on | `256A + B` | km |
| `2F` | fuel level input | `A × 100 / 255` | % |
| `31` | distance since DTC clear | `256A + B` | km |
| `42` | control-module voltage | `(256A + B) / 1000` | V |
| `46` | ambient air temperature | `A - 40` | °C |
| `51` | fuel type | `A` | enum |
| `5E` | engine fuel rate | `(256A + B) / 20` | L/h |

Support varies by vehicle and ECU. Absence of a field is not itself a fault.

## DTC read-only data

The diagnostic class can represent:

- stored DTCs from Mode 03;
- pending DTCs from Mode 07;
- permanent DTCs from Mode 0A;
- malfunction indicator lamp state; and
- ECU-reported DTC count.

The maximum in-memory list is bounded. Codes are validated before display.
This project does not implement Mode 04 DTC clearing. A displayed DTC is a
lead for service investigation, not a complete diagnosis; freeze-frame data,
manufacturer procedures, and a qualified scan tool may still be required.

## Validity and quality

The firmware maintains a validity bit for every measurement. Downstream code
should apply these rules:

1. use a value only when its validity bit is set;
2. reject NaN, infinity, malformed types, and physically impossible ranges;
3. preserve source and AWS receive timestamps;
4. track sequence gaps and duplicate `message_id` values;
5. show missing as `--` or `N/A`, not zero;
6. mark an aged class stale independently of other classes; and
7. keep simulator and real-CAN data visibly distinguishable.

Quality states are `GOOD`, `STALE`, `UNSUPPORTED`, and `ERROR`.

## JSON envelope

Example fast record:

```json
{
  "schema_version": 1,
  "protocol": "iiot-edge-json",
  "protocol_version": 1,
  "device_id": "CAR-01-OBD",
  "device_name": "Vehicle-01",
  "device_type": "obd2_vehicle",
  "site": "mobile",
  "area": "vehicle",
  "asset": "car01",
  "message_type": "telemetry",
  "data_class": "fast",
  "source_mode": "obd2_can",
  "sequence": 1204,
  "message_id": "CAR-01-OBD-1204",
  "uptime_ms": 456789,
  "data": {
    "engine_rpm": 1832.5,
    "vehicle_speed_kph": 72,
    "engine_load_pct": 31.4,
    "throttle_pct": 18.8
  }
}
```

Slow records carry values such as coolant, intake temperature, fuel level,
voltage, runtime, fuel trims, MAF, distance counters, and fuel rate.
Diagnostic records carry DTC lists and link/collection diagnostics.

## Multi-rate design

| Class | Typical content | Design purpose |
|---|---|---|
| `fast` | RPM, speed, throttle, load | responsive driving trend |
| `slow` | coolant, fuel, trims, voltage, MAF | thermal/maintenance trend |
| `event` | ignition/link/status transition | immediate state change |
| `diagnostic` | DTCs, counters, client heap | troubleshooting |

Exact intervals must be validated against ECU response time, Wi-Fi/TCP load,
gateway heap, LTE bandwidth, and AWS cost. Do not poll every PID at 100 ms.
Schedule groups independently and use deadband/change-of-state publishing for
slow or discrete signals.

## Derived trip and fuel estimates

Distance can be integrated from speed samples:

```text
distance_km += speed_kph × elapsed_seconds / 3600
```

When PID `5E` is supported, fuel can be integrated directly:

```text
fuel_liters += fuel_rate_l_h × elapsed_seconds / 3600
```

Otherwise a gasoline estimate may be derived from MAF and an assumed
air-fuel ratio/density. That result is less reliable because enrichment,
deceleration fuel cut, fuel type, sensor bias, and missing samples affect it.

```text
fuel_mass_g_s ≈ maf_g_s / assumed_AFR
fuel_l_h ≈ fuel_mass_g_s × 3600 / fuel_density_g_l
consumption_l_100km = fuel_liters / distance_km × 100
economy_km_l = distance_km / fuel_liters
```

Every estimate should include:

- sample count;
- time coverage;
- distance coverage;
- selected fuel-rate method;
- confidence label; and
- explicit `estimated`, not billing-grade, wording.

Avoid calculating `L/100 km` at very low distance or while stationary.

## Plausibility examples

| Signal | Plausibility rule |
|---|---|
| RPM | zero engine-off; stable plausible idle after start |
| Speed | zero while stationary; compare with cluster/GPS with tolerance |
| Coolant | rises gradually after cold start; no instant large jumps |
| Throttle/load | 0-100%; correlated with operating state |
| Fuel trims | investigate sustained large magnitude, not single samples |
| Voltage | interpret differently engine-off versus engine-running |
| Fuel level | changes slowly; affected by tank geometry/slosh |
| DTC | confirm with standard scanner and service information |

Thresholds in the demonstration API are examples. Establish a healthy
vehicle-specific baseline over comparable trips before using drift alerts.
