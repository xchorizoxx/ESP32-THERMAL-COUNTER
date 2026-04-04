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
        esp_err_t err = udp_.sendTelemetry(packet.telemetry);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Error sending telemetry (frame %lu)",
                     packet.telemetry.frame_id);
        }

        // Send thermal image
        err = udp_.sendImage(packet.image);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Error sending image (frame %lu)",
                     packet.image.frame_id);
        }

        // [NEW] Send via WebSocket to HTTP clients
        HttpServer::broadcastFrame(packet.image, packet.telemetry, packet.sensor_ok);

        ESP_LOGD(TAG, "Frame %lu transmitted: IN=%d OUT=%d tracks=%d",
                 packet.telemetry.frame_id,
                 packet.telemetry.count_in,
                 packet.telemetry.count_out,
                 packet.telemetry.num_tracks);

        // --- Self-Monitoring: Profile Stack High Water Mark (every 100 packets) ---
        static uint32_t packet_count = 0;
        if (++packet_count >= 100) {
            packet_count = 0;
            UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGI(TAG, "Stack High Water Mark: %u words (%u bytes) free", 
                     (unsigned int)hwm, (unsigned int)(hwm * sizeof(StackType_t)));
        }
    }
}
