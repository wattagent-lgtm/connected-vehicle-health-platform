"""Gateway configuration. Keep secrets out of source control in production."""

WIFI_SSID = "YOUR_WIFI_SSID"
WIFI_PASSWORD = "CHANGE_ME"
WIFI_TIMEOUT_SECONDS = 30

# Local network role:
# - "STA": join an existing Wi-Fi network (legacy/default installation).
# - "AP": host a private field network for ESP32 clients; AWS still uses LTE.
NETWORK_MODE = "AP"
AP_SSID = "VEHICLE-GW-01"
AP_PASSWORD = "CHANGE_ME_STRONG"
AP_IP = "192.168.4.1"
AP_NETMASK = "255.255.255.0"
AP_CHANNEL = 6
AP_MAX_CLIENTS = 8

# Stable LAN address for clients, dashboard and monitoring tools. Reserve this
# address for the gateway in the router as well, or keep it outside the DHCP
# allocation range to prevent another device receiving the same address.
WIFI_STATIC_IP_ENABLED = True
WIFI_STATIC_IP = "192.168.1.42"
WIFI_NETMASK = "255.255.255.0"
WIFI_GATEWAY = "192.168.1.1"
WIFI_DNS = "192.168.1.1"

# WebREPL provides authenticated file transfer and REPL access over the LAN.
# Use 4-9 characters for broad MicroPython WebREPL compatibility.
WEBREPL_ENABLED = False
WEBREPL_PASSWORD = "CHANGE_ME"

# Time synchronization. MicroPython's RTC is set to UTC; display conversion is
# applied separately so protocol timestamps can remain future-ready.
NTP_ENABLED = True
NTP_HOST = "pool.ntp.org"
NTP_RETRIES = 3
NTP_RETRY_DELAY_SECONDS = 2
TIMEZONE_OFFSET_HOURS = 7

# LILYGO T-A7670 R2 (classic ESP32) modem configuration.
MODEM_ENABLED = True
MODEM_UART_ID = 1
MODEM_BAUDRATE = 115200
# Pin names are from the ESP32 UART perspective, matching LilyGO utilities.h.
MODEM_RX_PIN = 27
MODEM_TX_PIN = 26
MODEM_PWRKEY_PIN = 4
MODEM_RESET_PIN = 5
MODEM_DTR_PIN = 25
MODEM_POWER_ENABLE_PIN = 12
MODEM_APN = "internet"
MODEM_APN_USERNAME = ""
MODEM_APN_PASSWORD = ""
MODEM_COMMAND_TIMEOUT_MS = 3000
MODEM_STARTUP_TIMEOUT_SECONDS = 20
MODEM_MONITOR_INTERVAL_SECONDS = 60
MODEM_INTERNET_TEST_INTERVAL_SECONDS = 300
MODEM_PING_HOST = "8.8.8.8"
MODEM_PING_TIMEOUT_SECONDS = 15

GATEWAY_NAME = "LTE-GW-01"
FIRMWARE_VERSION = "2.0.1"

# An unhandled service exception can leave lwIP sockets allocated even after
# the asyncio loop exits. Record the fault, then reboot to restore all network
# services instead of leaving the board alive but unreachable.
CRASH_LOG_FILE = "crash.log"
CRASH_RESTART_DELAY_SECONDS = 1

TCP_PORT = 5005
HTTP_PORT = 80
TCP_READ_SIZE = 1024
TCP_MAX_MESSAGE_SIZE = 4096
TCP_FRAME_TIMEOUT_MS = 150
# Persistent NDJSON clients may keep one socket open between telemetry frames.
# Legacy one-message-per-connection clients remain fully supported.
TCP_CLIENT_IDLE_TIMEOUT_MS = 5000
HTTP_READ_SIZE = 1024

# Dashboard workload controls. The fast status remains responsive while larger
# device/log payloads and heap collections run less frequently.
HTTP_GC_INTERVAL_REQUESTS = 10
HTTP_GC_LOW_MEMORY_BYTES = 55000
HTTP_MAX_ACTIVE_REQUESTS = 6
HTTP_HEALTH_INTERVAL_SECONDS = 60
HTTP_HEALTH_FAILURE_LIMIT = 3
HTTP_HEALTH_TIMEOUT_MS = 3000
DASHBOARD_DETAIL_LOG_LIMIT = 30

# Runtime Wi-Fi recovery. boot.py still performs the initial connection.
WIFI_MONITOR_INTERVAL_SECONDS = 10
WIFI_RECONNECT_TIMEOUT_SECONDS = 30

LOG_CAPACITY = 500
DEVICE_CAPACITY = 50
STATIC_ROOT = "www"

# AWS IoT Core over the A7670 internal MQTT/TLS stack.
MQTT_ENABLED = True
MQTT_BROKER = "YOUR_ENDPOINT-ats.iot.ap-southeast-1.amazonaws.com"
MQTT_PORT = 8883
MQTT_CLIENT_ID = "lte-gw-01"
MQTT_KEEPALIVE_SECONDS = 60
MQTT_QUEUE_CAPACITY = 20
MQTT_RECONNECT_MAX_SECONDS = 300
MQTT_TOPIC_PREFIX = "dt/iiot-lab/factory1"
MQTT_COMMAND_TOPIC = "cmd/iiot-lab/factory1/gateway/lte-gw-01/request"
MQTT_RESPONSE_TOPIC = "cmd/iiot-lab/factory1/gateway/lte-gw-01/response"
MQTT_CA_FILE = "certs/AmazonRootCA1.pem"
MQTT_CERT_FILE = "certs/device.pem.crt"
MQTT_KEY_FILE = "certs/private.pem.key"
CLOUD_ENABLED = True
