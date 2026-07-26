# Vehicle Health Interconnection Reference

This document defines how the read-only Vehicle Health demonstration moves
data from a vehicle ECU to the cloud HMI. It is an implementation reference,
not an automotive safety certification.

## End-to-end topology

```text
Vehicle ECU(s)
  OBD-II connector / ISO 15765-4 CAN
  11-bit identifiers, normally 500 kbit/s
        |
        v
ESP-OBD2 client
  ESP32 + SN65HVD230, GPIO26 TX / GPIO27 RX
  Mode 01 PID polling + Mode 03/07/0A DTC read-only
        |
        v
Private Wi-Fi: IIOT-GW-01 (192.168.4.0/24)
  ESP-OBD2 client: DHCP address, normally 192.168.4.2
  LILYGO gateway: 192.168.4.1
        |
        v
TCP port 5005
  One newline-delimited JSON document per message
  Gateway response: {"status":"OK"}
        |
        v
Bounded asynchronous MQTT queue
  TCP ACK is independent from LTE/AWS delivery latency
        |
        v
A7670 LTE Cat-1 modem
  TLS MQTT connection to AWS IoT Core
        |
        v
AWS IoT Core -> IoT Rule -> DynamoDB
        |
        v
Lambda -> API Gateway -> CloudFront/S3 HMI
```

## Physical vehicle interface

The board is powered from the vehicle OBD-II connector and communicates over
CAN High/CAN Low. Before connection:

1. Remove the external programming adapter.
2. Confirm the board uses 3.3 V logic and the fitted CAN transceiver.
3. Keep optional 120-ohm termination disabled on a production vehicle. The
   vehicle bus is already terminated.
4. Connect with ignition off, then switch ignition on while stationary.

The client transmits only functional diagnostic requests to CAN ID `0x7DF`
and accepts ECU replies in the `0x7E8` through `0x7EF` range. It does not send
actuator commands, clear DTCs, perform ECU coding, or inject arbitrary frames.

## Private Wi-Fi segment

| Item | Value |
|---|---|
| Gateway mode | ESP32 SoftAP |
| Default gateway address | `192.168.4.1/24` |
| Client addressing | DHCP |
| Default TCP destination | `192.168.4.1:5005` |
| Internet path | Gateway LTE modem, not Wi-Fi routing |

The private AP separates the vehicle client from the home or workshop LAN.
Change the SSID and WPA2 password before deployment. Do not commit credentials
to Git. The dashboard and WebREPL management interfaces should not be exposed
directly to the public Internet.

## TCP application contract

The ESP32 opens a TCP connection to port 5005 and sends compact UTF-8 JSON.
Each document is terminated by a newline. The gateway:

1. applies a maximum message size;
2. parses and validates JSON;
3. records device/statistical state;
4. copies the accepted message into a bounded MQTT queue;
5. returns `{"status":"OK"}` immediately; and
6. closes or reuses the connection according to client behavior.

An ACK means the gateway accepted the record into local processing. It does
not guarantee that AWS has stored it. Cloud delivery is observed separately
through MQTT counters, queue depth, AWS IoT logs, and DynamoDB timestamps.

Clients must use a timeout and reconnect with bounded exponential backoff.
Sequence numbers and `message_id` make missing or duplicate records visible.

## MQTT and Unified Namespace

Example topics:

```text
dt/iiot-lab/factory1/vehicle/obd2-vehicle/car-01-obd/telemetry/fast
dt/iiot-lab/factory1/vehicle/obd2-vehicle/car-01-obd/telemetry/slow
dt/iiot-lab/factory1/vehicle/obd2-vehicle/car-01-obd/event
dt/iiot-lab/factory1/vehicle/obd2-vehicle/car-01-obd/diagnostic
```

Topic elements identify message direction, namespace, site, area, asset type,
device ID, and data class. AWS IoT policies should grant the gateway only the
specific publish/subscribe topic ARNs it needs.

## Timing and buffering

Fast-changing measurements are published separately from slow values. This
prevents coolant or fuel level from being repeated at the RPM sampling rate.
The client samples first, applies validity/deadband rules, then publishes the
appropriate class.

The gateway's bounded queue decouples local TCP response time from LTE jitter.
When LTE is slower than incoming traffic, queue depth rises. A full queue must
increment a drop/rejection counter rather than allocate memory without limit.
Recommended operating checks are:

- TCP ACK success at least 99.5% in the laboratory baseline;
- no unexpected gateway restart;
- minimum free heap at least 28 KB;
- MQTT dropped/failed count of zero at the validated workload;
- sequence-gap and DynamoDB arrival monitoring; and
- stale/offline state if cloud data stops updating.

## AWS data path

AWS IoT Core authenticates the gateway with an X.509 device certificate. An
IoT Rule selects telemetry topics and writes each message to DynamoDB using:

- partition key: `device_id`;
- sort key: `received_at` in Unix epoch milliseconds; and
- payload attributes including `data_class`, `data`, sequence, and identity.

`received_at` is generated in AWS and remains authoritative when the gateway
runs in AP-only mode without NTP. The Lambda API queries newest records,
merges fast/slow/diagnostic classes, calculates demonstration indicators, and
serves JSON to the browser. The browser never receives AWS credentials.

## Failure behavior

| Failure | Expected behavior |
|---|---|
| ECU does not support a PID | Omit value; mark unsupported, never publish zero |
| Vehicle ignition off | Client reports link loss when possible; HMI becomes stale/offline |
| Wi-Fi interruption | Client reconnects with backoff; sequence gap remains observable |
| LTE interruption | TCP continues while bounded queue has capacity |
| MQTT disconnect | Gateway reconnects; failure/drop counters remain visible |
| Duplicate MQTT delivery | `device_id`, `sequence`, and `message_id` support detection |
| Lambda/API failure | HMI shows connection/error state without inventing values |
| Old DynamoDB record | Data-age logic marks STALE/OFFLINE |

## Security boundary

- OBD operation is read-only.
- TLS terminates at AWS IoT Core using per-gateway certificates.
- Private keys stay on the gateway and outside this repository.
- API Gateway exposes read-only vehicle views; production deployments should
  add authentication, authorization, throttling, WAF rules, and audit logs.
- DynamoDB uses IAM least privilege and encryption at rest.
- Dashboard values are advisory and must not replace the instrument cluster,
  manufacturer diagnostics, or qualified maintenance decisions.
