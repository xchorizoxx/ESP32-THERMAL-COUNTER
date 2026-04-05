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
#include "tracklet_tracker.hpp"  // A2: replaces AlphaBetaTracker
#include "tracklet_fsm.hpp"      // A3: counting logic
#include "mask_generator.hpp"
#include "frame_accumulator.hpp"  // A1: chess sub-frame compositor
#include "noise_filter.hpp"        // A1: Kalman 1D per-pixel noise filter
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>

static inline int16_t sat16(int v) {
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return (int16_t)v;
}

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
    TrackletTracker tracker_;                                // A2
    TrackletFSM     door_fsm_;                               // A3: Unified Bitmap FSM Count
    Track           track_array_[ThermalConfig::MAX_TRACKS]; // A2: filled by fillTrackArray()
    int             num_confirmed_tracks_ = 0;               // A2: count of confirmed tracks

    // A1: chess corrector + noise filter
    FrameAccumulator frame_accumulator_;
    NoiseFilter      noise_filter_;

    // Internal pipeline state
    float   current_frame_[ThermalConfig::TOTAL_PIXELS];   // Raw frame from sensor
    float   composed_frame_[ThermalConfig::TOTAL_PIXELS];  // A1: chess-fused frame
    float   filtered_frame_[ThermalConfig::TOTAL_PIXELS];  // A1: Kalman-filtered frame
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
