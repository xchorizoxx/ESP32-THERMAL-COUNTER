#pragma once
/**
 * @file peak_detector.hpp
 * @brief Step 2 — Thermal Peak Detection (Topology).
 *
 * Detects local maxima that exceed biological and contrast thresholds.
 */

#include "thermal_types.hpp"

class PeakDetector {
public:
    /**
     * @brief Detects thermal peaks in the current frame.
     * @param frame     Temperature frame [TOTAL_PIXELS]
     * @param background Background map [TOTAL_PIXELS]
     * @param outPeaks  Output array [MAX_TRACKS]
     * @param outNum    Number of detected peaks (output)
     * @param tempMin   Minimum biological threshold (°C)
     * @param deltaT    Minimum contrast vs background (°C)
     * @param maxPeaks  Maximum capacity of the outPeaks array
     *
     * Conditions for a valid peak:
     *  1. frame[i] > tempMin
     *  2. (frame[i] - background[i]) > deltaT
     *  3. frame[i] > its 8 neighbors (strict local maximum)
     *
     * Edges (row 0, row 23, col 0, col 31) are excluded to avoid OOB.
     */
    static void detect(const float* currentFrame, const float* backgroundMap,
                       ThermalPeak* peaks, int* numPeaks,
                       float tempMin, float deltaT, int maxPeaks);
};
