#pragma once
/**
 * @file thermal_pipeline.hpp
 * @brief Thermal vision pipeline orchestrator (Core 1).
 *
 * Executes the 5 steps of the pipeline at 16 Hz using vTaskDelayUntil,
 * manages the Watchdog, and sends results to Core 0 via FreeRTOS Queue.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "mlx90640_sensor.hpp"
#include "thermal_types.hpp"
#include "background_model.hpp"
#include "peak_detector.hpp"
#include "nms_suppressor.hpp"
#include "alpha_beta_tracker.hpp"
#include "mask_generator.hpp"
#include "thermal_config.hpp"

class ThermalPipeline {
public:
    /**
     * @brief Constructor.
     * @param sensor      Reference to the already initialized sensor
     * @param ipcQueue    Queue towards Core 0 (size IPC_QUEUE_DEPTH × sizeof(IpcPacket))
     * @param configQueue Queue from Core 0 to receive configuration commands
     */
    ThermalPipeline(Mlx90640Sensor& sensor, QueueHandle_t ipcQueue, QueueHandle_t configQueue);

    /**
     * @brief Initializes the task: configures Watchdog.
     * Call BEFORE xTaskCreatePinnedToCore.
     */
    void init();

    /**
     * @brief Static wrapper for FreeRTOS (passes `this` as parameter).
     * Use in xTaskCreatePinnedToCore:
     *   xTaskCreatePinnedToCore(ThermalPipeline::TaskWrapper, ..., &pipeline, ...);
     */
    static void TaskWrapper(void* pvParameters);

private:
    /**
     * @brief Main pipeline loop. Never returns.
     * Timing: vTaskDelayUntil at 1000/PIPELINE_FREQ_HZ ms.
     */
    void run();

    Mlx90640Sensor&  sensor_;
    QueueHandle_t    ipcQueue_;
    QueueHandle_t    configQueue_;
    AlphaBetaTracker tracker_;

    // ---------- Static buffers (no malloc at runtime) ----------
    float   frame_actual_[ThermalConfig::TOTAL_PIXELS];
    float   frame_display_[ThermalConfig::TOTAL_PIXELS];  ///< Temporal EMA for display visualization only
    float   mapa_fondo_[ThermalConfig::TOTAL_PIXELS];
    uint8_t mascara_bloqueo_[ThermalConfig::TOTAL_PIXELS];
    PicoTermico objetivos_crudos_[ThermalConfig::MAX_OBJETIVOS];

    int      num_objetivos_  = 0;
    int      count_in_       = 0;
    int      count_out_      = 0;
    uint32_t frame_id_       = 0;
    bool     fondoInit_      = false; // if background was already initialized with the 1st frame
    bool     displayInit_    = false; // if display buffer was initialized
    bool     sensor_initialized_ = false;
};
