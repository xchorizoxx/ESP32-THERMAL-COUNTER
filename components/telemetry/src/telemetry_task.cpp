/**
 * @file telemetry_task.cpp
 * @brief Implementación de la tarea de telemetría (Core 0).
 *
 * Consume IpcPackets de la queue del Core 1 y los transmite por UDP.
 */

#include "telemetry_task.hpp"
#include "esp_log.h"
#include "http_server.hpp" // [NEW] Added for WebSockets

static const char* TAG = "TELEMETRY";

TelemetryTask::TelemetryTask(QueueHandle_t ipcQueue, UdpTransmitter& udp)
    : ipcQueue_(ipcQueue)
    , udp_(udp)
{
}

void TelemetryTask::init()
{
    ESP_LOGI(TAG, "TelemetryTask inicializado (queue=%p)", ipcQueue_);
}

void TelemetryTask::TaskWrapper(void* pvParameters)
{
    auto* self = static_cast<TelemetryTask*>(pvParameters);
    self->run();
    vTaskDelete(NULL);
}

void TelemetryTask::run()
{
    static IpcPacket packet; // static: evita asignar ~1.6 KB en el stack de la tarea

    ESP_LOGI(TAG, "Tarea de telemetría arrancada en Core %d", xPortGetCoreID());

    while (true) {
        // Bloquearse hasta recibir un paquete del pipeline (Core 1)
        BaseType_t received = xQueueReceive(ipcQueue_, &packet, portMAX_DELAY);
        if (received != pdTRUE) {
            continue;
        }

        // Enviar telemetría (contadores + tracks)
        esp_err_t err = udp_.sendTelemetry(packet.telemetria);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Error enviando telemetría (frame %lu)",
                     packet.telemetria.frame_id);
        }

        // Enviar imagen térmica
        err = udp_.sendImage(packet.imagen);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Error enviando imagen (frame %lu)",
                     packet.imagen.frame_id);
        }

        // [NEW] Enviar por WebSocket a los clientes HTTP
        HttpServer::broadcastFrame(packet.imagen, packet.telemetria, packet.sensor_ok);

        ESP_LOGD(TAG, "Frame %lu transmitido: IN=%d OUT=%d tracks=%d",
                 packet.telemetria.frame_id,
                 packet.telemetria.count_in,
                 packet.telemetria.count_out,
                 packet.telemetria.num_tracks);
    }
}
