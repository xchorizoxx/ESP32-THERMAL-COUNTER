/**
 * @file tft_heatmap_view.cpp
 * @brief Heatmap view implementation: delegates to TftRenderer.
 */

#include "tft_heatmap_view.hpp"
#include "esp_log.h"

const char* TftHeatmapView::TAG = "TFT_HEATMAP";

void TftHeatmapView::onEnter() {
    if (!initialized_) {
        renderer_.init();
        initialized_ = true;
    }
    ESP_LOGI(TAG, "Heatmap view active");
}

void TftHeatmapView::onExit() {
    // Nothing to clean up
}

void TftHeatmapView::render(const TftSnapshot& snap,
                             uint16_t* fb, int width, int height) {
    if (!snap.sensor_ok) {
        // Render with available data (may be stale)
    }
    renderer_.renderHeatmap(snap.pixels, fb, width, height);
}
