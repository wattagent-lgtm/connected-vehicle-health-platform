idf_component_register(
    SRCS
        "app_main.c"
        "gateway_client.c"
        "obd2_pid.c"
        "obd2_source.c"
        "wifi_manager.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash json driver esp_timer
)
