#include "thermal_pipeline.hpp"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "fov_correction.hpp"
#include <cstring>
#include <cmath>

namespace ThermalConfig {
    float EMA_ALPHA = 0.05f;
    float BIOLOGICAL_TEMP_MIN = 25.0f;
    float BACKGROUND_DELTA_T = 1.5f;
    float SENSOR_HEIGHT_M = 3.0f;
    float PERSON_DIAMETER_M = 0.7f;
    int DEFAULT_LINE_ENTRY_Y = 11;
    int DEFAULT_LINE_EXIT_Y = 13;
    int DEFAULT_DEAD_ZONE_LEFT = 0;
    int DEFAULT_DEAD_ZONE_RIGHT = 31;
    int VIEW_MODE = 0;
    DoorLineConfig door_lines = { .lines = {}, .num_lines = 0, .use_segments = false };
    // P02-fix: Spinlock dedicado para acceso cross-core a door_lines.
    portMUX_TYPE door_lines_mux = portMUX_INITIALIZER_UNLOCKED;
}

static const char* TAG = "PIPELINE";

ThermalPipeline::ThermalPipeline(Mlx90640Sensor& sensor, QueueHandle_t ipcQueue, QueueHandle_t configQueue)
    : sensor_(sensor)
    , ipcQueue_(ipcQueue)
    , configQueue_(configQueue)
    , tracker_()
    , num_peaks_(0)
    , count_in_(0)
    , count_out_(0)
    , frame_id_(0)
    , bg_init_(false)
    , sensor_initialized_(false)
{
    memset(current_frame_,   0, sizeof(current_frame_));
    memset(composed_frame_,  0, sizeof(composed_frame_));
    memset(filtered_frame_,  0, sizeof(filtered_frame_));
    memset(background_map_,  0, sizeof(background_map_));
    memset(blocking_mask_,   0, sizeof(blocking_mask_));
    memset(peaks_,           0, sizeof(peaks_));
}

void ThermalPipeline::init()
{
    FovCorrection::init(ThermalConfig::SENSOR_HEIGHT_M);
    ESP_LOGI(TAG, "Pipeline initialized (Standard stack: %.1f KB)",
             (float)(sizeof(current_frame_) + sizeof(background_map_) + sizeof(blocking_mask_)) / 1024.0f);
}

void ThermalPipeline::TaskWrapper(void* pvParameters)
{
    auto* self = static_cast<ThermalPipeline*>(pvParameters);
    esp_task_wdt_add(NULL);
    self->run();
    vTaskDelete(NULL);
}

