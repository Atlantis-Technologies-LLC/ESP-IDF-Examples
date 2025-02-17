#include "project_settings.h"
#include "web_server.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_ota_ops.h>
#include <esp_littlefs.h>
#include <string.h>
#include <fcntl.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <dirent.h>
#include <inttypes.h>

#include "cJSON.h"

// External reference to version
extern const char* VERSION;

static const char* TAG = "WebServer";

// Define LittleFS Mount Path
#define MOUNT_POINT "/web"

// Get MIN Utility Function
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/* Generic file serving function */
static esp_err_t file_get_handler(httpd_req_t *req, const char *file_path, const char *mime_type) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file: %s", file_path);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, mime_type);
    char buffer[512];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, read_bytes);
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}


/************************************/
/* Packaged Firmware Update Utility */
/************************************/

// Define Package Header Format
#define PACKAGE_MAGIC "ESP_UPDATE"
#define HEADER_SIZE 10 + sizeof(uint32_t) * 4  // 10-byte magic + 4x uint32_t values
#define WRITE_BLOCK_SIZE 8192 // LittleFS Default: 4096 | LittleFS Default: 8192

// Struct for package header
typedef struct {
    uint32_t firmware_size;
    uint32_t littlefs_size;
    uint32_t firmware_offset;
    uint32_t littlefs_offset;
    uint32_t version;
} package_header_t;

