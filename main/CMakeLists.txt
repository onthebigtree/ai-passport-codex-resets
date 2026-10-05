idf_component_register(
    SRCS "main.c" "reset_ui.c" "reset_model.c" "reset_json.c" "reset_network.c"
         "../assets/fonts/reset_font_14.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_event esp_netif esp_timer esp_wifi nvs_flash
             esp_http_client esp_http_server mbedtls json lwip
)
