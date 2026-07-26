import json
import os
import re
import time
from decimal import Decimal

import boto3
from boto3.dynamodb.conditions import Key


TABLE_NAME = os.environ.get("TABLE_NAME", "iiot-gateway-telemetry")
DEFAULT_DEVICE_ID = os.environ.get("DEFAULT_DEVICE_ID", "CAR-01-OBD")
QUERY_LIMIT = int(os.environ.get("QUERY_LIMIT", "100"))
MAINTENANCE_QUERY_LIMIT = int(os.environ.get("MAINTENANCE_QUERY_LIMIT", "500"))
TRIP_QUERY_LIMIT = int(os.environ.get("TRIP_QUERY_LIMIT", "300"))

table = boto3.resource("dynamodb").Table(TABLE_NAME)
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FRONTEND_ROOT = os.path.join(PROJECT_ROOT, "frontend")


def _native(value):
    if isinstance(value, Decimal):
        return int(value) if value % 1 == 0 else float(value)
    if isinstance(value, list):
        return [_native(item) for item in value]
    if isinstance(value, dict):
        return {key: _native(item) for key, item in value.items()}
    return value


def _data(value):
    if isinstance(value, dict):
        return _native(value)
    if isinstance(value, str):
        try:
            parsed = json.loads(value)
            return _native(parsed) if isinstance(parsed, dict) else {}
        except (TypeError, ValueError):
            return {}
    return {}


def _response(status_code, body):
    return {
        "statusCode": status_code,
        "headers": {
            "Content-Type": "application/json",
            "Cache-Control": "no-store",
            "Access-Control-Allow-Origin": "*",
        },
        "body": json.dumps(_native(body), separators=(",", ":")),
    }


def _file_response(filename, content_type):
    path = os.path.join(FRONTEND_ROOT, filename)
    try:
        with open(path, "r", encoding="utf-8") as file:
            body = file.read()
    except OSError:
        return _response(404, {"status": "NOT_FOUND", "message": "Asset not found"})
    return {
        "statusCode": 200,
        "headers": {
            "Content-Type": content_type,
            "Cache-Control": "no-cache",
        },
        "body": body,
    }


def _device_id(event):
    path_parameters = event.get("pathParameters") or {}
    return (
        path_parameters.get("device_id")
        or path_parameters.get("deviceId")
        or DEFAULT_DEVICE_ID
    )


def _route(event):
    context = event.get("requestContext") or {}
    http = context.get("http") or {}
    return event.get("routeKey", ""), http.get("path", event.get("rawPath", ""))


def _query(device_id, limit=QUERY_LIMIT, start_ms=None):
    expression = Key("device_id").eq(device_id)
    if start_ms is not None:
        expression &= Key("received_at").gte(start_ms)

    result = table.query(
        KeyConditionExpression=expression,
        ScanIndexForward=False,
        Limit=limit,
    )
    return [_native(item) for item in result.get("Items", [])]


def _merge(records):
    telemetry = {}
    latest_by_class = {}

    # DynamoDB returns newest first. Never overwrite a newer tag with an older one.
    for record in records:
        data_class = str(record.get("data_class", "unknown"))
        if data_class not in latest_by_class:
            latest_by_class[data_class] = record

        for name, value in _data(record.get("data")).items():
            if name not in telemetry:
                telemetry[name] = value

    return telemetry, latest_by_class


_DTC_PATTERN = re.compile(r"^[PBCU][0-3][0-9A-F]{3}$")


def _valid_dtc_codes(value):
    """Discard empty/padding frames that some ECUs return as diagnostic codes."""
    if not isinstance(value, list):
        return []
    result = []
    for raw_code in value:
        code = str(raw_code).strip().upper()
        if (
            _DTC_PATTERN.match(code)
            and code not in ("P0000", "P00FF")
            and not code.endswith("FFF")
            and code not in result
        ):
            result.append(code)
    return result


def _received_ms(record):
    try:
        value = int(record.get("received_at", 0))
    except (TypeError, ValueError):
        return 0
    return value * 1000 if 0 < value < 100000000000 else value


def _finite(value):
    try:
        number = float(value)
        return number if number == number and abs(number) != float("inf") else None
    except (TypeError, ValueError):
        return None


