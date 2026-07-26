# Operating Limits and Validation Envelope

This repository is a reference implementation built around an ESP32 OBD-II
client, a MicroPython ESP32/A7670 gateway, and AWS managed services. Its limits
must be established by testing the exact hardware, cellular operator, vehicle,
firmware build, and cloud configuration used in deployment.

## Safety boundary

- Vehicle access is read-only.
- Only OBD-II Mode 01 live-data requests and Mode 03 DTC reads are enabled.
- DTC clearing, ECU coding, actuator tests, firmware programming, and CAN frame
  injection are outside the supported scope.
- The system is not a replacement for an approved workshop diagnostic tool.
- Installation must not obstruct pedals, steering, airbags, or the driver's
  view. Disconnect the prototype if it becomes hot, unstable, or affects the
  vehicle network.

## Supported data

The standard firmware discovers ECU-supported Mode 01 PIDs before polling.
Availability therefore depends on the vehicle. Unsupported values are reported
as unavailable rather than fabricated. Nissan CONSULT data is documented as a
separate research phase and is not enabled by default.

## Performance envelope

The design separates acquisition rates into fast, slow, event, and diagnostic
classes. It acknowledges valid TCP JSON before cellular publication and uses a
bounded asynchronous MQTT queue. This protects local acquisition from normal
LTE latency, but it does not provide unlimited buffering.

Before declaring a production limit, measure:

| Metric | Minimum evidence |
|---|---|
| TCP acceptance | ACK success rate and latency at the intended message rate |
| Queue stability | Maximum and steady-state queue depth |
| Memory | Minimum free heap and absence of a downward trend |
| LTE/MQTT | Reconnect count, outage duration, publish success |
| Cloud | IoT Rule failures, DynamoDB throttles, API error rate |
| Vehicle bus | No induced DTCs, warnings, or driveability changes |
| Recovery | Ignition off/on, AP reconnect, LTE loss, and power-cycle recovery |

## Qualification sequence

1. Bench simulator: 10 minutes.
2. Stationary vehicle, ignition on: 10–15 minutes.
3. Stationary engine idle: 15 minutes.
4. Controlled road test: 30–60 minutes with a second person observing.
5. Two-hour integration test.
6. Twenty-four-hour soak test on bench or parked vehicle with safe power.
7. Repeat after every firmware, modem, queue, or sampling-policy change.

Record firmware hashes, configuration, SIM operator, signal strength, ambient
temperature, load profile, and all result CSV files. A pass requires no
unexplained reset, no unbounded queue growth, no safety impact, and compliance
with the acceptance criteria in [TEST_PLAN.md](TEST_PLAN.md).

## Commercialization gaps

Additional work is required for authentication and tenant isolation, signed OTA,
secure manufacturing provisioning, privacy consent, fleet management, alert
delivery, retention policies, billing controls, regional compliance, enclosure
qualification, automotive power conditioning, and formal cybersecurity review.
