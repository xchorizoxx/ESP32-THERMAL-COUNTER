/**
 * @file telemetry_task.cpp
 * @brief Implementation of the telemetry task (Core 0).
 *
 * Consumes IpcPackets from Core 1 queue and transmits them via UDP.
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
    ESP_LOGI(TAG, "TelemetryTask initialized (queue=%p)", ipcQueue_);
}

void TelemetryTask::TaskWrapper(void* pvParameters)
{
    auto* self = static_cast<TelemetryTask*>(pvParameters);
    self->run();
    vTaskDelete(NULL);
}

void TelemetryTask::run()
{
    static IpcPacket packet; // static: avoids allocating ~1.6 KB on the task stack

    ESP_LOGI(TAG, "Telemetry task started on Core %d", xPortGetCoreID());

    while (true) {
        // Block until receiving a packet from the pipeline (Core 1)
        BaseType_t received = xQueueReceive(ipcQueue_, &packet, portMAX_DELAY);
        if (received != pdTRUE) {
            continue;
        }

        // Send telemetry (counters + tracks)
        esp_err_t err = udp_.sendTelemetry(packet.telemetria);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Error sending telemetry (frame %lu)",
                     packet.telemetria.frame_id);
        }

        // Send thermal image
        err = udp_.sendImage(packet.imagen);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Error sending image (frame %lu)",
                     packet.imagen.frame_id);
        }

        // [NEW] Send via WebSocket to HTTP clients
        HttpServer::broadcastFrame(packet.imagen, packet.telemetria, packet.sensor_ok);

        ESP_LOGD(TAG, "Frame %lu transmitted: IN=%d OUT=%d tracks=%d",
                 packet.telemetria.frame_id,
                 packet.telemetria.count_in,
                 packet.telemetria.count_out,
                 packet.telemetria.num_tracks);
    }
}