def _trip_estimate(records):
    """Estimate the latest observed trip from read-only OBD speed and fuel data."""
    state = {}
    field_times = {}
    previous = None
    distance_km = 0.0
    fuel_l = 0.0
    idle_seconds = 0.0
    idle_fuel_l = 0.0
    valid_seconds = 0.0
    possible_seconds = 0.0
    valid_intervals = 0
    gap_count = 0
    start_ms = 0
    end_ms = 0
    max_speed = 0.0
    max_rpm = 0.0
    max_coolant = None
    fuel_source = None

    for record in reversed(records):
        timestamp_ms = _received_ms(record)
        if not timestamp_ms:
            continue
        for name, value in _data(record.get("data")).items():
            state[name] = value
            field_times[name] = timestamp_ms

        speed = _finite(state.get("vehicle_speed_kph"))
        rpm = _finite(state.get("engine_rpm"))
        coolant = _finite(state.get("coolant_c"))
        direct_rate = _finite(state.get("engine_fuel_rate_l_h"))
        maf = _finite(state.get("maf_g_s"))

        rate = None
        source = None
        if direct_rate is not None and direct_rate >= 0:
            rate, source = direct_rate, "PID_5E_DIRECT"
        elif maf is not None and maf >= 0:
            # Gasoline estimate: MAF(g/s) * 3600 / (14.7 AFR * 745 g/L).
            rate, source = maf * 3600.0 / (14.7 * 745.0), "MAF_GASOLINE_ESTIMATE"

        fresh = (
            speed is not None
            and rpm is not None
            and rate is not None
            and timestamp_ms - field_times.get("vehicle_speed_kph", 0) <= 15000
            and timestamp_ms - field_times.get("engine_rpm", 0) <= 15000
            and timestamp_ms - field_times.get(
                "engine_fuel_rate_l_h" if source == "PID_5E_DIRECT" else "maf_g_s", 0
            ) <= 15000
        )
        engine_running = rpm is not None and rpm > 400
        if not fresh or not engine_running:
            previous = None
            continue

        max_speed = max(max_speed, speed)
        max_rpm = max(max_rpm, rpm)
        if coolant is not None:
            max_coolant = coolant if max_coolant is None else max(max_coolant, coolant)
        fuel_source = source
        if not start_ms:
            start_ms = timestamp_ms
        end_ms = timestamp_ms

        current = (timestamp_ms, speed, rate)
        if previous and timestamp_ms > previous[0]:
            dt = (timestamp_ms - previous[0]) / 1000.0
            possible_seconds += min(dt, 15.0)
            if dt <= 15.0:
                average_speed = (previous[1] + speed) / 2.0
                average_rate = (previous[2] + rate) / 2.0
                distance_km += average_speed * dt / 3600.0
                increment_l = average_rate * dt / 3600.0
                fuel_l += increment_l
                if average_speed < 1.0:
                    idle_seconds += dt
                    idle_fuel_l += increment_l
                valid_seconds += dt
                valid_intervals += 1
            else:
                gap_count += 1
        previous = current

    duration_seconds = max(0.0, (end_ms - start_ms) / 1000.0) if start_ms else 0.0
    coverage = min(100.0, valid_seconds / possible_seconds * 100.0) if possible_seconds else 0.0
    average_speed = distance_km / (valid_seconds / 3600.0) if valid_seconds else 0.0
    l_per_100km = fuel_l / distance_km * 100.0 if distance_km >= 0.1 else None
    km_per_l = distance_km / fuel_l if fuel_l >= 0.01 else None
    latest_rate = previous[2] if previous else None
    latest_speed = previous[1] if previous else None
    instant_l_per_100km = (
        latest_rate / latest_speed * 100.0
        if latest_rate is not None and latest_speed is not None and latest_speed >= 5.0
        else None
    )
    if coverage >= 90 and duration_seconds >= 600 and valid_intervals >= 30:
        confidence = "HIGH"
    elif coverage >= 70 and duration_seconds >= 180 and valid_intervals >= 10:
        confidence = "MEDIUM"
    else:
        confidence = "LOW"

    return {
        "method": fuel_source or "UNAVAILABLE",
        "estimated": fuel_source != "PID_5E_DIRECT",
        "status": "ACTIVE" if end_ms and int(time.time() * 1000) - end_ms <= 60000 else "STALE",
        "trip_start": start_ms or None,
        "trip_end": end_ms or None,
        "duration_seconds": round(duration_seconds, 1),
        "distance_km": round(distance_km, 3),
        "fuel_used_l": round(fuel_l, 3),
        "average_l_per_100km": round(l_per_100km, 2) if l_per_100km is not None else None,
        "average_km_per_l": round(km_per_l, 2) if km_per_l is not None else None,
        "instant_fuel_rate_l_h": round(latest_rate, 2) if latest_rate is not None else None,
        "instant_l_per_100km": (
            round(instant_l_per_100km, 2) if instant_l_per_100km is not None else None
        ),
        "average_speed_kph": round(average_speed, 1),
        "max_speed_kph": round(max_speed, 1),
        "max_engine_rpm": round(max_rpm),
        "max_coolant_c": round(max_coolant, 1) if max_coolant is not None else None,
        "idle_seconds": round(idle_seconds, 1),
        "idle_fuel_l": round(idle_fuel_l, 3),
        "data_coverage_pct": round(coverage, 1),
        "confidence": confidence,
        "valid_intervals": valid_intervals,
        "gap_count": gap_count,
        "records_evaluated": len(records),
        "disclaimer": "Distance and fuel values are estimates from read-only OBD telemetry, not billing or odometer data.",
    }


