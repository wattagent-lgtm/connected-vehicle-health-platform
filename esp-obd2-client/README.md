# ESP32 OBD-II Wi-Fi Client

Standalone ESP-IDF project for one read-only vehicle telemetry client:

```text
Vehicle OBD-II/CAN -> ESP32 -> Wi-Fi TCP/JSON -> LILYGO Gateway
                    -> LTE MQTT/TLS -> AWS IoT -> DynamoDB
```

This project does not modify the existing gateway. It uses the established TCP
port 5005 protocol and expects `{"status":"OK"}` after every accepted JSON
message.

## Safety boundary

- Simulator mode is enabled by default.
- Vehicle mode sends only standardized OBD-II Mode 01 diagnostic requests.
- Clear-DTC Mode 04, actuator commands, ECU coding, and arbitrary raw CAN
  transmission are not implemented.
- Do not use this project for vehicle control or safety functions.
- The supplied Artron Shop schematic confirms TWAI TX = GPIO26 and RX = GPIO27.
- Leave the onboard 120-ohm termination jumper JP1 open when connected to a
  normal vehicle OBD-II network. Close it only for a bench bus that requires
  this board to provide one end termination.
- Remove the external uploader before connecting the board to the vehicle to
  avoid powering it simultaneously from the uploader and OBD-II connector.
- Unplug the adapter when the vehicle is stored unless the board has verified
  low-power shutdown; OBD-II battery power may remain live with ignition off.

## Data classes

| Class | Default period | Values |
|---|---:|---|
| Fast | 1 second | RPM, speed, engine load, throttle |
| Slow | 5 seconds | coolant, intake temperature, fuel level |
| Diagnostic | 60 seconds | free heap, supported-PID mask, read-only state |

Deadbands suppress values that have not changed enough to justify LTE traffic.

Expected AWS topics after the gateway applies its UNS mapping:

```text
dt/iiot-lab/mobile/vehicle/obd2-vehicle/CAR-01-OBD/telemetry/fast
dt/iiot-lab/mobile/vehicle/obd2-vehicle/CAR-01-OBD/telemetry/slow
dt/iiot-lab/mobile/vehicle/obd2-vehicle/CAR-01-OBD/diagnostic
```

The exact prefix follows the gateway's MQTT topic configuration.

## Prerequisites

- ESP-IDF 5.x
- ESP32-based ESP-OBD2 board
- Onboard SN65HVD230 CAN transceiver (TX GPIO26, RX GPIO27)
- Vehicle using ISO 15765-4 CAN on OBD-II pins 6 and 14
- Existing gateway reachable on Wi-Fi

The ESP32 contains a TWAI/CAN controller but no physical CAN transceiver. The
ESP-OBD2 board must provide that transceiver.

## First test: simulator

Open an ESP-IDF terminal:

```powershell
cd connected-vehicle-health-platform\esp-obd2-client
idf.py set-target esp32
idf.py menuconfig
```

Set:

```text
OBD-II Wi-Fi Client
  Wi-Fi SSID
  Wi-Fi password
  Gateway IPv4 address = 192.168.1.42
  Gateway TCP port = 5005
  Use simulator instead of vehicle CAN = enabled
```

Then:

```powershell
idf.py build
idf.py -p COMx flash monitor
```

Verify:

```powershell
Invoke-RestMethod http://192.168.1.42/api/statistics |
    Format-List
```

The latest JSON should show:

```text
device_type = obd2_vehicle
source_mode = simulator
data_class = fast, slow, or diagnostic
```

## Vehicle mode

```text
Use simulator instead of vehicle CAN = disabled
TWAI TX GPIO = 26
TWAI RX GPIO = 27
Vehicle diagnostic CAN bitrate = 500 or 250 kbit/s
```

Rebuild and flash:

```powershell
idf.py fullclean
idf.py build
idf.py -p COMx flash monitor
```

Start with ignition on and engine off. Confirm diagnostic responses before
starting the engine.

## Supported standard PIDs

| PID | Measurement | Decode |
|---:|---|---|
| `04` | Calculated engine load | `A * 100 / 255` |
| `05` | Coolant temperature | `A - 40` |
| `0C` | Engine RPM | `((A * 256) + B) / 4` |
| `0D` | Vehicle speed | `A` |
| `0F` | Intake air temperature | `A - 40` |
| `11` | Throttle position | `A * 100 / 255` |
| `2F` | Fuel level | `A * 100 / 255` |

Not every vehicle supports every PID. A future revision should query PID `00`,
`20`, and `40` capability bitmaps and poll only confirmed PIDs.

## Current limitations

1. The hardware mapping is fixed to the supplied Artron Shop ESP-OBD2
   schematic: SN65HVD230 DI/TX on GPIO26 and RO/RX on GPIO27.
2. The current OBD request implementation handles common single-frame Mode 01
   replies. Full ISO-TP multi-frame handling is still required for VIN and
   extended diagnostics.
3. Offline flash/SD store-and-forward is not implemented.
4. NTP timestamping is delegated to the gateway/AWS ingestion path; the client
   includes monotonic uptime.
5. Vehicle-specific proprietary PIDs are outside this first version.

See [docs/METHODOLOGY.md](docs/METHODOLOGY.md) for commissioning and acceptance
criteria.

## Confirmed board hardware

The supplied schematic and dimension drawing identify:

- ESP32-WROOM-32E.
- SN65HVD230 3.3 V CAN transceiver.
- OBD-II pin 6 = CAN-H, pin 14 = CAN-L, pin 16 = vehicle supply, and pins 4/5
  = ground.
- GPIO26 drives transceiver DI (TWAI TX); GPIO27 reads RO (TWAI RX).
- Selectable 120-ohm bus termination through JP1.
- PSM712 CAN-line surge protector.
- TPS54202 buck supply, SS210 reverse-polarity series diode, and SMBJ30CA TVS.
- Board body approximately 33 x 30 mm; OBD connector section approximately
  40 x 20 mm.

The vendor states that the board accepts 12 V and 24 V vehicle systems, supports
common CAN bit rates including 250 and 500 kbit/s, and can be developed with
ESP-IDF. The schematic remains the controlling source for GPIO assignments.
