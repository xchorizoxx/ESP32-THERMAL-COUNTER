#include "peak_detector.hpp"
#include "thermal_config.hpp"

void PeakDetector::detect(const float* frame, const float* background,
                          PicoTermico* outPeaks, int* outNum,
                          float tempMin, float deltaT, int maxPeaks)
{
    *outNum = 0;

    // Iterate excluding edges: y ∈ [1, 22], x ∈ [1, 30]
    for (int y = 1; y <= 22; y++) {
        for (int x = 1; x <= 30; x++) {
            const int i = y * ThermalConfig::MLX_COLS + x;
            const float temp = frame[i];

            // Condition 1: Biological threshold
            if (temp <= tempMin) continue;

            // Condition 2: Contrast vs background
            if ((temp - background[i]) <= deltaT) continue;

            // Condition 3: Strict local maximum (> 8 neighbors)
            const int cols = ThermalConfig::MLX_COLS; // 32
            if (temp <= frame[i - cols - 1]) continue;  // Top-Left
            if (temp <= frame[i - cols])     continue;  // Top
            if (temp <= frame[i - cols + 1]) continue;  // Top-Right
            if (temp <= frame[i - 1])        continue;  // Left
            if (temp <= frame[i + 1])        continue;  // Right
            if (temp <= frame[i + cols - 1]) continue;  // Bottom-Left
            if (temp <= frame[i + cols])     continue;  // Bottom
            if (temp <= frame[i + cols + 1]) continue;  // Bottom-Right

            // All conditions met → it is a peak
            if (*outNum >= maxPeaks) return; // Capacity reached

            PicoTermico& peak = outPeaks[*outNum];
            peak.x = (uint8_t)x;
            peak.y = (uint8_t)y;
            peak.temperatura = temp;
            peak.suprimido = false;
            (*outNum)++;
        }
    }
}