def _health(latest, telemetry):
    score = 100
    warnings = []
    received_ms = _received_ms(latest)
    age_seconds = (
        max(0, int((int(time.time() * 1000) - received_ms) / 1000))
        if received_ms
        else None
    )
    status = "ONLINE"

    if age_seconds is None:
        score -= 30
        status = "UNKNOWN"
        warnings.append("Missing receive timestamp")
    elif age_seconds > 60:
        score -= 40
        status = "OFFLINE"
        warnings.append("No telemetry for more than 60 seconds")
    elif age_seconds > 15:
        score -= 15
        status = "STALE"
        warnings.append("Telemetry is stale")

    coolant = telemetry.get("coolant_c")
    if isinstance(coolant, (int, float)):
        if coolant > 115:
            score -= 30
            warnings.append("Coolant temperature critical")
        elif coolant > 105:
            score -= 15
            warnings.append("Coolant temperature high")

    fuel = telemetry.get("fuel_level_pct")
    if isinstance(fuel, (int, float)):
        if fuel < 10:
            score -= 20
            warnings.append("Fuel level critical")
        elif fuel < 20:
            score -= 10
            warnings.append("Fuel level low")

    engine_load = telemetry.get("engine_load_pct")
    if isinstance(engine_load, (int, float)) and engine_load > 85:
        score -= 5
        warnings.append("Engine load high")

    # Fuel trims are diagnostic context, not a fault diagnosis by themselves.
    # Alert only at large deviations; all supported values remain visible in UI.
    short_trim = telemetry.get("short_fuel_trim_b1_pct")
    long_trim = telemetry.get("long_fuel_trim_b1_pct")
    if isinstance(long_trim, (int, float)) and abs(long_trim) > 25:
        score -= 10
        warnings.append("Long-term fuel trim outside ±25%")
    elif (
        isinstance(short_trim, (int, float))
        and isinstance(long_trim, (int, float))
        and abs(short_trim + long_trim) > 25
    ):
        score -= 5
        warnings.append("Combined fuel trim outside ±25%")

    voltage = telemetry.get("control_module_voltage_v")
    rpm = telemetry.get("engine_rpm")
    if isinstance(voltage, (int, float)) and isinstance(rpm, (int, float)) and rpm > 400:
        if voltage < 12.5:
            score -= 20
            warnings.append("Charging voltage low while engine is running")
        elif voltage > 15.2:
            score -= 15
            warnings.append("Charging voltage high")

    dtcs = []
    for key in ("stored_dtc", "pending_dtc", "permanent_dtc"):
        cleaned = _valid_dtc_codes(telemetry.get(key))
        telemetry[key] = cleaned
        dtcs.extend(cleaned)
    dtcs = list(dict.fromkeys(dtcs))
    if dtcs:
        score -= min(35, 10 + len(dtcs) * 5)
        warnings.append(f"{len(dtcs)} diagnostic trouble code(s) detected")

    link_state = str(telemetry.get("vehicle_link_state", "")).upper()
    if link_state in ("STALE", "OFFLINE"):
        status = link_state
        score -= 25 if link_state == "OFFLINE" else 10
        warnings.append(
            "Vehicle OBD link offline" if link_state == "OFFLINE"
            else "Vehicle OBD data stale"
        )

    score = max(0, min(100, score))
    level = (
        "GOOD"
        if score >= 90
        else "WARNING"
        if score >= 70
        else "DEGRADED"
        if score >= 40
        else "CRITICAL"
    )
    return {
        "status": status,
        "health_score": score,
        "health_level": level,
        "last_seen_seconds": age_seconds,
        "warnings": warnings,
        "dtc_codes": dtcs,
        "vehicle_link_state": link_state or status,
    }


