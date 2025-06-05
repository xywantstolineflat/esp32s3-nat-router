idf_component_register(
    SRCS "main.c" "ap_config.c" "sta_config.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_http_server esp_spiffs
)
