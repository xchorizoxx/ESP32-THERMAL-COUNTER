/**
 * @file tft_renderer.cpp
 * @brief Bilinear interpolation + ironbow palette LUT renderer.
 */

#include "tft_renderer.hpp"
#include "tft_config.hpp"
#include "esp_log.h"
#include <math.h>

const char* TftRenderer::TAG = "TFT_REND";

// ────────────────────────────────────────────────────────────
//  Ironbow Palette Anchor Points
// ────────────────────────────────────────────────────────────

struct PaletteAnchor {
    uint8_t index;
    uint8_t r, g, b;
};

static const PaletteAnchor anchors[] = {
    {  0,   0,   0,  64},   // Deep blue
    { 32,   0,   0, 192},   // Blue
    { 64,   0,   0, 255},   // Bright blue
    { 96,   0, 180, 180},   // Cyan
    {112,   0, 255,   0},   // Green
    {128, 255, 255,   0},   // Yellow
    {160, 255, 160,   0},   // Orange
    {192, 255,  64,   0},   // Red-orange
    {224, 255,   0,   0},   // Red
    {240, 255, 100, 100},   // Pink-white
    {255, 255, 255, 255},   // White
};

static const int NUM_ANCHORS = sizeof(anchors) / sizeof(anchors[0]);

// ────────────────────────────────────────────────────────────
//  RGB888 -> RGB565 (byte-swapped for ST7735S big-endian SPI)
// ────────────────────────────────────────────────────────────

static inline uint16_t rgb565_swap(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return (c >> 8) | (c << 8);  // Byte swap for big-endian SPI
}

// ────────────────────────────────────────────────────────────
//  Linear interpolation helper
// ────────────────────────────────────────────────────────────

static inline uint8_t lerp_u8(uint8_t a, uint8_t b, float t) {
    return (uint8_t)((float)a + (float)(b - a) * t);
}

// ────────────────────────────────────────────────────────────
//  Palette Generation
// ────────────────────────────────────────────────────────────

void TftRenderer::init() {
    for (int i = 0; i < 256; i++) {
        // Find surrounding anchors
        int seg = 0;
        for (int a = 0; a < NUM_ANCHORS - 1; a++) {
            if (i >= anchors[a].index && i <= anchors[a + 1].index) {
                seg = a;
                break;
            }
        }

        const PaletteAnchor& a0 = anchors[seg];
        const PaletteAnchor& a1 = anchors[seg + 1];

        float t = 0.0f;
        int span = a1.index - a0.index;
        if (span > 0) {
            t = (float)(i - a0.index) / (float)span;
        }

        uint8_t r = lerp_u8(a0.r, a1.r, t);
        uint8_t g = lerp_u8(a0.g, a1.g, t);
        uint8_t b = lerp_u8(a0.b, a1.b, t);

        palette_[i] = rgb565_swap(r, g, b);
    }

    ESP_LOGI(TAG, "Web palette LUT initialized (256 entries, RGB565)");
}

// ────────────────────────────────────────────────────────────
//  Temperature Range Setter
// ────────────────────────────────────────────────────────────

void TftRenderer::setTempRange(float temp_min_c, float temp_max_c) {
    temp_min_100_ = (int16_t)(temp_min_c * 100.0f);
    temp_max_100_ = (int16_t)(temp_max_c * 100.0f);
    ESP_LOGI(TAG, "Temperature range: %.1f - %.1f deg C", temp_min_c, temp_max_c);
}

// ────────────────────────────────────────────────────────────
//  Bilinear Interpolation + Palette Lookup
// ────────────────────────────────────────────────────────────

void TftRenderer::renderHeatmap(const int16_t* src, uint16_t* dst,
                                 int w, int h) {
    const int SRC_COLS = TftConfig::SRC_COLS; // 32
    const int SRC_ROWS = TftConfig::SRC_ROWS; // 24

    const float x_scale = (float)(SRC_COLS - 1) / (float)(w - 1);
    const float y_scale = (float)(SRC_ROWS - 1) / (float)(h - 1);
    const float range_inv = 255.0f / (float)(temp_max_100_ - temp_min_100_);

    for (int dy = 0; dy < h; dy++) {
        float src_y = (float)dy * y_scale;
        int y0 = (int)src_y;
        int y1 = (y0 < SRC_ROWS - 1) ? y0 + 1 : y0;
        float fy = src_y - (float)y0;
        float fy_inv = 1.0f - fy;

        for (int dx = 0; dx < w; dx++) {
            float src_x = (float)dx * x_scale;
            int x0 = (int)src_x;
            int x1 = (x0 < SRC_COLS - 1) ? x0 + 1 : x0;
            float fx = src_x - (float)x0;

            // Bilinear interpolation in int16 temperature domain -> float
            float val =
                fy_inv * ((1.0f - fx) * (float)src[y0 * SRC_COLS + x0] +
                                   fx  * (float)src[y0 * SRC_COLS + x1]) +
                fy     * ((1.0f - fx) * (float)src[y1 * SRC_COLS + x0] +
                                   fx  * (float)src[y1 * SRC_COLS + x1]);

            // Map to palette index
            int idx = (int)((val - (float)temp_min_100_) * range_inv);
            if (idx < 0) idx = 0;
            if (idx > 255) idx = 255;

            dst[dy * w + dx] = palette_[idx];
        }
    }
}
