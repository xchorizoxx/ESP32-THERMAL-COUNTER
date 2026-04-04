/**
 * @file wifi_softap.cpp
 * @brief WiFi SoftAP manager implementation.
 */

#include "wifi_softap.hpp"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include <cstring>

static const char* TAG = "WIFI_AP";

bool WifiSoftAp::ready_ = false;

void WifiSoftAp::eventHandler(void* arg, esp_event_base_t eventBase,
                               int32_t eventId, void* eventData)
{
    if (eventBase == WIFI_EVENT) {
        if (eventId == WIFI_EVENT_AP_START) {
            ready_ = true;
            ESP_LOGI(TAG, "SoftAP active");
        } else if (eventId == WIFI_EVENT_AP_STACONNECTED) {
            auto* event = static_cast<wifi_event_ap_staconnected_t*>(eventData);
            ESP_LOGI(TAG, "Client connected: " MACSTR " (AID=%d)",
                     MAC2STR(event->mac), event->aid);
        } else if (eventId == WIFI_EVENT_AP_STADISCONNECTED) {
            auto* event = static_cast<wifi_event_ap_stadisconnected_t*>(eventData);
            ESP_LOGI(TAG, "Client disconnected: " MACSTR,
                     MAC2STR(event->mac));
        }
    }
}

esp_err_t WifiSoftAp::init(const char* ssid, const char* password,
                            int channel, int maxConn)
{
    ESP_LOGI(TAG, "Starting SoftAP — SSID: '%s', Channel: %d", ssid, channel);

    // 1. Initialize network stack and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    // 2. Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // 3. Register event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiSoftAp::eventHandler, NULL, NULL));

    // 4. Build AP configuration
    wifi_config_t wifiConfig = {};
    strncpy((char*)wifiConfig.ap.ssid, ssid, sizeof(wifiConfig.ap.ssid) - 1);
    wifiConfig.ap.ssid_len    = (uint8_t)strlen(ssid);
    wifiConfig.ap.channel     = (uint8_t)channel;
    wifiConfig.ap.max_connection = (uint8_t)maxConn;

    if (strlen(password) == 0) {
        wifiConfig.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        strncpy((char*)wifiConfig.ap.password, password,
                sizeof(wifiConfig.ap.password) - 1);
        wifiConfig.ap.authmode = WIFI_AUTH_WPA2_PSK;
        wifiConfig.ap.pmf_cfg.required = false;
    }

    // 5. Configure and start
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifiConfig));
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "SoftAP '%s' started on channel %d (max %d clients)",
             ssid, channel, maxConn);
    return ESP_OK;
}

bool WifiSoftAp::isReady()
{
    return ready_;
}
