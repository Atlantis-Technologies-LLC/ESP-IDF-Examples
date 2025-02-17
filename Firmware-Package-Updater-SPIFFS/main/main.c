#include <stdio.h>
#include <inttypes.h>  // Include this for PRI macros
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "project_settings.h"
#include "web_server.h"
#include "driver/gpio.h"
#include <esp_log.h>
#include <esp_spiffs.h>
#include <string.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "ota.h"

extern const char build_info_start[] asm("_binary_build_info_txt_start");
extern const char build_info_end[] asm("_binary_build_info_txt_end");

// Shared variables
volatile int timer_duration_seconds = 60; // Default 60 seconds
const char* VERSION = GIT_VERSION;

// Print build_info.txt to console
void print_build_info() {
    printf("%.*s\n", (int)(build_info_end - build_info_start), build_info_start);
}

// Initialize SPIFFS File System
static void init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE("SPIFFS", "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    size_t total, used;
    esp_spiffs_info(conf.partition_label, &total, &used);
    ESP_LOGI("SPIFFS", "Partition size: total: %" PRIu32 ", used: %" PRIu32, (uint32_t)total, (uint32_t)used);
}

void app_main(void) {
    // Print build info during boot
    print_build_info();

    /*********** INITIALIZATIONS ***********/
    // Initialize SPIFFS File System
    init_spiffs();

    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    wifi_init();

    // Initialize OTA update system
    init_ota();

    // Start the web server
    start_webserver();

    ESP_LOGI("MAIN", "Initialization complete - Version %s", VERSION);
}