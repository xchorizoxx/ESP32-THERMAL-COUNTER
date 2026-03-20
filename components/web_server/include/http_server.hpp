#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "thermal_types.hpp"


class HttpServer {
public:
    /**
     * @brief Initialize and start the HTTP and WebSocket server.
     */
    static esp_err_t start(QueueHandle_t configQueue = NULL);

    /**
     * @brief Stop the server and clean up resources.
     */
    static void stop();

    /**
     * @brief Broadcast a frame containing image and telemetry data to all connected WebSocket clients.
     */
    static void broadcastFrame(const ImagePayload& img, const TelemetryPayload& tel, bool sensor_ok = true);

private:
    static httpd_handle_t server_;

    // Static buffer system to avoid malloc/free during broadcast
    static constexpr size_t WS_BUFFER_COUNT = 2;
    static constexpr size_t WS_BUFFER_SIZE  = 2048;
    static uint8_t ws_buffers_[WS_BUFFER_COUNT][WS_BUFFER_SIZE];
    static int     ws_buffer_ref_counts_[WS_BUFFER_COUNT];

    /**
     * @brief Callback invoked when asynchronous transmission finishes.
     */
    static void wsAsyncCompletionCb(esp_err_t err, int socket, void *arg);

    // HTTP Handlers
    static esp_err_t indexGetHandler(httpd_req_t *req);
    static esp_err_t wsHandler(httpd_req_t *req);
    static esp_err_t otaPostHandler(httpd_req_t *req);
    
    // Internal handler for incoming WebSocket JSON messages
    static void handleWebSocketMessage(httpd_req_t *req, httpd_ws_frame_t *ws_pkt);
};
