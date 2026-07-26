# Commercial Offering

This document describes a proposed commercial packaging model for the
Connected Vehicle Health Platform. Prices are planning figures in Thai baht
(THB), not binding quotations, and should be reviewed against hardware,
installation, SIM, cloud and support costs before launch.

## Product package

| Item | Indicative price | Included |
|---|---:|---|
| ESP32 OBD-II client | THB 1,500 | Read-only standard OBD-II acquisition, Wi-Fi and TCP telemetry |
| LILYGO 4G gateway package | THB 3,500 | LTE gateway hardware, initial SIM and gateway firmware |
| Vehicle Health Starter Kit — Early Bird | THB 4,900 | OBD-II client, 4G gateway, onboarding and 15-day web trial |

Shipping, vehicle installation, taxes, replacement parts and ongoing mobile
data are quoted separately unless explicitly included by the seller.

### Early-bird launch offer

The THB 4,900 Starter Kit price is an early-bird promotional offer scheduled
to open on **1 October 2026**. It is not the standard list price. Availability,
quantity, eligibility, promotional end date, shipping, installation and final
terms must be confirmed before orders are accepted. The 15-day web trial still
starts only after the first valid cloud telemetry record is received.

## Commercial principles

1. Sell hardware once and charge recurring fees for cloud operations, history,
   support and advanced analytics.
2. Keep a useful free tier so customers retain access to essential vehicle
   health and diagnostic information.
3. Make all vehicle access read-only. Never advertise remote actuation, DTC
   clearing or ECU programming as part of this product.
4. Describe AI features as decision support, not guaranteed fault prediction.
5. Make data retention, ownership, consent and deletion rules visible before a
   customer begins the trial.

## Suggested customer journey

1. Customer purchases the Starter Kit.
2. Installer assigns the device to the customer's vehicle and account.
3. The 15-day trial starts after the first valid cloud telemetry record.
4. The customer sees current health, live trends, DTCs, trip estimates and
   maintenance guidance.
5. Before expiry, the application presents plan options without disabling
   access to safety-relevant information.
6. After expiry, the account moves to the Free plan unless the customer
   explicitly subscribes.

See [Subscription Plans](docs/SUBSCRIPTION_PLANS.md),
[AI Features](docs/AI_FEATURES.md), and
[Customer Data and Privacy](docs/CUSTOMER_DATA_PRIVACY.md).

## Important disclaimer

This platform is an informational, read-only monitoring product. It does not
replace Nissan service procedures, a qualified technician, regulatory vehicle
inspection or an approved diagnostic scanner. Estimated fuel, distance and
maintenance values must not be used for billing, taxation or warranty claims.
