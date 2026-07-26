"""MicroPython boot hook: bring up the station interface before main.py."""

import time
import network
from config import (WIFI_SSID, WIFI_PASSWORD, WIFI_TIMEOUT_SECONDS,
                    WIFI_STATIC_IP_ENABLED, WIFI_STATIC_IP, WIFI_NETMASK,
                    WIFI_GATEWAY, WIFI_DNS, WEBREPL_ENABLED,
                    WEBREPL_PASSWORD, NTP_ENABLED)
from network_manager import is_ap_mode, interface, ready, configure_ap


wlan = interface()
if is_ap_mode():
    configure_ap(wlan)
    print("Private WiFi AP started")
else:
    wlan.active(True)
    if WIFI_STATIC_IP_ENABLED:
        try:
            wlan.ifconfig((WIFI_STATIC_IP, WIFI_NETMASK, WIFI_GATEWAY, WIFI_DNS))
            print("Configured static IP:", WIFI_STATIC_IP)
        except Exception as exc:
            print("Static IP configuration failed; using DHCP:", exc)

    if not wlan.isconnected():
        print("Connecting to", WIFI_SSID)
        wlan.connect(WIFI_SSID, WIFI_PASSWORD)
        started = time.ticks_ms()
        timeout_ms = WIFI_TIMEOUT_SECONDS * 1000
        while not wlan.isconnected():
            if time.ticks_diff(time.ticks_ms(), started) >= timeout_ms:
                print("WiFi connection timed out")
                break
            time.sleep_ms(250)

print("Network:", wlan.ifconfig())

if WEBREPL_ENABLED and ready(wlan):
    try:
        import webrepl
        webrepl.start(password=WEBREPL_PASSWORD)
        print("WebREPL enabled on ws://%s:8266" % wlan.ifconfig()[0])
    except ImportError:
        print("WebREPL module is not included in this MicroPython firmware")
    except Exception as exc:
        print("WebREPL failed:", exc)

if NTP_ENABLED and not is_ap_mode() and wlan.isconnected():
    from time_manager import sync_time
    sync_time()
elif NTP_ENABLED and is_ap_mode():
    print("NTP skipped in AP-only mode; AWS received_at remains authoritative")
