idf_component_register(
    SRCS
        "app_main.c"
        "ring_buf.c"
        "msg.c"
        "rgb.c"
        "ws.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_websocket_client
    PRIV_REQUIRES
        esp_driver_ledc
        nvs_flash
        esp_wifi
        esp_netif
        esp_driver_gpio
)