#pragma once
/**
 * @file telemetry_task.hpp
 * @brief FreeRTOS telemetry task (Core 0).
 *
 * Receives IpcPackets from Core 1 via Queue and transmits them
 * over UDP (telemetry + image) to the receiving ESP32.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "thermal_types.hpp"
#include "udp_transmitter.hpp"

class TelemetryTask {
public:
    /**
     * @brief Constructor.
     * @param ipcQueue Shared queue with Core 1 pipeline
     * @param udp      Reference to the already initialized UDP transmitter
     */
    TelemetryTask(QueueHandle_t ipcQueue, UdpTransmitter& udp);

    /**
     * @brief Initialization (configuration log).
     */
    void init();

    /**
     * @brief Static wrapper for FreeRTOS.
     * Use in xTaskCreatePinnedToCore:
     *   xTaskCreatePinnedToCore(TelemetryTask::TaskWrapper, ..., &telemetry, ...);
     */
    static void TaskWrapper(void* pvParameters);

private:
    void run(); // Blocking loop on xQueueReceive

    QueueHandle_t   ipcQueue_;
    UdpTransmitter& udp_;
};