// Handles the firmware update request
static esp_err_t package_upload_handler(httpd_req_t *req) {
    char *write_buffer = heap_caps_malloc(WRITE_BLOCK_SIZE, MALLOC_CAP_DMA);
    if (!write_buffer) {
        ESP_LOGE(TAG, "Failed to allocate write buffer");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    ESP_LOGI(TAG, "Update package upload started. Size: %" PRIu32 " bytes", (uint32_t)remaining);

    // Read the package header
    int recv_len = httpd_req_recv(req, write_buffer, MIN(remaining, HEADER_SIZE));
    if (recv_len <= 0) {
        ESP_LOGE(TAG, "Error receiving package header");
        free(write_buffer);
        return ESP_FAIL;
    }

    // Check magic header
    if (memcmp(write_buffer, PACKAGE_MAGIC, 10) != 0) {
        ESP_LOGE(TAG, "Invalid package magic header");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid package format");
        free(write_buffer);
        return ESP_FAIL;
    }

    // Extract firmware & LittleFS sizes and offsets
    package_header_t pkg_header;
    memcpy(&pkg_header.firmware_size, write_buffer + 10, sizeof(uint32_t));
    memcpy(&pkg_header.littlefs_size, write_buffer + 10 + sizeof(uint32_t), sizeof(uint32_t));
    memcpy(&pkg_header.firmware_offset, write_buffer + 10 + (2 * sizeof(uint32_t)), sizeof(uint32_t));
    memcpy(&pkg_header.littlefs_offset, write_buffer + 10 + (3 * sizeof(uint32_t)), sizeof(uint32_t));

    ESP_LOGI(TAG, "Package contains: Firmware (%" PRIu32 " bytes), LittleFS (%" PRIu32 " bytes)", 
         (uint32_t)pkg_header.firmware_size, (uint32_t)pkg_header.littlefs_size);

    remaining -= recv_len;

    ESP_LOGI(TAG, "Remaining after package header (%d bytes)",remaining);

    // **Unmount LittleFS BEFORE updating firmware**
    ESP_LOGI(TAG, "Unmounting LittleFS before firmware update...");
    esp_vfs_littlefs_unregister("web");
    vTaskDelay(pdMS_TO_TICKS(500)); // Allow cleanup

    // **Start Firmware Update (OTA)**
    // Get update partition for firmware
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t ota_handle;
    if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start OTA update");
        free(write_buffer);
        return ESP_FAIL;
    }

    uint32_t firmware_written = 0;
    uint32_t firmware_remaining = pkg_header.firmware_size;
    while (firmware_written < pkg_header.firmware_size) {
        recv_len = httpd_req_recv(req, write_buffer, MIN(firmware_remaining, WRITE_BLOCK_SIZE));
        if (recv_len <= 0) {
            ESP_LOGE(TAG, "Firmware upload failed");
            esp_ota_end(ota_handle);
            free(write_buffer);
            return ESP_FAIL;
        }

        if (esp_ota_write(ota_handle, write_buffer, recv_len) != ESP_OK) {
            ESP_LOGE(TAG, "Firmware write failed");
            esp_ota_end(ota_handle);
            free(write_buffer);
            return ESP_FAIL;
        }

        firmware_written += recv_len;
        firmware_remaining -= recv_len;
        remaining -= recv_len;
        ESP_LOGI(TAG, "Writing firmware: current remaining (%d bytes), firmware written (%d bytes), firmware remaining (%d bytes)",remaining, firmware_written, firmware_remaining);
    }

    // Finalize OTA
    if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(update_partition) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to complete OTA update");
        free(write_buffer);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Firmware update complete!");

    // **Remount LittleFS After updating firmware**
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/web",
        .partition_label = "web",
        .format_if_mount_failed = true,
        .read_only = false
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to remount LittleFS after firmware update: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "LittleFS remounted successfully.");

    // **Handle LittleFS File Updates**
    while (remaining > 0) {
        uint16_t file_name_len;
        uint32_t file_size;

        // Read 6-byte file metadata (file_name_len + file_size)
        recv_len = httpd_req_recv(req, write_buffer, 6);
        if (recv_len != 6) {
            ESP_LOGE(TAG, "Failed to read file metadata");
            free(write_buffer);
            return ESP_FAIL;
        }

        // Extract values correctly (Little-Endian Fix)
        file_name_len = (uint16_t)((write_buffer[1] << 8) | write_buffer[0]);
        file_size = (uint32_t)((write_buffer[5] << 24) | (write_buffer[4] << 16) | (write_buffer[3] << 8) | write_buffer[2]);

        ESP_LOGI(TAG, "Extracted file metadata -> File Name Length: %" PRIu32 ", File Size: %" PRIu32, 
         (uint32_t)file_name_len, (uint32_t)file_size);

        // Validate File Name Length (1-255 bytes)
        if (file_name_len < 1 || file_name_len > 255) {
            ESP_LOGE(TAG, "Invalid file name length: %d", file_name_len);
            free(write_buffer);
            return ESP_FAIL;
        }        

        // Validate File Size (1MB limit)
        if (file_size > 1024 * 1024) {
            ESP_LOGE(TAG, "Invalid file size: %d", file_size);
            free(write_buffer);
            return ESP_FAIL;
        }

        remaining -= 6;

        // Read file name
        recv_len = httpd_req_recv(req, write_buffer, file_name_len);
        if (recv_len != file_name_len) {
            ESP_LOGE(TAG, "Error: Expected file name length %d but received %d", file_name_len, recv_len);
            free(write_buffer);
            return ESP_FAIL;
        }

        char file_name[256];
        memcpy(file_name, write_buffer, file_name_len);
        file_name[file_name_len] = '\0';

        remaining -= recv_len;

        char filepath[300];
        
        // Ensure the file path is within bounds
        if (strlen(file_name) > 250) {  // Leave space for "/web/"
            ESP_LOGE(TAG, "File path too long: %s", file_name);
            free(write_buffer);
            return ESP_FAIL;
        }

        snprintf(filepath, sizeof(filepath), "/web/%s", file_name);
        ESP_LOGI(TAG, "Writing file: %s (Size: %d bytes)", filepath, file_size);

        // Open file in binary mode to prevent corruption
        FILE *file = fopen(filepath, "wb");
        if (!file) {
            ESP_LOGE(TAG, "Failed to open file for writing: %s", filepath);
            free(write_buffer);
            return ESP_FAIL;
        }

        // Track bytes written per file
        uint32_t file_written = 0;
        uint32_t file_remaining = file_size;

        while (file_written < file_size) {
            recv_len = httpd_req_recv(req, write_buffer, MIN(file_remaining, WRITE_BLOCK_SIZE));
            
            if (recv_len <= 0) {
                ESP_LOGE(TAG, "File upload error: %s", filepath);
                fclose(file);
                remove(filepath);
                free(write_buffer);
                return ESP_FAIL;
            }

            fwrite(write_buffer, 1, recv_len, file);
            file_written += recv_len;
            file_remaining -= recv_len;
            remaining -= recv_len;

            ESP_LOGI(TAG, "Writing LittleFS file: %s, Remaining: %d bytes", filepath, file_remaining);
        }

        fclose(file);
        ESP_LOGI(TAG, "Successfully wrote file: %s (%" PRIu32 " bytes)", filepath, (uint32_t)file_written);
    }

    // Free memory and confirm update
    free(write_buffer);
    httpd_resp_sendstr(req, "Update complete! Device rebooting...");

    // **Reboot after everything is done**
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

/************************************/


