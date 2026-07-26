# OBD-II Vehicle Telemetry Methodology

## Objective

Collect standardized, read-only OBD-II measurements from one vehicle while
preserving the gateway's immediate TCP acknowledgement and asynchronous cloud
publishing model.

## Commissioning stages

### Stage 1: software-only

1. Run the ESP32 client in simulator mode.
2. Confirm TCP ACK success for ten minutes.
3. Confirm MQTT topics in AWS IoT Test Client.
4. Confirm items arrive in DynamoDB.
5. Verify no gateway restart and no MQTT drop/failure.

### Stage 2: bench CAN

1. Confirm the fitted board matches schematic revision 1.0: SN65HVD230,
   TX GPIO26, RX GPIO27.
2. Keep JP1 open when attaching to an already terminated vehicle bus. For an
   isolated bench bus, fit JP1 only when this node must supply one of the two
   120-ohm end terminations.
3. Use a protected CAN simulator or OBD-II ECU simulator.
4. Confirm 500 kbit/s first, then 250 kbit/s if required.
5. Confirm only IDs `0x7DF` are transmitted and only `0x7E8-0x7EF` diagnostic
   responses are decoded.

### Stage 3: stationary vehicle

1. Park outdoors or in a ventilated area.
2. Apply the parking brake and keep the transmission in Park/Neutral.
3. Connect with ignition on and engine off.
4. Confirm supported PIDs and stable CAN response.
5. Start the engine only when the stationary test passes.

### Stage 4: operational trial

Use a passenger to monitor logs. The driver must not operate a laptop or debug
equipment while driving. Compare OBD speed and RPM against the instrument
cluster for plausibility, not calibration certification.

## Data-quality rules

- A timed-out PID is omitted rather than published as zero.
- Every message carries a monotonically increasing sequence and message ID.
- Engineering values are decoded before transmission.
- Fast and slow data are separated so slow parameters do not inflate traffic.
- Deadband is applied after sampling, not before.
- Diagnostic health is separate from vehicle telemetry.

## Ten-minute simulator acceptance

| Metric | Target |
|---|---:|
| Duration | 600 seconds |
| Gateway TCP ACK success | >=99.5% |
| Gateway sampled availability | 100% |
| Unexpected gateway reset | 0 |
| MQTT dropped/failed | 0 |
| Gateway minimum free heap | >=28 KB |
| Average TCP ACK latency | <300 ms |
| Maximum TCP ACK latency | <2 seconds |

## Vehicle plausibility checks

| Signal | Basic check |
|---|---|
| RPM | Near zero with engine stopped; plausible idle after start |
| Speed | Zero while stationary |
| Coolant | Rises gradually after cold start |
| Throttle | Within 0-100% and changes smoothly |
| Engine load | Within 0-100% |
| Fuel level | Changes slowly and remains within 0-100% |

Values outside physical range must be marked invalid and investigated before
cloud analytics are enabled.

## Confirmed vendor hardware

- MCU: ESP32-WROOM-32E
- TWAI TX: GPIO26
- TWAI RX: GPIO27
- Transceiver: SN65HVD230DR at 3.3 V
- Termination: selectable 120 ohms through JP1
- CAN protection: PSM712
- Power: TPS54202 buck, SS210 series protection, SMBJ30CA input TVS
- Programming: external 3.3 V UART uploader using TX0/RX0/IO0/EN header

Before vehicle connection, remove the uploader and verify JP1 is open. The
simulator remains the mandatory first commissioning stage.
