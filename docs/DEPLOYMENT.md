# Deployment Guide

## Prerequisites

- ESP-IDF 5.5.x and esptool
- MicroPython for ESP32 and `mpremote`
- AWS CLI v2 and AWS SAM CLI
- AWS account in one selected region
- AWS IoT Thing, active certificate and least-privilege policy
- 4G SIM/APN compatible with the A7670 variant

## AWS security setup

Use an IAM deployment user/role, not account-root credentials. The gateway IoT
policy should permit:

- connect only as its assigned client ID;
- publish only to its vehicle telemetry prefix;
- subscribe/receive only to its command topic if commands are enabled.

The vehicle firmware does not require AWS credentials.

## IoT rule

Create a rule matching the telemetry namespace, for example:

```sql
SELECT *, timestamp() AS received_at
FROM 'dt/iiot-lab/factory1/vehicle/#'
```

Target the DynamoDB table created by SAM and map `device_id` plus
`received_at`. Add a CloudWatch error action before production use.

## SAM deployment

```powershell
cd aws-dashboard
sam validate --lint
sam build
sam deploy --guided
```

Record the API output. The bundled frontend defaults to same-origin API calls.
If hosted separately, set `frontend/config.js` `apiBaseUrl` to the stage URL,
then deploy the static files through the selected hosting/CDN.

## Field commissioning

1. Run the OBD client in simulator mode.
2. Verify TCP ACK, MQTT topic, DynamoDB records and dashboard.
3. Bench-test CAN with a protected supply.
4. Verify read-only CAN behavior using a CAN analyzer.
5. Install in vehicle with ignition off.
6. Start ignition, compare values with a trusted scan tool.
7. Complete the staged tests in `TEST_PLAN.md`.
