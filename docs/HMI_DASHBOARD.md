# Vehicle Health HMI Dashboard Reference

## Purpose

The HMI gives an operator a single-page, read-only view of vehicle condition,
connectivity, recent behavior, diagnostic evidence, and estimated trip/fuel
performance. It is optimized for post-trip and passenger/technician viewing;
the driver must not interact with it while driving.

## Information hierarchy

The cockpit layout follows a three-level hierarchy:

1. **Can I trust the data?** Connection state, source mode, data age, OBD link,
   AWS status, and last update.
2. **Does the vehicle need attention?** Overall level, warning count, DTC
   state, critical temperatures, voltage, and maintenance findings.
3. **What evidence explains it?** Gauges, numeric cards, trends, DTC lists,
   trip estimates, and diagnostic counters.

Color is supplemental, never the only indicator:

- green: normal/connected/clear;
- amber: stale, warning, uncertain estimate, or attention required;
- red: offline, critical, or diagnostic fault;
- muted/`--`: unavailable or unsupported.

## API contract

The static frontend calls these read-only routes:

| Route | Dashboard use |
|---|---|
| `GET /api/cars/{device_id}/health` | merged live values, health level, warnings, maintenance findings |
| `GET /api/cars/{device_id}/latest` | newest source record and metadata |
| `GET /api/cars/{device_id}/history?minutes=60` | time-series trend |
| `GET /api/cars/{device_id}/trip` | distance, fuel, economy, coverage, confidence |

API Gateway invokes Lambda. Lambda queries DynamoDB by `device_id`, ordered by
`received_at`, converts Decimal values to JSON numbers, parses nested payloads,
and merges the newest fast, slow, and diagnostic classes.

The browser receives no AWS keys. `frontend/config.js` contains only the
public API base URL and vehicle ID.

## Header and trust state

The header displays vehicle identity, data source (`SIMULATOR` or `OBD2_CAN`),
last update, and overall status. Data age is calculated from AWS
`received_at`, not the client clock.

Current backend behavior:

- recent data: `ONLINE`;
- aged data: `STALE`;
- no telemetry for more than 60 seconds: `OFFLINE`.

When data is stale/offline, gauges must not imply that the last values are
live. The HMI preserves last-known values only with an obvious age/state
label.

## Cockpit gauges and parameter cards

Primary gauges emphasize RPM and road speed because they change quickly and
help verify live acquisition. Cards provide exact values for:

- engine RPM and vehicle speed;
- coolant and intake-air temperatures;
- engine load and throttle;
- fuel level;
- control-module voltage;
- short/long fuel trims;
- manifold pressure and MAF;
- engine runtime and supported diagnostic metadata.

The frontend must render `0` as a valid reading and reserve `--` for missing
data. Units are always shown. Values should be range checked before display.

## Trend chart

The trend panel uses recent history rather than extrapolated values. Signals
with different units are normalized for a compact overview; tooltips and
cards retain engineering units. A production analytics view should use
separate axes/panels where exact correlation matters.

Trend rendering should:

- sort by `received_at`;
- limit points to protect browser performance;
- preserve gaps rather than connect long missing periods;
- label the selected time window;
- distinguish data classes; and
- show an empty-state explanation if history is unavailable.

## Health assessment

The demonstration score starts from 100 and applies deductions for evidence
such as stale/offline link, high coolant, low fuel, abnormal sustained fuel
trim, charging voltage outside example bounds, or DTC presence. The result is
mapped to a textual level such as `GOOD`, `WARNING`, or `CRITICAL`.

The score is a UI prioritization mechanism, not a probability of failure.
Warnings and raw evidence are more important than the number. Manufacturer
limits and technician judgment take precedence.

## Predictive-maintenance panel

Predictive indicators are deterministic condition-monitoring rules based on
recent samples. They cover areas such as:

- cooling-system temperature behavior;
- fuel-control trim behavior;
- charging-voltage behavior;
- DTC/MIL evidence; and
- acquisition/link quality.

Each finding contains:

- subsystem and status;
- evidence in engineering units;
- recommended next action;
- sample count; and
- confidence based on available history.

The dashboard should call this **predictive indication** until sufficient
vehicle-specific history and labeled failures exist for a validated
statistical/ML model. Collect 2-4 weeks of comparable healthy trips before
enabling drift thresholds.

## DTC presentation

Stored, pending, and permanent DTCs are displayed separately. The HMI shows
MIL state and collection time and recommends confirmation with a qualified
scanner. It never offers a clear-code button.

Useful technician workflow:

1. note code type and timestamp;
2. review related temperatures, trims, load, and voltage;
3. reproduce the condition safely;
4. confirm using manufacturer service data; and
5. record repair and follow-up trip evidence.

## Trip and fuel panel

Trip cards show estimated distance, fuel used, `L/100 km`, `km/L`, method,
coverage, and confidence. The HMI must suppress consumption ratios when
distance/fuel coverage is insufficient and explicitly label all results as
estimates.

This separates customer-friendly monitoring from future paid services:

- free view: current health, DTC visibility, basic trip summary;
- maintenance service: longer history, evidence reports, reminders;
- fleet service: multi-vehicle comparison, utilization, alerts, exports; and
- analytics service: baseline drift and maintenance optimization.

## Refresh and error behavior

The browser polls the API periodically without reloading the page. Only one
refresh should be active at a time. On timeout or non-2xx response:

1. retain the last-known values;
2. mark the connection state as error/stale;
3. show the time of the last successful update;
4. retry with a bounded interval; and
5. avoid rapidly repeated requests.

The UI must remain usable if history or trip endpoints fail while health data
continues to work.

## Responsive and accessibility requirements

- Fit the operational overview on one desktop screen where practical.
- Stack cards and charts on phone/tablet widths.
- Use readable minimum font sizes and high contrast.
- Provide text labels in addition to color.
- Do not use animation that hides state changes or distracts the driver.
- Support keyboard focus for interactive elements.
- Use ISO/metric units consistently and localize date/time deliberately.

## Production hardening

Before customer use, add:

- user authentication and per-vehicle authorization;
- API throttling, WAF, CORS allow-list, and audit logging;
- tenant isolation in API and DynamoDB access patterns;
- encrypted certificate/credential lifecycle management;
- configurable vehicle-specific thresholds;
- alarm acknowledgement and maintenance history;
- privacy retention/deletion controls for location and driving data;
- monitoring for Lambda errors, API latency, IoT Rule failures, and DynamoDB
  throttling; and
- versioned schemas with backward-compatible frontend behavior.

The current HMI is a strong technical demonstration but not a certified
automotive instrument, safety display, or diagnostic decision system.
