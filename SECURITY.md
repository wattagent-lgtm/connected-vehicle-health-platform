# Security and Vehicle-Safety Policy

## Supported scope

This repository is a read-only telemetry reference. It requests diagnostic
data and must never be used for braking, steering, throttle, airbag, immobilizer
or other safety-critical control.

## Non-negotiable boundaries

- Do not add DTC clear, ECU coding, actuator tests or arbitrary CAN transmit.
- Do not commit Wi-Fi passwords, AWS credentials, certificates or private keys.
- Use a dedicated AWS IoT certificate and least-privilege policy per gateway.
- Keep the gateway access point private and change all example passwords.
- Disconnect the OBD adapter during long-term parking until sleep-current
  behavior has been measured on the actual vehicle.
- Validate proprietary Nissan/CONSULT requests on a bench before vehicle use.

## Reporting a vulnerability

Do not open a public issue for a credential leak or exploitable vulnerability.
Contact the repository owner privately through the GitHub profile. Include the
affected component, reproduction steps and expected impact. Revoke exposed
credentials immediately.
