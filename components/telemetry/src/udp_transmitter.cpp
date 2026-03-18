/**
 * @file udp_transmitter.cpp
 * @brief Implementación del transmisor UDP.
 *
 * Nota sobre malloc: sendTelemetry y sendImage usan buffers locales en stack
 * (tamaños conocidos y acotados en tiempo de compilación). No se usa heap.
 */

#include "udp_transmitter.hpp"
#include "thermal_config.hpp"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <cstring>

static const char* TAG = "UDP_TX";

UdpTransmitter::UdpTransmitter(const char* broadcastIp, int port)
    : broadcastIp_(broadcastIp)
    , port_(port)
    , sock_(-1)
{
}

esp_err_t UdpTransmitter::init()
{
    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock_ < 0) {
        ESP_LOGE(TAG, "Error al crear socket UDP: errno=%d", errno);
        return ESP_FAIL;
    }

    // Activar broadcast en el socket
    int broadcastEnable = 1;
    if (setsockopt(sock_, SOL_SOCKET, SO_BROADCAST,
                   &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        ESP_LOGE(TAG, "setsockopt SO_BROADCAST falló: errno=%d", errno);
        close(sock_);
        sock_ = -1;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Socket UDP creado → %s:%d", broadcastIp_, port_);
    return ESP_OK;
}

esp_err_t UdpTransmitter::sendTelemetry(const PayloadTelemetria& payload)
{
    if (sock_ < 0) {
        ESP_LOGE(TAG, "sendTelemetry: socket no inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    // Construir datagrama: [type_byte][payload]
    constexpr int bufSize = 1 + sizeof(PayloadTelemetria);
    uint8_t buf[bufSize];
    buf[0] = ThermalConfig::UDP_PACKET_TELEMETRY;
    memcpy(buf + 1, &payload, sizeof(PayloadTelemetria));

    struct sockaddr_in destAddr = {};
    destAddr.sin_family      = AF_INET;
    destAddr.sin_port        = htons((uint16_t)port_);
    destAddr.sin_addr.s_addr = inet_addr(broadcastIp_);

    int sent = sendto(sock_, buf, bufSize, 0,
                      (struct sockaddr*)&destAddr, sizeof(destAddr));
    if (sent < 0) {
        ESP_LOGW(TAG, "sendTelemetry falló: errno=%d", errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t UdpTransmitter::sendImage(const PayloadImagen& payload)
{
    if (sock_ < 0) {
        ESP_LOGE(TAG, "sendImage: socket no inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    // Construir datagrama: [type_byte][payload]
    constexpr int bufSize = 1 + sizeof(PayloadImagen);
    uint8_t buf[bufSize];
    buf[0] = ThermalConfig::UDP_PACKET_IMAGE;
    memcpy(buf + 1, &payload, sizeof(PayloadImagen));

    struct sockaddr_in destAddr = {};
    destAddr.sin_family      = AF_INET;
    destAddr.sin_port        = htons((uint16_t)port_);
    destAddr.sin_addr.s_addr = inet_addr(broadcastIp_);

    int sent = sendto(sock_, buf, bufSize, 0,
                      (struct sockaddr*)&destAddr, sizeof(destAddr));
    if (sent < 0) {
        ESP_LOGW(TAG, "sendImage falló: errno=%d", errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}
