#pragma once
/**
 * @file log_writer.hpp
 * @brief Dedicated SD writer for CSV event logs.
 *
 * Decouples IPC-consuming tasks (TelemetryTask) from blocking SD I/O.
 * Writers push formatted CSV lines to a static queue; LogWriter drains
 * the queue and appends them to the daily CSV file on SD.
 *
 * All memory is static (pre-allocated during init). No runtime allocation.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <cstdint>
#include <cstdio>

class LogWriter {
public:
    static constexpr int QUEUE_DEPTH   = 32;
    static constexpr int MAX_LINE_LEN  = 256;
    static constexpr int MAX_PATH_LEN  = 48;

    /**
     * @brief Initialize the LogWriter: create queue + start writer task.
     * Must be called AFTER SD is mounted (g_sd.isMounted() == true).
     */
    static void init();

    /**
     * @brief Enqueue a CSV line for async SD write to a specific file.
     * Non-blocking: copies the line and pushes to queue.
     * If the queue is full, the line is silently dropped.
     * Safe to call from any FreeRTOS task context.
     * @param filename  Relative path under SD root, e.g. "logs/session_075/crossings.csv"
     * @param line      CSV line content (without trailing newline)
     */
    static void enqueue(const char* filename, const char* line);

private:
    struct LogLine {
        char path[MAX_PATH_LEN];
        char text[MAX_LINE_LEN];
    };

    static QueueHandle_t   s_queue;
    static StaticQueue_t   s_queue_buf;
    static uint8_t         s_queue_storage[QUEUE_DEPTH * sizeof(LogLine)];

    // Writer task
    static StaticTask_t    s_task_buf;
    static StackType_t     s_task_stack[4096 / sizeof(StackType_t)];

    static void task(void* pv);
};
