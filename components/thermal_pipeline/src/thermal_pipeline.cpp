/**
 * @file thermal_pipeline.cpp
 * @brief Implementation of the thermal vision pipeline orchestrator.
 *
 * Executes the 5 steps at 16 Hz on Core 1 with vTaskDelayUntil,
 * and dispatches results to Core 0 via FreeRTOS Queue.
 */

#include "thermal_pipeline.hpp"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include <cstring>
#include <cmath>

namespace ThermalConfig {
    float EMA_ALPHA = 0.05f;
    float TEMP_BIOLOGICO_MIN = 25.0f;
    float DELTA_T_FONDO = 1.5f;
    int NMS_RADIUS_CENTER_SQ = 16;
    int NMS_RADIUS_EDGE_SQ = 4;
    int DEFAULT_LINE_ENTRY_Y = 11;
    int DEFAULT_LINE_EXIT_Y = 13;
    int VIEW_MODE = 0;
    bool APP_RESET_COUNTS = false;
    bool APP_RETRY_SENSOR = false;
}

static const char* TAG = "PIPELINE";

ThermalPipeline::ThermalPipeline(Mlx90640Sensor& sensor, QueueHandle_t ipcQueue, QueueHandle_t configQueue)
    : sensor_(sensor)
    , ipcQueue_(ipcQueue)
    , configQueue_(configQueue)
    , tracker_()
    , num_objetivos_(0)
    , count_in_(0)
    , count_out_(0)
    , frame_id_(0)
    , fondoInit_(false)
    , sensor_initialized_(false)
{
    memset(frame_actual_,    0, sizeof(frame_actual_));
    memset(frame_display_,   0, sizeof(frame_display_));
    memset(mapa_fondo_,      0, sizeof(mapa_fondo_));
    memset(mascara_bloqueo_, 0, sizeof(mascara_bloqueo_));
    memset(objetivos_crudos_, 0, sizeof(objetivos_crudos_));
}

void ThermalPipeline::init()
{
    ESP_LOGI(TAG, "Pipeline initialized (target: %d Hz, static stack: %.1f KB)",
             ThermalConfig::PIPELINE_FREQ_HZ,
             (float)(sizeof(frame_actual_) + sizeof(mapa_fondo_) +
                     sizeof(mascara_bloqueo_)) / 1024.0f);
}

void ThermalPipeline::TaskWrapper(void* pvParameters)
{
    auto* self = static_cast<ThermalPipeline*>(pvParameters);

    // Register this task with the system Watchdog
    esp_task_wdt_add(NULL);

    self->run();

    // Should never reach here
    vTaskDelete(NULL);
}

