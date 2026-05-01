/**
 * @file telemetry_task.cpp
 * @brief Implementation of the telemetry task (Core 0).
 *
 * Consumes IpcPackets from Core 1 queue and transmits them via UDP.
 */

#include "telemetry_task.hpp"
#include "esp_log.h"
#include "esp_task_wdt.h"  
#include "http_server.hpp" 
#include "esp_netif.h"     // [NEW] To check interface status
#include <errno.h>         // [FIX] Required for errno usage in logs

static const char* TAG = "TELEMETRY";

TelemetryTask::TelemetryTask(QueueHandle_t ipcQueue, UdpTransmitter& udp)
    : ipcQueue_(ipcQueue)
    , udp_(udp)
{
}

void TelemetryTask::init()
{
    // [MAGENTA] Network-related log (Full line)
    ESP_LOG_COLOR(LOG_COLOR_MAGENTA, TAG, "TelemetryTask initialized (queue=%p)", ipcQueue_);
}

void TelemetryTask::TaskWrapper(void* pvParameters)
{
    // P05-fix: Register this task with the WDT to prevent zombie-task on network freeze.
    esp_err_t wdt_err = esp_task_wdt_add(NULL);
    if (wdt_err != ESP_OK && wdt_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "WDT register failed: %s — task will run unmonitored", esp_err_to_name(wdt_err));
    }

    auto* self = static_cast<TelemetryTask*>(pvParameters);
    self->run();

    esp_task_wdt_delete(NULL);
    ESP_LOGE(TAG, "TelemetryTask::run() returned unexpectedly — deleting task");
    vTaskDelete(NULL);
}

void TelemetryTask::run()
{
    static IpcPacket packet; // static: avoids allocating ~1.6 KB on the task stack

    // [MAGENTA] Network-related log (Full line)
    ESP_LOG_COLOR(LOG_COLOR_MAGENTA, TAG, "Telemetry task started on Core %d", xPortGetCoreID());

    while (true) {
        // WDT reset FIRST — before any potentially blocking I/O (like HTTP locks)
        esp_task_wdt_reset();

        // Block until receiving a packet from the pipeline (Core 1)
        // Timeout 100ms guarantees the WDT gets reset even if queue is empty
        BaseType_t received = xQueueReceive(ipcQueue_, &packet, pdMS_TO_TICKS(100));
        if (received != pdTRUE) {
            continue;
        }

        // Check if ANY netif is up before trying to send via UDP
        // This stops the log flood if there's no network link.
        /* 
        // [W4-CLEANUP] network_ready unused while UDP is disabled
        bool network_ready = false;
        esp_netif_t* netif = nullptr;
        while ((netif = esp_netif_next_unsafe(netif)) != nullptr) {
            if (esp_netif_is_netif_up(netif)) {
                network_ready = true;
                break;
            }
        }
        */

        // [FIX] Disable UDP broadcasts globally.
        // Broadcasting huge thermal frames (1500+ bytes) to 255.255.255.255 saturates the 
        // TinyUSB NCM driver queue and prevents essential network traffic (like DHCP replies)
        // from going out. The Web UI uses WebSockets, not UDP.
        /*
        if (network_ready) {
            // Send telemetry (counters + tracks)
            esp_err_t err = udp_.sendTelemetry(packet.telemetry);
            if (err != ESP_OK) {
                ESP_LOGD(TAG, "Error sending telemetry (frame %lu) - errno=%d",
                         packet.telemetry.frame_id, errno);
            }

            // Send thermal image
            err = udp_.sendImage(packet.image);
            if (err != ESP_OK) {
                ESP_LOGD(TAG, "Error sending image (frame %lu) - errno=%d",
                         packet.image.frame_id, errno);
            }
        }
        */

        // [NEW] Send via WebSocket to HTTP clients
        HttpServer::broadcastFrame(packet.image, packet.telemetry, packet.sensor_ok);

        // W4-CSV: Broadcast individual crossing events as JSON for precise logging
        for (int i = 0; i < packet.telemetry.num_events; i++) {
            HttpServer::broadcastEvent(packet.telemetry.events[i]);
        }

        // WDT reset moved to top of loop
        // esp_task_wdt_reset();

        ESP_LOGD(TAG, "Frame %lu transmitted: IN=%d OUT=%d tracks=%d",
                 packet.telemetry.frame_id,
                 packet.telemetry.count_in,
                 packet.telemetry.count_out,
                 packet.telemetry.num_tracks);

        // --- Self-Monitoring: Profile Stack & Heap (every 100 packets) ---
        /* 
        static uint32_t packet_count = 0;
        if (++packet_count >= 100) {
            packet_count = 0;
            UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
            // [WHITE] Memory monitoring (Full line, commented)
            // ESP_LOG_COLOR(LOG_COLOR_WHITE, TAG, "Health: Stack=%u bytes free, Heap=%u bytes free", 
            //               (unsigned int)(hwm * sizeof(StackType_t)),
            //               (unsigned int)esp_get_free_heap_size());
        }
        */
    }
}
