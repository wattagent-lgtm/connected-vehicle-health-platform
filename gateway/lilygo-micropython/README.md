# LILYGO T-A7670 MicroPython Gateway

The gateway hosts a private Wi-Fi AP for the OBD client, accepts newline
delimited JSON on TCP port 5005 and forwards validated messages to AWS IoT over
the A7670 LTE modem.

## Important configuration

Edit `config.py` locally:

- replace every `YOUR_...` and `CHANGE_ME`;
- set the carrier APN;
- set the AWS IoT endpoint and client ID;
- install CA, device certificate and private key at the configured paths;
- keep WebREPL disabled unless it is required and protected.

The TCP server acknowledges accepted input before MQTT publication. The MQTT
queue is bounded to protect ESP32 RAM; monitor queue depth and drop counters
under load.

Never commit the configured file or the `certs/` directory to a public fork.
