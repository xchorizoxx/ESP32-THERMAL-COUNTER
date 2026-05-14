#include "peak_detector.hpp"
#include "thermal_config.hpp"
#include "fov_correction.hpp"

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
                // Centroide sub-píxel ponderado por temperatura en el vecindario 3×3.
                // Reduce saltos discretos cuando la persona se mueve entre celdas.
                float sum_w = 0.0f, sum_wx = 0.0f, sum_wy = 0.0f;
                for (int dr2 = -1; dr2 <= 1; dr2++) {
                    for (int dc2 = -1; dc2 <= 1; dc2++) {
                        float w = currentFrame[(r + dr2) * cols + (c + dc2)];
                        if (w < tempMin) w = 0.0f; // no pesar píxeles fríos
                        sum_w  += w;
                        sum_wx += w * (float)(c + dc2);
                        sum_wy += w * (float)(r + dr2);
                    }
                }
                float cx_raw = (sum_w > 0.0f) ? (sum_wx / sum_w) : (float)c;
                float cy_raw = (sum_w > 0.0f) ? (sum_wy / sum_w) : (float)r;

                // Corrección FOV: Transforma las coordenadas
                FovCorrection::correct(cx_raw, cy_raw);

                peaks[*numPeaks].x = cx_raw;
                peaks[*numPeaks].y = cy_raw;

                // Interpolar temperatura bilineal en la posición corregida por FOV
                // (la temperatura del pico raw puede diferir hasta ~0.5°C de la corregida)
                float temp_interp = val;
                int ix = (int)cx_raw;
                int iy = (int)cy_raw;
                if (ix >= 0 && ix < cols - 1 && iy >= 0 && iy < rows - 1) {
                    float fx = cx_raw - (float)ix;
                    float fy = cy_raw - (float)iy;
                    float p00 = currentFrame[iy * cols + ix];
                    float p10 = currentFrame[iy * cols + ix + 1];
                    float p01 = currentFrame[(iy + 1) * cols + ix];
                    float p11 = currentFrame[(iy + 1) * cols + ix + 1];
                    float top = p00 + fx * (p10 - p00);
                    float bot = p01 + fx * (p11 - p01);
                    temp_interp = top + fy * (bot - top);
                }
                peaks[*numPeaks].temperature = temp_interp;
                peaks[*numPeaks].suppressed  = false;
                (*numPeaks)++;
            } else {
                return; // Capacity reached
            }
        }
    }
}