static esp_err_t css_get_handler(httpd_req_t *req) {
    return file_get_handler(req, "/web/styles.css", "text/css");
}

static esp_err_t firmware_update_html_get_handler(httpd_req_t *req) {
    return file_get_handler(req, "/web/firmware-update.html", "text/html");
}

static esp_err_t firmware_update_js_get_handler(httpd_req_t *req) {
    return file_get_handler(req, "/web/firmware-update.js", "application/javascript");
}

// Handler for firmware version endpoint (returns JSON)
static esp_err_t version_handler(httpd_req_t *req)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t app_desc;
    
    if (esp_ota_get_partition_description(running, &app_desc) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to get firmware version");
        return ESP_FAIL;
    }

    // Create JSON response
    char json_response[100];
    snprintf(json_response, sizeof(json_response), "{\"version\": \"%s\"}", app_desc.version);

    httpd_resp_set_type(req, "application/json");  // Set response type to JSON
    httpd_resp_sendstr(req, json_response);
    return ESP_OK;
}

/* URI handlers */
static httpd_uri_t styles_css_uri = {
    .uri       = "/styles.css",
    .method    = HTTP_GET,
    .handler   = css_get_handler,
    .user_ctx  = NULL
};

static httpd_uri_t firmware_update_html_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = firmware_update_html_get_handler,
    .user_ctx  = NULL
};

static httpd_uri_t firmware_update_js_uri = {
    .uri       = "/firmware-update.js",
    .method    = HTTP_GET,
    .handler   = firmware_update_js_get_handler,
    .user_ctx  = NULL
};

httpd_uri_t update_firmware_uri = {
    .uri       = "/update_firmware",
    .method    = HTTP_POST,
    .handler   = package_upload_handler,
    .user_ctx  = NULL
};

httpd_uri_t version = {
    .uri       = "/version",
    .method    = HTTP_GET,
    .handler   = version_handler,
    .user_ctx  = NULL
};

/* Function to generate unique SSID */
void generate_unique_ssid(char *ssid, size_t ssid_len) {
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);  // Get the MAC address of the ESP32
    snprintf(ssid, ssid_len, "%s_%02X%02X", WIFI_AP_BASE_SSID, mac[4], mac[5]);  // Append underscore and last 4 digits of MAC
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    static int retry_count = 0;  // Track the number of reconnection attempts
    const int MAX_RETRY = 5;     // Maximum reconnection attempts

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi started, attempting to connect...");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGW(TAG, "WiFi disconnected! Reason: %d, retrying... (Attempt %d/%d)", event->reason, retry_count + 1, MAX_RETRY);

                if (retry_count < MAX_RETRY) {
                    vTaskDelay(pdMS_TO_TICKS(2000));  // Wait 2 seconds before retry
                    esp_wifi_connect();
                    retry_count++;
                } else {
                    ESP_LOGE(TAG, "Max WiFi retry limit reached (%d). Not reconnecting until restart.", MAX_RETRY);
                }
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_count = 0;  // Reset retry count on successful connection
    }
}

void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = ESP_IP4TOADDR(192, 168, 4, 1);
    ip_info.gw.addr = ESP_IP4TOADDR(0, 0, 0, 0);
    ip_info.netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0);

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register the event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    char unique_ssid[32];
    generate_unique_ssid(unique_ssid, sizeof(unique_ssid));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .ssid_len = strlen(unique_ssid),
            .channel = 1,
            .password = WIFI_AP_PASSWORD,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    strncpy((char *)wifi_config.ap.ssid, unique_ssid, sizeof(wifi_config.ap.ssid) - 1);

    if (strlen(WIFI_AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_config_t sta_config = {
        .sta = {
            .ssid = WIFI_STA_SSID,
            .password = WIFI_STA_PASSWORD,
            .listen_interval = 1
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));  // Disable power-saving

    
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialized in AP+STA mode.");
    ESP_LOGI(TAG, "AP Mode SSID: %s", unique_ssid);
}

/* Start the web server */
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Increase the maximum number of URI handlers
    config.max_uri_handlers = 20;  // Increase from default (8) to accommodate all our endpoints

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Register the URI handlers for files
        httpd_register_uri_handler(server, &styles_css_uri);
        httpd_register_uri_handler(server, &firmware_update_html_uri);
        httpd_register_uri_handler(server, &firmware_update_js_uri);

        // Register API endpoint handlers
        httpd_register_uri_handler(server, &update_firmware_uri);
        httpd_register_uri_handler(server, &version);

        ESP_LOGI(TAG, "Web server started");
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
    return server;
}