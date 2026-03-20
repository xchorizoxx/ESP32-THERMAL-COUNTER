#pragma once
/**
 * @file thermal_pipeline.hpp
 * @brief Step 5 — Orchestrator of the Thermal Vision Pipeline.
 *
 * This class coordinates the Background Subtraction, Peak Detection,
 * NMS, and Tracking phases. Runs on Core 1 at deterministic 16 Hz.
 */

#include "thermal_config.hpp"
#include "thermal_types.hpp"
#include "mlx90640_sensor.hpp"
#include "background_model.hpp"
#include "peak_detector.hpp"
#include "nms_suppressor.hpp"
#include "alpha_beta_tracker.hpp"
#include "mask_generator.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

class ThermalPipeline {
public:
    /**
     * @brief Constructor.
     * @param sensor      Initialized MLX90640 sensor.
     * @param ipcQueue    Queue for Core 0 dispatch.
     * @param configQueue Queue for receiving UI parameter updates.
     */
    ThermalPipeline(Mlx90640Sensor& sensor, QueueHandle_t ipcQueue, QueueHandle_t configQueue);

    /**
     * @brief Initialization of internal states.
     */
    void init();

    /**
     * @brief Static wrapper for FreeRTOS task creation.
     */
    static void TaskWrapper(void* pvParameters);

private:
    /**
     * @brief Main processing loop (16 Hz).
     */
    void run();

    Mlx90640Sensor& sensor_;
    QueueHandle_t   ipcQueue_;
    QueueHandle_t   configQueue_;
    AlphaBetaTracker tracker_;

    // Internal pipeline state
    float   current_frame_[ThermalConfig::TOTAL_PIXELS];
    float   display_frame_[ThermalConfig::TOTAL_PIXELS];
    float   background_map_[ThermalConfig::TOTAL_PIXELS];
    uint8_t blocking_mask_[ThermalConfig::TOTAL_PIXELS];
    
    ThermalPeak peaks_[ThermalConfig::MAX_PEAKS];
    int         num_peaks_ = 0;

    int      count_in_  = 0;
    int      count_out_ = 0;
    uint32_t frame_id_  = 0;
    bool     bg_init_   = false;
    bool     sensor_initialized_ = false;
};