def _number(value):
    return float(value) if isinstance(value, (int, float)) else None


def _series(records, name):
    values = []
    for record in reversed(records):
        value = _number(_data(record.get("data")).get(name))
        if value is not None:
            values.append((_received_ms(record), value))
    return values


def _confidence(sample_count):
    if sample_count >= 20:
        return "HIGH"
    if sample_count >= 8:
        return "MEDIUM"
    return "LOW"


def _finding(system, label, status, evidence, recommendation, sample_count):
    return {
        "system": system,
        "label": label,
        "status": status,
        "confidence": _confidence(sample_count),
        "evidence": evidence,
        "recommendation": recommendation,
        "sample_count": sample_count,
    }


def _predictive_maintenance(records, telemetry, health):
    findings = []

    coolant_values = _series(records, "coolant_c")
    coolant = _number(telemetry.get("coolant_c"))
    hot_count = sum(1 for _, value in coolant_values if value > 105)
    coolant_rise = (
        coolant_values[-1][1] - coolant_values[0][1]
        if len(coolant_values) >= 5
        else 0
    )
    if coolant is not None and coolant > 115:
        cooling_status = "INSPECT_NOW"
        cooling_evidence = f"Coolant is {coolant:.1f} C, above the 115 C critical threshold."
        cooling_action = "Stop safely if temperature continues rising and arrange immediate cooling-system inspection."
    elif hot_count >= 3:
        cooling_status = "SERVICE_SOON"
        cooling_evidence = f"{hot_count} of {len(coolant_values)} recent samples exceeded 105 C."
        cooling_action = "Inspect coolant level, leaks, radiator airflow, thermostat and fan operation."
    elif coolant is not None and (coolant > 105 or (coolant > 95 and coolant_rise >= 10)):
        cooling_status = "OBSERVE"
        cooling_evidence = f"Coolant is {coolant:.1f} C; recent change is {coolant_rise:+.1f} C."
        cooling_action = "Monitor the next drive and inspect if the temperature repeats or continues rising."
    else:
        cooling_status = "NORMAL" if coolant is not None else "INSUFFICIENT_DATA"
        cooling_evidence = (
            f"Latest coolant is {coolant:.1f} C with no repeated high-temperature pattern."
            if coolant is not None else "No coolant samples are available yet."
        )
        cooling_action = "Continue normal monitoring." if coolant is not None else "Collect live OBD data with the engine warm."
    findings.append(_finding(
        "cooling", "Cooling system", cooling_status, cooling_evidence,
        cooling_action, len(coolant_values)
    ))

    voltage_values = _series(records, "control_module_voltage_v")
    voltage = _number(telemetry.get("control_module_voltage_v"))
    rpm = _number(telemetry.get("engine_rpm"))
    abnormal_voltage = sum(
        1 for _, value in voltage_values if value < 12.5 or value > 15.2
    )
    engine_running = rpm is not None and rpm > 400
    if engine_running and voltage is not None and (voltage < 11.8 or voltage > 15.5):
        charging_status = "INSPECT_NOW"
        charging_evidence = f"Module voltage is {voltage:.2f} V while engine speed is {rpm:.0f} rpm."
        charging_action = "Inspect the battery, alternator, belt and electrical connections promptly."
    elif engine_running and abnormal_voltage >= 3:
        charging_status = "SERVICE_SOON"
        charging_evidence = f"{abnormal_voltage} of {len(voltage_values)} voltage samples were outside 12.5-15.2 V."
        charging_action = "Perform a battery and charging-system load test."
    elif engine_running and voltage is not None and (voltage < 12.5 or voltage > 15.2):
        charging_status = "OBSERVE"
        charging_evidence = f"Latest running voltage is {voltage:.2f} V."
        charging_action = "Repeat the measurement with stable idle and electrical loads switched on."
    else:
        charging_status = "NORMAL" if voltage is not None else "INSUFFICIENT_DATA"
        charging_evidence = (
            f"Latest module voltage is {voltage:.2f} V; no persistent charging pattern detected."
            if voltage is not None else "No control-module voltage samples are available yet."
        )
        charging_action = "Continue normal monitoring." if voltage is not None else "Collect PID 0x42 with engine off and running."
    findings.append(_finding(
        "charging", "Battery & charging", charging_status, charging_evidence,
        charging_action, len(voltage_values)
    ))

    short_trim = _number(telemetry.get("short_fuel_trim_b1_pct"))
    long_trim = _number(telemetry.get("long_fuel_trim_b1_pct"))
    trim_samples = max(
        len(_series(records, "short_fuel_trim_b1_pct")),
        len(_series(records, "long_fuel_trim_b1_pct")),
    )
    combined_trim = (
        short_trim + long_trim
        if short_trim is not None and long_trim is not None
        else None
    )
    if (long_trim is not None and abs(long_trim) > 25) or (
        combined_trim is not None and abs(combined_trim) > 25
    ):
        fuel_status = "SERVICE_SOON"
        fuel_evidence = f"STFT={short_trim if short_trim is not None else 'N/A'}%, LTFT={long_trim if long_trim is not None else 'N/A'}%."
        fuel_action = "Check for intake leaks, fuel-pressure issues and sensor bias with a qualified scan tool."
    elif (long_trim is not None and abs(long_trim) > 15) or (
        combined_trim is not None and abs(combined_trim) > 15
    ):
        fuel_status = "OBSERVE"
        fuel_evidence = f"Fuel-trim deviation is elevated; combined trim is {combined_trim:.1f}%." if combined_trim is not None else f"LTFT is {long_trim:.1f}%."
        fuel_action = "Compare hot-idle and steady-cruise trims before scheduling inspection."
    else:
        fuel_status = "NORMAL" if trim_samples else "INSUFFICIENT_DATA"
        fuel_evidence = (
            "Available fuel-trim values are within the configured observation band."
            if trim_samples else "Fuel-trim PIDs have not been observed."
        )
        fuel_action = "Continue trend monitoring." if trim_samples else "Collect STFT and LTFT after closed-loop operation begins."
    findings.append(_finding(
        "fuel_air", "Fuel & air", fuel_status, fuel_evidence,
        fuel_action, trim_samples
    ))

    dtcs = health.get("dtc_codes", [])
    dtc_status = "INSPECT_NOW" if len(dtcs) >= 3 else "SERVICE_SOON" if dtcs else "NORMAL"
    dtc_evidence = (
        f"{len(dtcs)} active code(s): {', '.join(dtcs[:5])}."
        if dtcs else "No stored, pending or permanent DTCs were reported."
    )
    dtc_action = (
        "Record freeze-frame data and arrange diagnosis; do not clear codes before inspection."
        if dtcs else "Continue read-only diagnostic monitoring."
    )
    findings.append(_finding(
        "diagnostics", "Diagnostics", dtc_status, dtc_evidence,
        dtc_action, max(1, len(records))
    ))

    errors = _number(telemetry.get("obd_error_count")) or 0
    successes = _number(telemetry.get("obd_success_count")) or 0
    total_requests = errors + successes
    error_rate = errors / total_requests if total_requests else 0
    link_state = str(health.get("vehicle_link_state", "UNKNOWN")).upper()
    if link_state == "OFFLINE":
        data_status = "INSPECT_NOW"
    elif link_state == "STALE" or (total_requests >= 20 and error_rate > 0.10):
        data_status = "SERVICE_SOON"
    elif total_requests >= 20 and error_rate > 0.03:
        data_status = "OBSERVE"
    else:
        data_status = "NORMAL" if records else "INSUFFICIENT_DATA"
    data_evidence = (
        f"OBD link={link_state}; request error rate={error_rate * 100:.1f}% "
        f"({int(errors)}/{int(total_requests)})."
    )
    data_action = (
        "Inspect the OBD connector, ignition state, CAN link and client power."
        if data_status in ("INSPECT_NOW", "SERVICE_SOON")
        else "Watch connector stability on the next drive."
        if data_status == "OBSERVE"
        else "No data-quality action required."
    )
    findings.append(_finding(
        "data_quality", "OBD data quality", data_status, data_evidence,
        data_action, len(records)
    ))

    rank = {
        "INSUFFICIENT_DATA": 0,
        "NORMAL": 1,
        "OBSERVE": 2,
        "SERVICE_SOON": 3,
        "INSPECT_NOW": 4,
    }
    overall = max(findings, key=lambda item: rank[item["status"]])["status"]
    actionable = sum(1 for item in findings if rank[item["status"]] >= 2)
    oldest_ms = _received_ms(records[-1]) if records else 0
    newest_ms = _received_ms(records[0]) if records else 0
    observed_minutes = max(0, round((newest_ms - oldest_ms) / 60000, 1))

    return {
        "method": "EXPLAINABLE_RULES_V1",
        "overall_status": overall,
        "actionable_systems": actionable,
        "records_evaluated": len(records),
        "observed_minutes": observed_minutes,
        "baseline": {
            "status": "COLLECTING",
            "recommended_days": 14,
            "message": "Collect at least 2-4 weeks of comparable trips before enabling vehicle-specific drift thresholds.",
        },
        "findings": findings,
        "disclaimer": "Condition indicators are screening aids, not a diagnosis. Confirm faults with the Nissan service procedure and a qualified technician.",
    }


