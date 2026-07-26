"""Shared Wi-Fi interface helpers for station and private access-point modes."""

import network

from config import (NETWORK_MODE, AP_SSID, AP_PASSWORD, AP_IP, AP_NETMASK,
                    AP_CHANNEL, AP_MAX_CLIENTS)


def is_ap_mode():
    return str(NETWORK_MODE).upper() == "AP"


def interface():
    return network.WLAN(network.AP_IF if is_ap_mode() else network.STA_IF)


def ready(wlan=None):
    wlan = wlan or interface()
    if is_ap_mode():
        return wlan.active()
    return wlan.isconnected()


def configure_ap(wlan=None):
    """Activate the private WLAN with conservative MicroPython compatibility."""
    wlan = wlan or network.WLAN(network.AP_IF)
    sta = network.WLAN(network.STA_IF)
    try:
        sta.active(False)
    except Exception:
        pass
    wlan.active(True)
    auth = getattr(network, "AUTH_WPA2_PSK", 3)
    options = {"essid": AP_SSID, "password": AP_PASSWORD,
               "authmode": auth, "channel": AP_CHANNEL}
    try:
        options["max_clients"] = AP_MAX_CLIENTS
        wlan.config(**options)
    except Exception:
        # Some MicroPython builds do not expose max_clients.
        options.pop("max_clients", None)
        wlan.config(**options)
    wlan.ifconfig((AP_IP, AP_NETMASK, AP_IP, AP_IP))
    return wlan


def station_count(wlan=None):
    if not is_ap_mode():
        return None
    try:
        return len((wlan or interface()).status("stations"))
    except Exception:
        return None
