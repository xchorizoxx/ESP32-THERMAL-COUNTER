#include "tft_renderer.hpp"
#include "esp_log.h"

static const char* TAG = "TFT_REND";

TftRenderer::TftRenderer() : m_initialized(false) {}

TftRenderer::~TftRenderer() {}

bool TftRenderer::init() {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
    return false;
}

void TftRenderer::renderHeatmap(const int16_t* srcPixels,
                                uint16_t* fb,
                                int srcW, int srcH,
                                int dstW, int dstH) {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
    (void)srcPixels;
    (void)fb;
    (void)srcW;
    (void)srcH;
    (void)dstW;
    (void)dstH;
}
