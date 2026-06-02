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
#include <ctime>
#include "tft_snapshot.hpp"
#include "tft_system_snapshot.hpp"
#include "rtc_driver.hpp"
#include "sd_manager.hpp"
#include "esp_wifi.h"

extern RTCDriver g_rtc;
extern SDManager g_sd;

static const char* TAG = "TELEMETRY";

TelemetryTask::TelemetryTask(QueueHandle_t ipcQueue)
    : ipcQueue_(ipcQueue)
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
        static float    last_ambient     = 0.0f;
        static float    last_sensor_temp = 0.0f;
        static bool     last_sensor_ok   = false;
        static uint32_t last_frame_id    = 0;
        esp_task_wdt_reset();

        // --- System snapshot update every ~1s (independent of IPC packet rate) ---
        static uint32_t last_sys_update_ms = 0;
        uint32_t now_ms_sys = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        if (now_ms_sys - last_sys_update_ms >= 1000 || last_sys_update_ms == 0) {
            last_sys_update_ms = now_ms_sys;
            SystemInfoSnapshot info = {};

            bool has_time = false;
            if (g_rtc.isAvailable()) {
                RTCDriver::DateTime dt;
                if (g_rtc.getTime(dt) == ESP_OK) {
                    snprintf(info.time_str, sizeof(info.time_str), "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
                    snprintf(info.date_str, sizeof(info.date_str), "%02d/%02d/%02d", dt.day, dt.month, dt.year % 100);
                    has_time = true;
                }
            }

            if (!has_time && HttpServer::isTimeValid()) {
                uint64_t now_ms = HttpServer::getSystemTimeMs();
                time_t seconds = now_ms / 1000;
                struct tm tm_info;
                localtime_r(&seconds, &tm_info);
                
                // Use intermediate buffers and clamp range to avoid compiler warning about string truncation (-Werror=format-truncation)
                char t_buf[32];
                char d_buf[32];
                snprintf(t_buf, sizeof(t_buf), "%02d:%02d:%02d", 
                         (int)((tm_info.tm_hour % 24 + 24) % 24), 
                         (int)((tm_info.tm_min % 60 + 60) % 60), 
                         (int)((tm_info.tm_sec % 60 + 60) % 60));
                snprintf(d_buf, sizeof(d_buf), "%02d/%02d/%02d", 
                         (int)((tm_info.tm_mday % 32 + 32) % 32), 
                         (int)(((tm_info.tm_mon + 1) % 13 + 13) % 13), 
                         (int)((tm_info.tm_year % 100 + 100) % 100));
                
                strncpy(info.time_str, t_buf, sizeof(info.time_str) - 1);
                info.time_str[sizeof(info.time_str) - 1] = '\0';
                strncpy(info.date_str, d_buf, sizeof(info.date_str) - 1);
                info.date_str[sizeof(info.date_str) - 1] = '\0';
                has_time = true;
            }

            info.rtc_ok = has_time;

            info.sd_ok = g_sd.isMounted();
            if (info.sd_ok) {
                uint64_t free_b = g_sd.getFreeSpaceBytes();
                uint64_t total_b = g_sd.getTotalSpaceBytes();
                info.sd_free_kb = (uint32_t)(free_b / 1024);
                info.sd_total_kb = (uint32_t)(total_b / 1024);
            }

            {
                wifi_sta_list_t sta_list;
                memset(&sta_list, 0, sizeof(sta_list));
                if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
                    info.wifi_clients = sta_list.num;
                }
            }

            info.uptime_sec = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
            info.ambient_temp = last_ambient;
            info.sensor_temp  = last_sensor_temp;
            info.sensor_ok    = last_sensor_ok;
            info.update_id    = last_frame_id;

            writeSystemSnapshot(info);
        }

        // Block until receiving a packet from the pipeline (Core 1)
        BaseType_t received = xQueueReceive(ipcQueue_, &packet, pdMS_TO_TICKS(100));
        if (received != pdTRUE) {
            continue;
        }

        // [FIX] UDP broadcasts are disabled globally.
        // Broadcasting huge thermal frames (1500+ bytes) to 255.255.255.255 saturates the 
        // TinyUSB NCM driver queue and prevents essential network traffic (like DHCP replies)
        // from going out. The Web UI uses WebSockets, not UDP.

        // [NEW] Send via WebSocket to HTTP clients
        HttpServer::broadcastFrame(packet.image, packet.telemetry, packet.sensor_ok);

        {
            ThermalConfig::DoorLineConfig dl;
            portENTER_CRITICAL(&ThermalConfig::door_lines_mux);
            dl = ThermalConfig::door_lines;
            portEXIT_CRITICAL(&ThermalConfig::door_lines_mux);
            TftBridge::writeSnapshot(packet.image, packet.telemetry, packet.sensor_ok, dl);
        }

        // Store sensor data for system snapshot updates
        last_ambient     = packet.telemetry.ambient_temp;
        last_sensor_temp = packet.telemetry.sensor_temp;
        last_sensor_ok   = packet.sensor_ok;
        last_frame_id  = packet.telemetry.frame_id;

        // W4-CSV: Broadcast individual crossing events as JSON for precise logging
        for (int i = 0; i < packet.telemetry.num_events; i++) {
            HttpServer::broadcastEvent(packet.telemetry.events[i], packet.telemetry.ambient_temp, packet.telemetry.num_tracks);
        }

        // WDT reset moved to top of loop
        // esp_task_wdt_reset();

        ESP_LOGD(TAG, "Frame %lu transmitted: IN=%d OUT=%d tracks=%d",
                 packet.telemetry.frame_id,
                 packet.telemetry.count_in,
                 packet.telemetry.count_out,
                 packet.telemetry.num_tracks);

        // --- Self-Monitoring: Stack HWM + Heap every 320 frames (~10s at 32Hz) ---
        static uint32_t packet_count = 0;
        if (++packet_count >= 320) {
            packet_count = 0;
            UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
            (void)hwm;
            // ESP_LOGI(TAG, "Health: Stack HWM=%u B free | Heap=%u B free",
            //          (unsigned int)(hwm * sizeof(StackType_t)),
            //          (unsigned int)esp_get_free_heap_size());
        }
    }
}
