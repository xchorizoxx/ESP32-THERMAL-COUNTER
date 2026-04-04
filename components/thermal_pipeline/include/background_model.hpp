#pragma once
/**
 * @file background_model.hpp
 * @brief Step 1 — Dynamic Background Update (Selective EMA).
 *
 * Updates the background map using exponential moving average,
 * only in zones not blocked by the active tracks mask.
 */

#include <cstdint>

class BackgroundModel {
public:
    /**
     * @brief Updates the background map with selective EMA.
     * @param frame       Current temperature frame [TOTAL_PIXELS]
     * @param background  Background map to update in-place [TOTAL_PIXELS]
     * @param mask        Block mask (0=free, 1=occupied) [TOTAL_PIXELS]
     * @param totalPixels Total number of pixels (768)
     * @param alpha       EMA constant (e.g., 0.05)
     *
     * Formula: background[i] = alpha * frame[i] + (1 - alpha) * background[i]
     * Only applied where mask[i] == 0.
     */
    static void update(const float* frame, float* background,
                       const uint8_t* mask, int totalPixels, float alpha);

    /**
     * @brief Initializes the background map by copying the first frame.
     * @param frame       Temperature frame [TOTAL_PIXELS]
     * @param background  Background map to initialize [TOTAL_PIXELS]
     * @param totalPixels Total number of pixels
     */
    static void initialize(const float* frame, float* background, int totalPixels);
};
