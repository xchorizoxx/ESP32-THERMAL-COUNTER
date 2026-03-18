#pragma once
/**
 * @file wifi_softap.hpp
 * @brief Gestor del modo SoftAP WiFi del ESP32-S3.
 *
 * Crea una red WiFi propia a la que el ESP32 receptor se conecta
 * como Station para recibir los datagramas UDP de imagen y telemetría.
 */

#include "esp_err.h"
#include "esp_wifi.h"

class WifiSoftAp {
public:
    /**
     * @brief Inicializa y arranca el SoftAP.
     * @param ssid     Nombre de la red (max 32 chars)
     * @param password Contraseña (min 8 chars para WPA2, "" para abierta)
     * @param channel  Canal WiFi [1..13]
     * @param maxConn  Máximo de clientes simultáneos
     * @return ESP_OK en éxito.
     *
     * @note Requiere nvs_flash_init() previo en app_main().
     */
    static esp_err_t init(const char* ssid, const char* password,
                          int channel, int maxConn);

    /**
     * @brief Indica si el SoftAP está activo.
     */
    static bool isReady();

private:
    static void eventHandler(void* arg, esp_event_base_t eventBase,
                             int32_t eventId, void* eventData);
    static bool ready_;
};
