idf_component_register(
    SRCS "ota.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client app_update esp_wifi nvs_flash esp_driver_gpio
)