def lambda_handler(event, context):
    try:
        device_id = _device_id(event)
        route_key, path = _route(event)

        if route_key.startswith("OPTIONS "):
            return _response(204, {})
        if path in ("/", "/v1", "/v1/"):
            return _file_response("index.html", "text/html; charset=utf-8")
        if "/assets/" in path:
            asset = path.rsplit("/", 1)[-1]
            allowed = {
                "style.css": "text/css; charset=utf-8",
                "app.js": "application/javascript; charset=utf-8",
                "config.js": "application/javascript; charset=utf-8",
            }
            if asset not in allowed:
                return _response(404, {"status": "NOT_FOUND"})
            return _file_response(asset, allowed[asset])

        if path.endswith("/history"):
            query = event.get("queryStringParameters") or {}
            try:
                minutes = max(1, min(1440, int(query.get("minutes", "60"))))
            except (TypeError, ValueError):
                minutes = 60
            start_ms = int(time.time() * 1000) - (minutes * 60 * 1000)
            records = _query(device_id, limit=QUERY_LIMIT, start_ms=start_ms)
            return _response(
                200,
                {
                    "device_id": device_id,
                    "minutes": minutes,
                    "count": len(records),
                    "records": records,
                },
            )

        if path.endswith("/maintenance"):
            records = _query(device_id, limit=MAINTENANCE_QUERY_LIMIT)
            if not records:
                return _response(404, {"status": "NOT_FOUND", "device_id": device_id})
            telemetry, _ = _merge(records)
            health = _health(records[0], telemetry)
            return _response(
                200,
                {
                    "device_id": device_id,
                    "received_at": records[0].get("received_at"),
                    "predictive_maintenance": _predictive_maintenance(
                        records, telemetry, health
                    ),
                },
            )

        if path.endswith("/trip"):
            records = _query(device_id, limit=TRIP_QUERY_LIMIT)
            if not records:
                return _response(404, {"status": "NOT_FOUND", "device_id": device_id})
            return _response(
                200,
                {
                    "device_id": device_id,
                    "received_at": records[0].get("received_at"),
                    "trip_estimate": _trip_estimate(records),
                },
            )

        records = _query(device_id)
        if not records:
            return _response(
                404,
                {
                    "status": "NOT_FOUND",
                    "device_id": device_id,
                    "message": "No telemetry found",
                },
            )

        latest = records[0]
        telemetry, classes = _merge(records)
        health = _health(latest, telemetry)
        body = {
            "device_id": device_id,
            "device_name": latest.get("device_name", device_id),
            "device_type": latest.get("device_type", "obd2_vehicle"),
            "source_mode": latest.get("source_mode", "unknown"),
            "received_at": latest.get("received_at"),
            "records_evaluated": len(records),
            "available_data_classes": list(classes.keys()),
            "telemetry": telemetry,
            **health,
        }

        if path.endswith("/latest"):
            body.pop("health_score", None)
            body.pop("health_level", None)
            body.pop("warnings", None)

        return _response(200, body)
    except Exception as error:
        print("Car health API error:", repr(error))
        return _response(500, {"status": "ERROR", "message": str(error)})
