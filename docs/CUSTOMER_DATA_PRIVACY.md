# Customer Data and Privacy

Vehicle telemetry can reveal location patterns, driving behavior and vehicle
condition. Treat it as customer-controlled data even when exact GPS is not
collected.

## Required policy

- State what is collected, why it is collected and how long it is retained.
- Obtain explicit consent before sharing data with a garage, fleet manager,
  insurer or other third party.
- Keep customer data private by default.
- Provide account export, consent revocation and deletion workflows.
- Separate customer identity from raw telemetry where practical.
- Encrypt communications and cloud storage.
- Use least-privilege AWS identities and per-environment credentials.
- Record administrative access and sharing changes.
- Never commit customer records, AWS keys, private certificates, SIM
  credentials or production endpoints to this repository.

## Suggested retention

| Plan | Online history |
|---|---:|
| Free | 7 days |
| Car Care | 90 days |
| AI Care | 365 days |
| Fleet | Contract-defined |

Backups and deletion queues need their own documented expiration. Retention
should be configurable where local law or a customer contract requires it.

## Consent record

Store the customer/account ID, vehicle ID, policy version, purposes granted,
sharing recipients, timestamp and revocation timestamp. Do not infer consent
from hardware purchase or dashboard use.

## Repository boundary

Public source should contain sample payloads with fictional identifiers only.
Production configuration belongs in a secrets manager or protected deployment
environment.
