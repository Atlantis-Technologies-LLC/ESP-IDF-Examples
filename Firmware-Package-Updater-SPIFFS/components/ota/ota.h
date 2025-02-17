#pragma once

#include <esp_err.h>

/**
 * @brief Initialize OTA subsystem
 * 
 * Initializes NVS and checks for pending validation after OTA update
 */
void init_ota(void);

/**
 * @brief Perform OTA update from given URL
 * 
 * Downloads and installs new firmware from the specified URL
 * 
 * @param url URL of the firmware file
 * @return esp_err_t ESP_OK on success, or error code
 */
esp_err_t perform_ota_update(const char *url);