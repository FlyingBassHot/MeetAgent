idf_component_register(
    SRCS "net_client.c" "command_table.c" "wifi_prov.c"
    INCLUDE_DIRS "include"
    REQUIRES
        espui
        protocol
        state
    PRIV_REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        esp_http_client
        json
        esp_timer
        heap
        espressif__esp_websocket_client
)