void ThermalPipeline::run()
{
    const TickType_t period = pdMS_TO_TICKS(1000 / ThermalConfig::PIPELINE_FREQ_HZ); // 62 ms
    TickType_t lastWakeTime = xTaskGetTickCount();

    ESP_LOGI(TAG, "Pipeline started on Core %d at %d Hz",
             xPortGetCoreID(), ThermalConfig::PIPELINE_FREQ_HZ);

    // Initialize local state based on actual sensor state at boot
    sensor_initialized_ = sensor_.isInitialized();

    while (true) {
        // ============================================================
        // STEP 0.1: Process configuration commands from UI (Thread-Safe)
        // ============================================================
        AppConfigCmd cmd;
        while (configQueue_ && xQueueReceive(configQueue_, &cmd, 0) == pdTRUE) {
            switch (cmd.type) {
                case ConfigCmdType::SET_TEMP_BIO:    ThermalConfig::TEMP_BIOLOGICO_MIN = cmd.value; break;
                case ConfigCmdType::SET_DELTA_T:     ThermalConfig::DELTA_T_FONDO = cmd.value; break;
                case ConfigCmdType::SET_EMA_ALPHA:   ThermalConfig::EMA_ALPHA = cmd.value; break;
                case ConfigCmdType::SET_LINE_ENTRY:  ThermalConfig::DEFAULT_LINE_ENTRY_Y = (int)cmd.value; break;
                case ConfigCmdType::SET_LINE_EXIT:   ThermalConfig::DEFAULT_LINE_EXIT_Y = (int)cmd.value; break;
                case ConfigCmdType::SET_NMS_CENTER:  ThermalConfig::NMS_RADIUS_CENTER_SQ = (int)cmd.value; break;
                case ConfigCmdType::SET_NMS_EDGE:    ThermalConfig::NMS_RADIUS_EDGE_SQ = (int)cmd.value; break;
                case ConfigCmdType::SET_VIEW_MODE:   ThermalConfig::VIEW_MODE = (int)cmd.value; break;
                case ConfigCmdType::RESET_COUNTS:
                    count_in_ = 0;
                    count_out_ = 0;
                    ESP_LOGI(TAG, "Counters reset");
                    break;
                case ConfigCmdType::RETRY_SENSOR:
                    ESP_LOGI(TAG, "Attempting to initialize sensor per web request...");
                    if (sensor_.init() == ESP_OK && sensor_.setRefreshRate(0x05) == ESP_OK) {
                        sensor_initialized_ = true;
                        ESP_LOGI(TAG, "Sensor initialized successfully");
                    } else {
                        sensor_initialized_ = false;
                        ESP_LOGE(TAG, "Failed to initialize sensor");
                    }
                    break;
                case ConfigCmdType::SAVE_CONFIG:   // Handled in Core 0 (http_server) — no-op here
                case ConfigCmdType::APPLY_CONFIG:  // Handled in Core 0 (http_server) — no-op here
                    break;
            }
        }

        // ============================================================
        // STEP 0: Frame acquisition from sensor
        // ============================================================
        bool sensor_ok = false;
        if (sensor_initialized_) {
            esp_err_t readErr = sensor_.readFrame(frame_actual_);
            if (readErr == ESP_OK) {
                sensor_ok = true;
            } else {
                ESP_LOGW(TAG, "Frame %lu: read error (probablemente sensor disconnected)", frame_id_);
                sensor_initialized_ = false; // Forces requiring a new init
            }
        }

        if (sensor_ok) {
            // ============================================================
            // STEP 0.4: Integrity Guard (Massive Glitch Detection)
            // ============================================================
            static float last_avg = 0;
            float current_avg = 0;
            for(int i=0; i<32; i++) current_avg += frame_actual_[i*10]; // Fast sampling
            current_avg /= 32.0f;

            if (frame_id_ > 10 && fabsf(current_avg - last_avg) > 15.0f) {
                ESP_LOGW(TAG, "Thermal glitch detected (Delta > 15C). Retrying...");
                sensor_ok = false; // Discards this subframe
            }
            last_avg = current_avg;
        }

        if (sensor_ok) {
            // ============================================================
            // STEP 0.5: Spatial Filter and Subpage Reconstruction
            // ============================================================
            float max_temp = -100.0f;
            float min_temp = 100.0f;
            uint8_t currentSubPage = sensor_.getLastSubPageID();
            
            static float filtered_frame[ThermalConfig::TOTAL_PIXELS];
            
            for (int r = 0; r < ThermalConfig::MLX_ROWS; r++) {
                for (int c = 0; c < ThermalConfig::MLX_COLS; c++) {
                    int idx = r * ThermalConfig::MLX_COLS + c;
                    float val = frame_actual_[idx];
                    
                    if (val > max_temp) max_temp = val;
                    if (val < min_temp) min_temp = val;

                    bool is_new_pixel = ((r + c) % 2) == currentSubPage;

                    if (!is_new_pixel) {
                        // This is a pixel from the "old" subpage.
                        // Instead of always overwriting it, we average it gently
                        // with its new neighbors to reduce the checkerboard effect.
                        if (r > 0 && r < ThermalConfig::MLX_ROWS - 1 && c > 0 && c < ThermalConfig::MLX_COLS - 1) {
                            float neighbors = (frame_actual_[(r-1)*32+c] + frame_actual_[(r+1)*32+c] + 
                                               frame_actual_[r*32+(c-1)] + frame_actual_[r*32+(c+1)]) / 4.0f;
                            // 70% previous value (to not lose detail) + 30% neighbors (for smoothing)
                            val = (val * 0.7f) + (neighbors * 0.3f);
                        }
                    } 
                    filtered_frame[idx] = val;
                }
            }
            
            for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) frame_actual_[i] = filtered_frame[i];

            // ============================================================
            // STEP 0.6: Visualization Image (Exact 8 FPS Sync)
            // ============================================================
            // Only update display buffer when the full board is complete (subpage 1).
            if (currentSubPage == 1) {
                memcpy(frame_display_, frame_actual_, sizeof(frame_display_));
            }
            // NOTE: Algorithm processing (STEP 1 onwards) continues at 16Hz
            // to maintain tracking precision.

            // ============================================================
            // STEP 1: Dynamic Background — Selective EMA
            // ============================================================
            if (!fondoInit_) {
                // First frame: initialize background with raw data
                BackgroundModel::initialize(frame_actual_, mapa_fondo_,
                                             ThermalConfig::TOTAL_PIXELS);
                fondoInit_ = true;
                ESP_LOGI(TAG, "Background map initialized with the first frame");
            } else {
                BackgroundModel::update(frame_actual_, mapa_fondo_, mascara_bloqueo_,
                                         ThermalConfig::TOTAL_PIXELS, ThermalConfig::EMA_ALPHA);
            }
            
            // Check for completely uniform surface noise
            bool is_uniform = (max_temp - min_temp) < 1.0f;

            // ============================================================
            // STEP 2: Topology — Peak Detection
            // ============================================================
            num_objetivos_ = 0;
            if (!is_uniform) {
                PeakDetector::detect(frame_actual_, mapa_fondo_,
                                      objetivos_crudos_, &num_objetivos_,
                                      ThermalConfig::TEMP_BIOLOGICO_MIN,
                                      ThermalConfig::DELTA_T_FONDO,
                                      ThermalConfig::MAX_OBJETIVOS);
            }

            // ============================================================
            // STEP 3: Adaptive NMS
            // ============================================================
            NmsSuppressor::suppress(objetivos_crudos_, num_objetivos_,
                                     ThermalConfig::NMS_RADIUS_CENTER_SQ,
                                     ThermalConfig::NMS_RADIUS_EDGE_SQ,
                                     ThermalConfig::NMS_CENTER_X_MIN,
                                     ThermalConfig::NMS_CENTER_X_MAX);

            // ============================================================
            // STEP 4: Alpha-Beta Tracking + Counting
            // ============================================================
            tracker_.update(objetivos_crudos_, num_objetivos_,
                             ThermalConfig::ALPHA_TRK, ThermalConfig::BETA_TRK,
                             ThermalConfig::MAX_MATCH_DIST_SQ, ThermalConfig::TRACK_MAX_AGE,
                             ThermalConfig::DEFAULT_LINE_ENTRY_Y,
                             ThermalConfig::DEFAULT_LINE_EXIT_Y,
                             count_in_, count_out_);

            // ============================================================
            // STEP 5: Feedback — Mask Generation
            // ============================================================
            MaskGenerator::generate(tracker_.getTracks(), tracker_.getMaxTracks(),
                                     mascara_bloqueo_, ThermalConfig::MASK_HALF_SIZE);
        }

        // ============================================================
        // DISPATCH: Build and send IpcPacket to Core 0
        // ============================================================
        static IpcPacket packet;

        // --- Telemetry ---
        packet.sensor_ok             = sensor_ok;
        packet.telemetria.ambient_temp = sensor_.getAmbientTemp();
        packet.telemetria.frame_id   = frame_id_;
        packet.telemetria.count_in   = (int16_t)count_in_;
        packet.telemetria.count_out  = (int16_t)count_out_;
        packet.telemetria.num_tracks = (uint8_t)(sensor_ok ? tracker_.getActiveCount() : 0);

        const Track* tracks = tracker_.getTracks();
        int tidx = 0;
        for (int i = 0; i < tracker_.getMaxTracks() && tidx < ThermalConfig::MAX_TRACKS; i++) {
            if (tracks[i].activo) {
                packet.telemetria.tracks[tidx].id      = tracks[i].id;
                packet.telemetria.tracks[tidx].x_100   = (int16_t)(tracks[i].x * 100.0f);
                packet.telemetria.tracks[tidx].y_100   = (int16_t)(tracks[i].y * 100.0f);
                packet.telemetria.tracks[tidx].v_x_100 = (int16_t)(tracks[i].v_x * 100.0f);
                packet.telemetria.tracks[tidx].v_y_100 = (int16_t)(tracks[i].v_y * 100.0f);
                tidx++;
            }
        }

        // --- Thermal Image (pixels as int16 × 100) ---
        packet.imagen.frame_id = frame_id_;
        if (ThermalConfig::VIEW_MODE == 1) { // Subtractor Mode
            for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
                float diff = frame_actual_[i] - mapa_fondo_[i];
                // Noise Gate: Ignore thermal noise less than 0.6°C
                if (diff < 0.6f && diff > -0.6f) diff = 0.0f;
                packet.imagen.pixels[i] = (int16_t)(diff * 100.0f);
            }
        } else { // Normal Mode
            for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
                float val = frame_display_[i];
                float bg = mapa_fondo_[i];
                // Noise Gate in normal mode: If difference from background is minimal, show pure background
                if (val - bg < 0.4f && val - bg > -0.4f) val = bg; 
                packet.imagen.pixels[i] = (int16_t)(val * 100.0f);
            }
        }

        // Non-blocking send: if queue is full, discard frame
        BaseType_t sent = xQueueSend(ipcQueue_, &packet, 0);
        if (sent != pdTRUE) {
            ESP_LOGW(TAG, "Frame %lu: IPC Queue full, frame discarded", frame_id_);
        }

        ESP_LOGD(TAG, "Frame %lu: %d peaks, %d tracks, IN=%d, OUT=%d",
                 frame_id_, num_objetivos_, tracker_.getActiveCount(),
                 count_in_, count_out_);

        frame_id_++;

        // ============================================================
        // CYCLE END: Reset WDT + deterministic wait
        // ============================================================
        esp_task_wdt_reset();
        vTaskDelayUntil(&lastWakeTime, period);
    }
}
