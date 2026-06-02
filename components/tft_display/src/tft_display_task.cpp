/**
 * @file tft_display_task.cpp
 * @brief TFT display task: button polling -> snapshot read -> render -> SPI push @ 15 FPS.
 */

#include "tft_display_task.hpp"
#include "tft_snapshot.hpp"
#include "tft_config.hpp"
#include "esp_log.h"
#include "esp_attr.h"
#include <cstring>

const char* TftDisplayTask::TAG = "TFT_TASK";

esp_err_t TftDisplayTask::init(TftDriver* driver) {
    if (!driver || !driver->isInitialized()) {
        ESP_LOGW(TAG, "TFT driver not available — task will not render");
        return ESP_ERR_INVALID_STATE;
    }
    driver_ = driver;

    // Initialize button
    button_.init();

    // Initialize view manager and register views
    view_manager_.registerView(&heatmap_view_);   // View 0: thermal camera
    view_manager_.registerView(&tracking_view_);  // View 1: tracking overlay
    view_manager_.registerView(&stats_view_);     // View 2: stats
    view_manager_.registerView(&info_view_);      // View 3: info

    ESP_LOGI(TAG, "TftDisplayTask initialized (target %d FPS, %d views)",
             TftConfig::TARGET_FPS, view_manager_.viewCount());
    return ESP_OK;
}

void TftDisplayTask::TaskWrapper(void* pvParameters) {
    auto* self = static_cast<TftDisplayTask*>(pvParameters);
    self->run();
    vTaskDelete(NULL);
}

void TftDisplayTask::run() {
    ESP_LOGI(TAG, "Display task started on Core %d", xPortGetCoreID());

    // Static framebuffer in DMA-capable SRAM (40 KB)
    static DMA_ATTR uint16_t s_framebuffer[TftConfig::PIXEL_COUNT];

    const TickType_t period = pdMS_TO_TICKS(TftConfig::FRAME_PERIOD_MS);
    TickType_t lastWake = xTaskGetTickCount();

    uint32_t last_frame_id = 0;
    uint32_t frame_count = 0;

    while (true) {
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        // Poll button (no ISR — polling at frame rate)
        BootButton::Event btn = button_.poll(now_ms);
        if (btn == BootButton::Event::SHORT_PRESS) {
            view_manager_.nextView();
        } else if (btn == BootButton::Event::LONG_PRESS) {
            bool sleeping = view_manager_.toggleSleep();
            if (sleeping) {
                driver_->sleep();
            } else {
                driver_->wakeup();
            }
        }

        // Skip rendering if sleeping
        if (view_manager_.isSleeping()) {
            vTaskDelayUntil(&lastWake, period);
            continue;
        }

        // Read snapshot (spinlock copy, ~2 us)
        static TftSnapshot s_snap;
        s_snap = TftBridge::readSnapshot();

        static bool s_has_valid_frame = false;
        if (s_snap.sensor_ok) {
            s_has_valid_frame = true;
        }

        if (!s_snap.sensor_ok || !s_has_valid_frame) {
            vTaskDelayUntil(&lastWake, period);
            continue;
        }

        // Render current view (skip if frame unchanged)
        if (s_snap.frame_id != last_frame_id && driver_) {
            last_frame_id = s_snap.frame_id;

            ITftView* view = view_manager_.currentView();
            if (view) {
                view->render(s_snap, s_framebuffer,
                            TftConfig::WIDTH, TftConfig::HEIGHT);
                driver_->pushPixels(s_framebuffer);
                frame_count++;
            }
        }

        // Diagnostic every ~10 seconds (150 frames at 15 FPS)
        if (frame_count > 0 && frame_count % 150 == 0) {
            UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGI(TAG, "Health: %lu frames rendered, stack HWM=%u bytes",
                     (unsigned long)frame_count,
                     (unsigned int)(hwm * sizeof(StackType_t)));
        }

        vTaskDelayUntil(&lastWake, period);
    }
}
