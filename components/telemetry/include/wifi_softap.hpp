#pragma once
/**
 * @file wifi_softap.hpp
 * @brief ESP32-S3 WiFi SoftAP Mode Manager.
 *
 * Creates its own WiFi network to which the receiving ESP32 connects
 * as a Station to receive thermal image and telemetry UDP datagrams.
 */

#include "esp_err.h"
#include "esp_wifi.h"

class WifiSoftAp {
public:
    /**
     * @brief Initializes and starts the SoftAP.
     * @param ssid     Network name (max 32 chars)
     * @param password Password (min 8 chars for WPA2, "" for open)
     * @param channel  WiFi channel [1..13]
     * @param maxConn  Maximum simultaneous clients
     * @return ESP_OK on success.
     *
     * @note Requires prior nvs_flash_init() in app_main().
     */
    static esp_err_t init(const char* ssid, const char* password,
                          int channel, int maxConn);

    /**
     * @brief Indicates if the SoftAP is active.
     */
    static bool isReady();

private:
    static void eventHandler(void* arg, esp_event_base_t eventBase,
                             int32_t eventId, void* eventData);
    static bool ready_;
};
