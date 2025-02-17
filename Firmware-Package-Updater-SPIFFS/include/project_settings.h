// project_settings.h
#ifndef PROJECT_SETTINGS_H
#define PROJECT_SETTINGS_H

// WiFi AP Settings - For locally hosted ESP32 WiFi
#define WIFI_AP_BASE_SSID "ESP"    // Base SSID name - Last 4 of the MAC Address will get appended to this in the web_server.c code
#define WIFI_AP_PASSWORD "12345678"

// WiFi Station Settings - To connect ESP32 to existing WiFi network
#define WIFI_STA_SSID "My_SSID"   // Your router's WiFi SSID
#define WIFI_STA_PASSWORD "My_Password" // Your router's WiFi password

#endif // PROJECT_SETTINGS_H
