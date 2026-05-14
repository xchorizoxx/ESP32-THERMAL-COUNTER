/**
 * @file log_writer.cpp
 * @brief Implementation of LogWriter — async SD CSV writer.
 *
 * Writes to multiple files (crossings.csv, clips.csv) specified per-line
 * via enqueue(filename, line). Headers are prepended at boot by HttpServer.
 */

#include "log_writer.hpp"
#include "sd_manager.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>

static const char* TAG = "LOG_WRITER";

// Static member definitions
QueueHandle_t   LogWriter::s_queue        = nullptr;
StaticQueue_t   LogWriter::s_queue_buf;
uint8_t         LogWriter::s_queue_storage[QUEUE_DEPTH * sizeof(LogLine)];

StaticTask_t    LogWriter::s_task_buf;
StackType_t     LogWriter::s_task_stack[4096 / sizeof(StackType_t)];

// ---------------------------------------------------------------------------
//  Forward declaration from SD manager
// ---------------------------------------------------------------------------
extern SDManager g_sd;

// ---------------------------------------------------------------------------
//  init
// ---------------------------------------------------------------------------
void LogWriter::init()
{
    s_queue = xQueueCreateStatic(QUEUE_DEPTH, sizeof(LogLine),
                                 s_queue_storage, &s_queue_buf);
    if (!s_queue) {
        ESP_LOGE(TAG, "Failed to create queue — logging disabled");
        return;
    }

    TaskHandle_t hdl = xTaskCreateStaticPinnedToCore(
        task, "LogWriter",
        4096 / sizeof(StackType_t), nullptr,
        tskIDLE_PRIORITY + 1, s_task_stack, &s_task_buf, 0);
    if (!hdl) {
        ESP_LOGE(TAG, "Failed to create writer task — logging disabled");
        s_queue = nullptr;
        return;
    }
    ESP_LOGI(TAG, "LogWriter initialized (queue=%d slots, task on Core 0)", QUEUE_DEPTH);
}

// ---------------------------------------------------------------------------
//  enqueue  — non-blocking, drops if full
// ---------------------------------------------------------------------------
void LogWriter::enqueue(const char* fname, const char* line)
{
    if (!s_queue) return;

    LogLine entry;
    size_t plen = strlen(fname);
    if (plen >= MAX_PATH_LEN) plen = MAX_PATH_LEN - 1;
    memcpy(entry.path, fname, plen);
    entry.path[plen] = '\0';

    size_t len = strlen(line);
    if (len >= MAX_LINE_LEN) len = MAX_LINE_LEN - 1;
    memcpy(entry.text, line, len);
    entry.text[len] = '\0';

    if (xQueueSend(s_queue, &entry, 0) != pdTRUE) {
        static uint32_t s_last_drop_log = 0;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
        if (now - s_last_drop_log > 5000) {
            ESP_LOGW(TAG, "Queue full — dropping log line");
            s_last_drop_log = now;
        }
    }
}

// ---------------------------------------------------------------------------
//  task  — drains queue, writes to SD
// ---------------------------------------------------------------------------
void LogWriter::task(void* pv)
{
    (void)pv;
    LogLine entry;

    // Headers are enqueued by HttpServer::start() before any data lines

    while (true) {
        if (xQueueReceive(s_queue, &entry, portMAX_DELAY) == pdTRUE) {
            g_sd.appendLine(entry.path, entry.text);
        }
    }
}
