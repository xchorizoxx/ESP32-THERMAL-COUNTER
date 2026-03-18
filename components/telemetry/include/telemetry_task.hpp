#pragma once
/**
 * @file telemetry_task.hpp
 * @brief Tarea FreeRTOS de telemetría (Core 0).
 *
 * Recibe IpcPackets del Core 1 vía Queue y los transmite
 * por UDP (telemetría + imagen) al ESP32 receptor.
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
     * @param ipcQueue Queue compartida con el pipeline de Core 1
     * @param udp      Referencia al transmisor UDP ya inicializado
     */
    TelemetryTask(QueueHandle_t ipcQueue, UdpTransmitter& udp);

    /**
     * @brief Inicialización (log de configuración).
     */
    void init();

    /**
     * @brief Wrapper estático para FreeRTOS.
     * Usar en xTaskCreatePinnedToCore:
     *   xTaskCreatePinnedToCore(TelemetryTask::TaskWrapper, ..., &telemetry, ...);
     */
    static void TaskWrapper(void* pvParameters);

private:
    void run(); // Bucle bloqueante en xQueueReceive

    QueueHandle_t   ipcQueue_;
    UdpTransmitter& udp_;
};
