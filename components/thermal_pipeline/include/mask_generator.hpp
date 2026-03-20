#pragma once
/**
 * @file mask_generator.hpp
 * @brief Step 5 — Feedback Mask Generation.
 *
 * Generates the blocking mask that protects zones with active persons
 * in the next pipeline cycle (feedback loop).
 */

#include "thermal_types.hpp"

class MaskGenerator {
public:
    /**
     * @brief Regenerates the blocking mask from active tracks.
     * @param tracks      Tracks array [maxTracks]
     * @param maxTracks   Total number of slots in the array
     * @param mask        Output mask [TOTAL_PIXELS], resetted and rewritten
     * @param halfSize    Radius of the drawn square (1 = 3x3 square)
     *
     * Flow:
     * 1. memset(mask, 0, TOTAL_PIXELS)
     * 2. For each active track: draw a (2*halfSize+1)² px square
     *    with boundary checks.
     */
    static void generate(const Track* tracks, int maxTracks,
                         uint8_t* mask, int halfSize);
};
