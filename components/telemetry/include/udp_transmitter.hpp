#pragma once
/**
 * @file udp_transmitter.hpp
 * @brief Transmisor UDP para imagen térmica y telemetría.
 *
 * Envía dos tipos de datagramas por broadcast UDP:
 *   - 0x01: PayloadTelemetria (contadores + tracks)
 *   - 0x02: PayloadImagen (768 píxeles × int16)
 */

#include "thermal_types.hpp"
#include "esp_err.h"

class UdpTransmitter {
public:
    /**
     * @brief Constructor.
     * @param broadcastIp IP de destino/broadcast (ej. "192.168.4.255")
     * @param port        Puerto UDP
     */
    UdpTransmitter(const char* broadcastIp, int port);

    /**
     * @brief Inicializa el socket UDP.
     * @return ESP_OK en éxito.
     */
    esp_err_t init();

    /**
     * @brief Envía el paquete de telemetría (contadores + tracks).
     * Datagrama: [0x01][PayloadTelemetria]
     * @return ESP_OK en éxito.
     */
    esp_err_t sendTelemetry(const PayloadTelemetria& payload);

    /**
     * @brief Envía el paquete de imagen térmica.
     * Datagrama: [0x02][PayloadImagen]
     * @return ESP_OK en éxito.
     */
    esp_err_t sendImage(const PayloadImagen& payload);

private:
    const char* broadcastIp_;
    int         port_;
    int         sock_;
};