void ThermalPipeline::run()
{
    // --- Phase 0: Verify sensor was pre-initialized by app_main() (Bug1 fix) ---
    // The pipeline does NOT call sensor_.init() here. Calling DumpEE twice
    // would waste one of the MLX90640's ~10 lifetime EEPROM write cycles.
    sensor_initialized_ = sensor_.isInitialized();
    if (!sensor_initialized_) {
        ESP_LOGE(TAG, "Sensor not initialized by app_main -- pipeline in degraded mode (retry via UI)");
    } else {
        ESP_LOGI(TAG, "Pipeline ready -- sensor pre-initialized by app_main");
    }

    // Thermal stabilization wait (sensor already powered, let readings settle)
    ESP_LOGI(TAG, "Waiting 2000ms for thermal stabilization...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    const TickType_t period = pdMS_TO_TICKS(1000 / ThermalConfig::PIPELINE_FREQ_HZ);
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (true) {
        // --- Phase 0.1: UI Config Updates ---
        AppConfigCmd cmd;
        while (configQueue_ && xQueueReceive(configQueue_, &cmd, 0) == pdTRUE) {
            switch (cmd.type) {
                case ConfigCmdType::SET_TEMP_BIO:
                    ThermalConfig::BIOLOGICAL_TEMP_MIN = cmd.value;
                    break;
                case ConfigCmdType::SET_DELTA_T:
                    ThermalConfig::BACKGROUND_DELTA_T = cmd.value;
                    break;
                case ConfigCmdType::SET_EMA_ALPHA:
                    ThermalConfig::EMA_ALPHA = cmd.value;
                    break;
                case ConfigCmdType::SET_LINE_ENTRY:
                    ThermalConfig::DEFAULT_LINE_ENTRY_Y = (int)cmd.value;
                    break;
                case ConfigCmdType::SET_LINE_EXIT:
                    ThermalConfig::DEFAULT_LINE_EXIT_Y = (int)cmd.value;
                    break;
                case ConfigCmdType::SET_DEAD_LEFT:
                    ThermalConfig::DEFAULT_DEAD_ZONE_LEFT = (int)cmd.value;
                    break;
                case ConfigCmdType::SET_DEAD_RIGHT:
                    ThermalConfig::DEFAULT_DEAD_ZONE_RIGHT = (int)cmd.value;
                    break;
                case ConfigCmdType::SET_SENSOR_HEIGHT:
                    ThermalConfig::SENSOR_HEIGHT_M = cmd.value;
                    FovCorrection::init(cmd.value);
                    ESP_LOGI(TAG, "Sensor height updated: %.2f m", cmd.value);
                    break;
                case ConfigCmdType::SET_PERSON_DIAMETER:
                    ThermalConfig::PERSON_DIAMETER_M = cmd.value;
                    ESP_LOGI(TAG, "Person diameter updated: %.2f m", cmd.value);
                    break;
                case ConfigCmdType::SET_VIEW_MODE:
                    ThermalConfig::VIEW_MODE = (int)cmd.value;
                    break;
                case ConfigCmdType::RESET_COUNTS:
                    count_in_ = 0;
                    count_out_ = 0;
                    ESP_LOGI(TAG, "Counters reset");
                    break;
                case ConfigCmdType::RETRY_SENSOR:
                    if (sensor_.init() == ESP_OK) {
                        sensor_initialized_ = true;
                        frame_accumulator_.reset();  // A1 fix: reset chess compositor
                        noise_filter_.reset();        // A1 fix: reset Kalman filter
                        bg_init_ = false;             // force background re-initialization
                        ESP_LOGI(TAG, "Sensor re-initialized -- accumulator and filter reset");
                    }
                    break;
                default: break;
            }
        }

        // --- Phase 0.2: Frame Acquisition ---
        bool sensor_ok = false;
        if (sensor_initialized_ && (sensor_.readFrame(current_frame_) == ESP_OK)) {
            sensor_ok = true;
        } else {
            sensor_initialized_ = false;
        }

        if (sensor_ok) {
            uint8_t currentSubPage = sensor_.getLastSubPageID();

            // --- A1 fix: Chess sub-frame accumulation ---
            // Fuse both sub-frames into composed_frame_ to eliminate the visual chess artifact.
            frame_accumulator_.integrate(current_frame_, currentSubPage, composed_frame_);

            if (!frame_accumulator_.isReady()) {
                // Wait for the second sub-page before running the pipeline
                goto dispatch;
            }

            // --- A1 fix: Kalman noise filter ---
            // Apply per-pixel 1D Kalman to the composed frame -> filtered_frame_.
            // Detection uses filtered_frame_; image dispatch uses composed_frame_ for fidelity.
            noise_filter_.apply(composed_frame_, filtered_frame_);

            // --- Step 1: Background modeling (filtered frame) ---
            if (!bg_init_) {
                BackgroundModel::initialize(filtered_frame_, background_map_, ThermalConfig::TOTAL_PIXELS);
                bg_init_ = true;
            } else {
                BackgroundModel::update(filtered_frame_, background_map_, blocking_mask_,
                                         ThermalConfig::TOTAL_PIXELS, ThermalConfig::EMA_ALPHA);
            }

            // --- Step 2: Peak Detection (uses filtered frame) ---
            num_peaks_ = 0;
            PeakDetector::detect(filtered_frame_, background_map_,
                                  peaks_, &num_peaks_,
                                  ThermalConfig::BIOLOGICAL_TEMP_MIN,
                                  ThermalConfig::BACKGROUND_DELTA_T,
                                  ThermalConfig::MAX_PEAKS);

            // --- Step 3: NMS ---
            // Calculate dynamic physical radius logic
            float radius_px = (ThermalConfig::PERSON_DIAMETER_M / 2.0f) * FovCorrection::getPixelsPerMeter();
            int radius_sq = (int)(radius_px * radius_px);
            if (radius_sq < 1) radius_sq = 1;

            NmsSuppressor::suppress(peaks_, num_peaks_, radius_sq);

            // --- Step 4: Tracking (A2: TrackletTracker) ---
            uint32_t ts = xTaskGetTickCount();
            tracker_.update(peaks_, num_peaks_, ts);

            // --- Step 4b: Counting FSM (A3: TrackletFSM) ---
            door_fsm_.update(tracker_, count_in_, count_out_);

            tracker_.fillTrackArray(track_array_, &num_confirmed_tracks_);

            // --- Step 5: Masking ---
            MaskGenerator::generate(track_array_, num_confirmed_tracks_,
                                     blocking_mask_, ThermalConfig::MASK_HALF_SIZE);
        }

        dispatch:

        // --- DISPATCH: Build IpcPacket ---
        static IpcPacket packet; // static: reduces stack usage ( ~1.6 KB )
        memset(&packet, 0, sizeof(IpcPacket)); // Safety: zero-init
        
        packet.sensor_ok              = sensor_ok;
        packet.telemetry.frame_id     = frame_id_;
        frame_id_ = (frame_id_ == UINT32_MAX) ? 1 : frame_id_ + 1;
        packet.telemetry.ambient_temp = sensor_.getAmbientTemp();
        packet.telemetry.count_in     = sat16(count_in_);
        packet.telemetry.count_out    = sat16(count_out_);

        // A2: use dense track_array_ (confirmed tracks only, all active = true)
        // P04/P10-fix: Si el sensor falló, no encolar datos de un frame congelado.
        // Forzar tracks = 0 para que la UI muestre estado de error limpio.
        const int tracks_to_send = sensor_ok ? num_confirmed_tracks_ : 0;
        int tidx = 0;
        for (int i = 0; i < tracks_to_send && tidx < ThermalConfig::MAX_TRACKS; i++) {
            if (track_array_[i].active) {
                packet.telemetry.tracks[tidx].id      = track_array_[i].id;
                packet.telemetry.tracks[tidx].x_100   = (int16_t)(track_array_[i].x   * 100.0f);
                packet.telemetry.tracks[tidx].y_100   = (int16_t)(track_array_[i].y   * 100.0f);
                packet.telemetry.tracks[tidx].v_x_100 = (int16_t)(track_array_[i].v_x * 100.0f);
                packet.telemetry.tracks[tidx].v_y_100 = (int16_t)(track_array_[i].v_y * 100.0f);
                tidx++;
            }
        }
        packet.telemetry.num_tracks = tidx;
        
        // Final image dispatch:
        // - Normal mode (VIEW_MODE=0): composed_frame_ (fused, faithful to sensor)
        // - Radar mode  (VIEW_MODE=1): (filtered_frame_ - background) for clean subtraction
        const float* display_src = (ThermalConfig::VIEW_MODE == 1) ? filtered_frame_ : composed_frame_;
        for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
            float val = (ThermalConfig::VIEW_MODE == 1)
                        ? (display_src[i] - background_map_[i])
                        : display_src[i];
            packet.image.pixels[i] = (int16_t)(val * 100.0f);
        }
        packet.image.frame_id = packet.telemetry.frame_id;

        xQueueSend(ipcQueue_, &packet, 0);

        // --- Self-Monitoring: Profile Stack High Water Mark (approx every 5s @ 16Hz) ---
        static uint32_t monitor_count = 0;
        if (++monitor_count >= 80) {
            monitor_count = 0;
            UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGI(TAG, "Stack High Water Mark: %u words (%u bytes) free", 
                     (unsigned int)hwm, (unsigned int)(hwm * sizeof(StackType_t)));
        }

        esp_task_wdt_reset();
        vTaskDelayUntil(&lastWakeTime, period);
    }
}
