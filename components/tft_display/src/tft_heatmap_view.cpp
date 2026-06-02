#include "tft_heatmap_view.hpp"
#include "esp_log.h"

static const char* TAG = "TFT_HEAT";

TftHeatmapView::TftHeatmapView() : m_initialized(false) {}

TftHeatmapView::~TftHeatmapView() {}

void TftHeatmapView::onEnter() {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
}

void TftHeatmapView::onExit() {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
}

void TftHeatmapView::render(const TftSnapshot& snap,
                            uint16_t* fb, int width, int height) {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
    (void)snap;
    (void)fb;
    (void)width;
    (void)height;
}

const char* TftHeatmapView::name() const {
    return "heatmap";
}
