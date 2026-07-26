# Verification and Soak-Test Plan

## Acceptance stages

### A. Static and simulator

- Firmware builds without warnings promoted to errors.
- Simulator sends all four message classes.
- TCP ACK success >= 99.5% for a 30-minute baseline.
- Dashboard handles supported, unsupported, stale and API-error fixtures.

### B. Bench CAN

- Correct bitrate and GPIO mapping.
- Only tester requests from the approved allow-list are transmitted.
- No DTC clear, actuator, coding or arbitrary frame path exists.
- Power interruption and ignition recovery complete without manual reset.

### C. Vehicle validation

- Compare RPM, speed, coolant, throttle, load and intake air with a trusted
  scanner and instrument cluster where applicable.
- Unsupported PIDs show `N/S`.
- Read Mode 03 DTCs without clearing them.
- Test ignition off, restart and Wi-Fi/LTE recovery.

### D. Reliability

Run 30 minutes, 2 hours and then 24 hours at the intended workload. Capture:

- TCP sent/ACK/failure and latency percentiles;
- gateway uptime, restarts, minimum free heap and queue high-water mark;
- LTE registration and MQTT reconnects;
- AWS records expected versus stored;
- dashboard API availability and stale/offline correctness.

## Suggested 24-hour gate

- No unplanned reset or unrecovered service outage.
- TCP ACK >= 99.9% at the declared operating rate.
- Cloud delivery >= 99.5%, with documented queue/drop behavior.
- Free heap has no sustained downward trend.
- No safety-boundary violation.

These are project acceptance targets, not automotive certification.
