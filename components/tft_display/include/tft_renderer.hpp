#pragma once
/**
 * @file tft_renderer.hpp
 * @brief Thermal heatmap renderer: bilinear interpolation + ironbow palette.
 *
 * Upscales a 32x24 thermal sensor image to a 160x128 RGB565 framebuffer.
 * Uses float arithmetic (ESP32-S3 hardware FPU) for speed.
 */

#include <stdint.h>

class TftRenderer {
public:
    /**
     * @brief Initialize the ironbow palette LUT. Call once at startup.
     */
    void init();

    /**
     * @brief Render a thermal heatmap into an RGB565 framebuffer.
     *
     * Steps:
     *   1. Bilinear interpolation: 32x24 -> 160x128 (int16 temperature domain)
     *   2. Palette lookup: temperature -> RGB565 via pre-computed LUT
     *
     * @param src_pixels  Source thermal image (32x24, int16_t, temp x 100)
     * @param dst_fb      Destination framebuffer (160x128, uint16_t, RGB565)
     *   Pixels are pre-swapped for ST7735S big-endian SPI protocol.
     * @param dst_width   Destination width (must be 160)
     * @param dst_height  Destination height (must be 128)
     */
    void renderHeatmap(const int16_t* src_pixels, uint16_t* dst_fb,
                       int dst_width, int dst_height);

    /**
     * @brief Update the temperature range for palette mapping.
     * @param temp_min_c  Minimum temperature [deg C] (maps to palette index 0)
     * @param temp_max_c  Maximum temperature [deg C] (maps to palette index 255)
     */
    void setTempRange(float temp_min_c, float temp_max_c);

private:
    /// Pre-computed ironbow palette in RGB565 (byte-swapped for SPI)
    uint16_t palette_[256];

    /// Temperature range in int16 (x100) for fast mapping
    int16_t temp_min_100_ = 1500;  // 15.0 deg C default
    int16_t temp_max_100_ = 4500;  // 45.0 deg C default

    static const char* TAG;
};
