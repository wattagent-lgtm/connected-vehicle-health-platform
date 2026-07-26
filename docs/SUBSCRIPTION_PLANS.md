# Subscription Plans

The following prices are an initial market-test proposal per vehicle. Confirm
AWS, cellular, payment, tax and customer-support costs before publishing them.

| Capability | Free | Car Care | AI Care |
|---|---:|---:|---:|
| Indicative monthly price | THB 0 | THB 299 | THB 499 |
| Trial | 15 days of AI Care | — | — |
| Current vehicle health | Yes | Yes | Yes |
| Standard read-only DTC display | Yes | Yes | Yes |
| Data history | 7 days | 90 days | 365 days |
| Trip and fuel estimation | Summary | Full | Full |
| Maintenance planner | Basic | Full | Full |
| Downloadable reports | No | Yes | Yes |
| Rule-based condition monitoring | Basic | Advanced | Advanced |
| AI-assisted anomaly detection | No | No | Yes |
| AI maintenance summary | No | No | Yes |
| Priority support | No | Standard | Priority |

## Fleet plan

Fleet pricing should be quoted separately and can add:

- multi-vehicle dashboard and vehicle groups;
- driver/vehicle utilization reports;
- maintenance workflow and workshop sharing with explicit consent;
- organization roles, audit records and API access;
- configurable data retention and volume discounts.

## Trial and billing rules

- Begin the 15-day trial only after the first valid vehicle telemetry reaches
  the cloud.
- Show the trial end date and current plan in the dashboard.
- Do not automatically charge without explicit customer consent.
- Send reminders before expiry.
- On expiry, downgrade to Free without deleting customer data immediately.
- Publish cancellation, refund, retention and hardware-warranty terms.
- Meter high-volume exports or unusually frequent cloud polling separately if
  they create material infrastructure cost.

## Unit economics checklist

For each active vehicle, estimate:

`monthly margin = subscription revenue - cloud - SIM - payment fees - support - warranty reserve`

Track AWS IoT messages, DynamoDB writes/storage/reads, API requests, dashboard
traffic, cellular usage, support time and replacement rate. Pricing is not
validated until these costs are measured in a representative pilot.
