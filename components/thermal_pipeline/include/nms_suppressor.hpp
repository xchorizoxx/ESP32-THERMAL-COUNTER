#pragma once
/**
 * @file nms_suppressor.hpp
 * @brief Step 3 — Adaptive Non-Maximum Suppression (NMS).
 *
 * Suppresses peaks near the hottest peak, using a variable radius
 * to compensate for lens distortion at the edges.
 */

#include "thermal_types.hpp"

class NmsSuppressor {
public:
    /**
     * @brief Applies adaptive NMS over the peaks array.
     * @param peaks       Peaks array [numPeaks], modified in-place
     * @param numPeaks    Number of peaks in the array
     * @param rCenterSq   Radius² for the lens center zone (e.g., 16 = radius 4)
     * @param rEdgeSq     Radius² for the lens edges (e.g., 4 = radius 2)
     * @param centerXMin  Minimum X column of the center zone (e.g., 8)
     * @param centerXMax  Maximum X column of the center zone (e.g., 23)
     *
     * Algorithm:
     * 1. Insertion sort by descending temperature (O(N²), optimal for N≤15)
     * 2. For each peak j not suppressed: suppress all k>j with D²≤R²
     */
    static void suppress(ThermalPeak* peaks, int numPeaks,
                         int rCenterSq, int rEdgeSq,
                         int centerXMin, int centerXMax);
};
