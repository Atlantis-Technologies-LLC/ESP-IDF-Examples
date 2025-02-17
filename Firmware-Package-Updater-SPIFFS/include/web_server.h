#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <esp_http_server.h>

extern const char* VERSION; //Git versioning variable

// Function to initialize WiFi in station mode
void wifi_init(void);

// Function to start the web server
httpd_handle_t start_webserver(void);

#endif // WEB_SERVER_H