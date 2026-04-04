#include "peak_detector.hpp"
#include "thermal_config.hpp"

void PeakDetector::detect(const float* currentFrame, const float* backgroundMap,
                          ThermalPeak* peaks, int* numPeaks,
                          float tempMin, float deltaT, int maxPeaks)
{
    *numPeaks = 0;
    const int cols = ThermalConfig::MLX_COLS;
    const int rows = ThermalConfig::MLX_ROWS;

    for (int r = 1; r < rows - 1; r++) {
        for (int c = 1; c < cols - 1; c++) {
            int i = r * cols + c;
            float val = currentFrame[i];

            // Condition 1: Exceeds minimum biological temperature
            if (val < tempMin) continue;

            // Condition 2: High contrast relative to background
            if (val - backgroundMap[i] < deltaT) continue;

            // Condition 3: Is it a local maximum in its 3x3 neighborhood?
            bool isMax = true;
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    if (val <= currentFrame[(r + dr) * cols + (c + dc)]) {
                        isMax = false;
                        break;
                    }
                }
                if (!isMax) break;
            }

            if (!isMax) continue;

            // All conditions met → it is a peak
            if (*numPeaks < maxPeaks) {
                peaks[*numPeaks].x = (uint8_t)c;
                peaks[*numPeaks].y = (uint8_t)r;
                peaks[*numPeaks].temperature = val;
                peaks[*numPeaks].suppressed  = false;
                (*numPeaks)++;
            } else {
                return; // Capacity reached
            }
        }
    }
}
