#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "thermal_types.hpp"
#include <stdint.h>

class HttpServer {
public:
    /**
     * @brief Time quality constants — broadcast in every binary frame (W3).
     */
    static constexpr uint8_t TIME_QUALITY_NONE    = 0; ///< No time reference available
    static constexpr uint8_t TIME_QUALITY_BROWSER = 1; ///< Unix time set by browser on connect
    static constexpr uint8_t TIME_QUALITY_RTC     = 2; ///< Reserved for DS3231 hardware RTC (W8)

    /**
     * @brief Binary WebSocket frame magic byte.
     *        0x12 = protocol v2 (includes session_id, time_quality, peak_temp_100 per track).
     *        Old clients sending 0x11 frames will be silently ignored by the new JS.
     */
    static constexpr uint8_t WS_FRAME_MAGIC = 0x12;

    /**
     * @brief Static buffer pool for async WebSocket broadcasting.
     *        W1-2: Increased from 2048 to 4096. A compile-time assert in
     *        http_server.cpp verifies the worst-case frame fits.
     */
    static constexpr size_t WS_BUFFER_COUNT = 4;
    static constexpr size_t WS_BUFFER_SIZE  = 4096;

    static uint8_t ws_buffers_[WS_BUFFER_COUNT][WS_BUFFER_SIZE];
    static int     ws_buffer_ref_counts_[WS_BUFFER_COUNT];

    /**
     * @brief Initialize HTTP + WebSocket server.
     * @param configQueue  FreeRTOS queue for sending AppConfigCmd to Core 1.
     */
    static esp_err_t start(QueueHandle_t configQueue = NULL);

    /**
     * @brief Stop the server and release all resources.
     */
    static void stop();

    /**
     * @brief Broadcast a binary frame to all connected WebSocket clients.
     *        Called from Core 1 pipeline dispatch at 16 Hz.
     *        Also updates the counter mirror used by the NVS save timer (W6).
     */
    static void broadcastFrame(const ImagePayload& img,
                               const TelemetryPayload& tel,
                               bool sensor_ok = true);

private:
    static httpd_handle_t server_;

    // --- HTTP handlers ---
    static esp_err_t indexGetHandler(httpd_req_t *req);
    static esp_err_t styleGetHandler(httpd_req_t *req);
    static esp_err_t appJsGetHandler(httpd_req_t *req);
    static esp_err_t wsHandler(httpd_req_t *req);
    static esp_err_t otaPostHandler(httpd_req_t *req);
    static esp_err_t rebootPostHandler(httpd_req_t *req); ///< W1-5: static fn, not lambda

    // --- WebSocket ---
    static void wsAsyncCompletionCb(esp_err_t err, int socket, void *arg);
    static void handleWebSocketMessage(httpd_req_t *req, httpd_ws_frame_t *ws_pkt);
};
