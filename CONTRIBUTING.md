# Contributing

1. Open an issue describing the vehicle, ECU, PID and expected behavior.
2. Keep all vehicle communication read-only.
3. Add decode equations, units, validity ranges and a captured test fixture.
4. Run the validation steps in `docs/TEST_PLAN.md`.
5. Never include vehicle identifiers, credentials, certificates or customer
   telemetry in commits.

Pull requests that introduce write services, unsafe CAN injection or hidden
credentials will not be accepted.
