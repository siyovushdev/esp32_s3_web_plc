idf_component_register(
        SRCS
        "main.c"
        "web_server.c"
        "wifi_ap.c"
        "api_handlers.c"
        "plc_frame.c"
        "plc_uart_client.c"
        "plc_gateway_state.c"
        "plc_client.c"
        "plc_graph_storage.c"
        "plc_graph_builder.c"
        INCLUDE_DIRS
        "."
        REQUIRES
        esp_http_server
        esp_netif
        esp_wifi
        esp_event
        nvs_flash
        littlefs
        driver
        mbedtls
)

littlefs_create_partition_image(storage /home/siovush/plc-web-hmi/dist FLASH_IN_PROJECT)
