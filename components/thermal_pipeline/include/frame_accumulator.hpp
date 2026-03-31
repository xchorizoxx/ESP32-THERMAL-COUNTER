#pragma once
/**
 * @file frame_accumulator.hpp
 * @brief Merges chess sub-frames to produce artifact-free visualization.
 *
 * The MLX90640 in chess mode updates alternating pixels on each sub-frame:
 *   - Sub-page 0: updates pixels where (row + col) % 2 == 0
 *   - Sub-page 1: updates pixels where (row + col) % 2 == 1
 *
 * This accumulator keeps the most recent valid value for each pixel,
 * regardless of which sub-frame it was captured in.
 *
 * NOTE: The Melexis library (MLX90640_CalculateTo) already applies the
 * mathematical chess correction. This component only resolves the VISUAL
 * checkerboard artifact caused by displaying a single sub-frame before
 * both halves have been received.
 *
 * Memory: 768 x 4 bytes = 3 KB (class member, no PSRAM required).
 */

#include <cstdint>
#include <cstring>
#include "thermal_config.hpp"

class FrameAccumulator {
public:
    FrameAccumulator() : initialized_(false)
    {
        memset(composed_, 0, sizeof(composed_));
    }

    /**
     * @brief Integrates a new sub-frame into the composed buffer.
     *
     * Only pixels belonging to the current sub-frame are updated.
     * Pixels from the opposite sub-frame retain their previous value.
     *
     * @param frame    Full output of MLX90640_CalculateTo() [TOTAL_PIXELS floats]
     * @param subpage  Received sub-frame index (0 or 1)
     * @param composed Output buffer with fused pixel values [TOTAL_PIXELS floats]
     */
    void integrate(const float* frame, uint8_t subpage, float* composed)
    {
        const int COLS = ThermalConfig::MLX_COLS;  // 32
        const int ROWS = ThermalConfig::MLX_ROWS;  // 24

        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                int idx = row * COLS + col;
                // Determine which sub-page owns this pixel in chess mode
                int pixel_subpage = (row + col) % 2;

                if (pixel_subpage == (int)subpage) {
                    // This pixel was updated in the current sub-frame
                    composed_[idx] = frame[idx];
                }
                // Other pixels automatically retain their previous value
            }
        }

        // Mark as ready after sub-page 1 is received for the first time,
        // ensuring both halves of the chessboard contain valid data.
        if (!initialized_ && subpage == 1) {
            initialized_ = true;
        }

        memcpy(composed, composed_, ThermalConfig::TOTAL_PIXELS * sizeof(float));
    }

    /**
     * @brief Returns true once a full chess cycle (both sub-frames) has been received.
     *
     * The pipeline should skip processing until this returns true.
     */
    bool isReady() const { return initialized_; }

    /**
     * @brief Resets the accumulator to the uninitialized state.
     *
     * Call after sensor re-initialization (ConfigCmdType::RETRY_SENSOR).
     */
    void reset()
    {
        initialized_ = false;
        memset(composed_, 0, sizeof(composed_));
    }

private:
    float composed_[ThermalConfig::TOTAL_PIXELS];  // Fused pixel buffer
    bool  initialized_;                             // True after first complete chess cycle
};
