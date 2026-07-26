# AI-Assisted Features

AI is an optional commercial layer above validated OBD-II telemetry. It should
support a technician or vehicle owner; it must not claim certainty or control
the vehicle.

## Recommended AI Care features

- detect unusual coolant, voltage, fuel-trim, load and data-quality patterns;
- compare the vehicle with its own learned baseline across similar trips;
- summarize DTC evidence in plain language;
- rank maintenance observations by urgency and confidence;
- create a customer-friendly trip or monthly health summary;
- identify missing, stale or contradictory sensor data;
- recommend the next safe diagnostic check.

## Processing model

1. Validate timestamps, units, source mode and PID support.
2. Separate simulator, standard SAE PID and Nissan proprietary evidence.
3. Establish a per-vehicle baseline using at least 2–4 weeks of comparable
   operating data.
4. Run transparent engineering rules before any statistical or AI model.
5. Generate an anomaly score with evidence, sample count and confidence.
6. Present recommendations for review; never issue vehicle control commands.
7. Record model/rule version and user-visible explanation for audit.

## Product language

Use:

- “AI-assisted condition insight”
- “unusual pattern detected”
- “recommended inspection”
- “confidence: low/medium/high”

Avoid:

- “guaranteed failure prediction”
- “the component will fail in X days” without a validated prognostic model
- “manufacturer-approved diagnosis”
- “safe to drive” based only on remote telemetry

## Minimum evidence

An AI result should contain the affected system, observed values, baseline,
time window, data coverage, confidence, applicable DTCs and recommended next
action. If data are stale or unsupported, return `INSUFFICIENT_DATA`.

## Safety boundary

AI features remain read-only. They do not clear DTCs, change ECU parameters,
operate actuators or replace professional inspection.
