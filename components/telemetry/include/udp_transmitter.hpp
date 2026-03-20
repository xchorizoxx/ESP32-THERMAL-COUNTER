#pragma once
/**
 * @file udp_transmitter.hpp
 * @brief UDP transmitter for thermal image and telemetry.
 *
 * Sends two types of datagrams via UDP broadcast:
 *   - 0x01: TelemetryPayload (counters + tracks)
 *   - 0x02: ImagePayload (768 pixels × int16)
 */

#include "thermal_types.hpp"
#include "esp_err.h"

class UdpTransmitter {
public:
    /**
     * @brief Constructor.
     * @param broadcastIp Target/broadcast IP (e.g., "192.168.4.255")
     * @param port        UDP port
     */
    UdpTransmitter(const char* broadcastIp, int port);

    /**
     * @brief Initializes the UDP socket.
     * @return ESP_OK on success.
     */
    esp_err_t init();

    /**
     * @brief Sends the telemetry packet (counters + tracks).
     * Datagram: [0x01][TelemetryPayload]
     * @return ESP_OK on success.
     */
    esp_err_t sendTelemetry(const TelemetryPayload& payload);

    /**
     * @brief Sends the thermal image packet.
     * Datagram: [0x02][ImagePayload]
     * @return ESP_OK on success.
     */
    esp_err_t sendImage(const ImagePayload& payload);

private:
    const char* broadcastIp_;
    int         port_;
    int         sock_;
};
