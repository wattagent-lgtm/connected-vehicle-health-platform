# Data Model and API Contract

## Common envelope

Every payload carries identity, schema and timing metadata:

```json
{
  "schema_version": 1,
  "protocol": "iiot-edge-json",
  "device_id": "CAR-01-OBD",
  "device_name": "Vehicle-01",
  "device_type": "obd2_vehicle",
  "message_type": "telemetry",
  "data_class": "fast",
  "source_mode": "obd2_can",
  "sequence": 42,
  "uptime_ms": 123456,
  "data": {}
}
```

AWS adds authoritative `received_at` epoch milliseconds. Vehicle data without
a trusted RTC must not fabricate wall-clock timestamps.

## DynamoDB

- Partition key: `device_id` (String)
- Sort key: `received_at` (Number)
- Additional attributes: identity, class, source, sequence and `data`

The Lambda queries recent records for one device and merges the newest value
from each class. It never treats missing/unsupported parameters as zero.

## HTTP API

| Route | Purpose |
|---|---|
| `GET /api/cars/{id}/health` | Current merged state, warnings and health level |
| `GET /api/cars/{id}/latest` | Latest raw class records |
| `GET /api/cars/{id}/history` | Time-series points |
| `GET /api/cars/{id}/maintenance` | Rule-based maintenance evidence |
| `GET /api/cars/{id}/trip` | Distance/fuel estimates and confidence |

Responses should include `received_at`, `last_seen_seconds`,
`records_evaluated`, `available_data_classes` and source mode.

## UNS topics

```text
dt/{namespace}/{site}/vehicle/{device_type}/{device_id}/telemetry/fast
dt/{namespace}/{site}/vehicle/{device_type}/{device_id}/telemetry/slow
dt/{namespace}/{site}/vehicle/{device_type}/{device_id}/event
dt/{namespace}/{site}/vehicle/{device_type}/{device_id}/diagnostic
```

Topic segments use lowercase slug form; payload identity retains canonical
vehicle/device labels.
