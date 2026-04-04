#include "fov_correction.hpp"
#include "thermal_config.hpp"
#include <cmath>
#include "esp_log.h"

static const char* TAG = "FOV_CORRECT";

namespace FovCorrection {

    static float cx_table[ThermalConfig::MLX_COLS][ThermalConfig::MLX_ROWS];
    static float cy_table[ThermalConfig::MLX_COLS][ThermalConfig::MLX_ROWS];
    static float pixels_per_meter_val = 1.0f;
    static bool  ready = false;

    void init(float height_m) {
        // FOV: 110° H x 70° V (User specified 70 for V, usually 75 for MLX90640 but let's use 70 as requested)
        constexpr float H_FOV_HALF_RAD = 55.0f * (float)M_PI / 180.0f;
        constexpr float V_FOV_HALF_RAD = 35.0f * (float)M_PI / 180.0f; // 70 / 2 = 35
        constexpr int   COLS = ThermalConfig::MLX_COLS;
        constexpr int   ROWS = ThermalConfig::MLX_ROWS;

        // Ground mapped dimensions
        const float ground_w = 2.0f * height_m * tanf(H_FOV_HALF_RAD);
        const float ground_h = 2.0f * height_m * tanf(V_FOV_HALF_RAD);

        for (int c = 0; c < COLS; c++) {
            for (int r = 0; r < ROWS; r++) {
                float ax = ((float)c - (COLS - 1) / 2.0f) / ((COLS - 1) / 2.0f) * H_FOV_HALF_RAD;
                float ay = ((float)r - (ROWS - 1) / 2.0f) / ((ROWS - 1) / 2.0f) * V_FOV_HALF_RAD;

                float x_m = height_m * tanf(ax);
                float y_m = height_m * tanf(ay) / cosf(ax); // Double axis correction

                cx_table[c][r] = ((x_m + ground_w / 2.0f) / ground_w) * (COLS - 1);
                cy_table[c][r] = ((y_m + ground_h / 2.0f) / ground_h) * (ROWS - 1);

                if (cx_table[c][r] < 0.0f) cx_table[c][r] = 0.0f;
                if (cx_table[c][r] > COLS - 1) cx_table[c][r] = COLS - 1;
                if (cy_table[c][r] < 0.0f) cy_table[c][r] = 0.0f;
                if (cy_table[c][r] > ROWS - 1) cy_table[c][r] = ROWS - 1;
            }
        }
        pixels_per_meter_val = (COLS - 1) / ground_w;
        ready = true;
        ESP_LOGI(TAG, "FOV table recalculated for H=%.2fm (ground %.2fm x %.2fm, px/m=%.2f)",
                 height_m, ground_w, ground_h, pixels_per_meter_val);
    }

    void correct(float& cx_raw, float& cy_raw) {
        if (!ready) return;
        
        int ci = (int)cx_raw;
        int ri = (int)cy_raw;
        float fx = cx_raw - (float)ci;
        float fy = cy_raw - (float)ri;
        
        constexpr int COLS = ThermalConfig::MLX_COLS;
        constexpr int ROWS = ThermalConfig::MLX_ROWS;
        
        if (ci < 0) ci = 0;
        if (ci >= COLS - 1) ci = COLS - 2;
        if (ri < 0) ri = 0;
        if (ri >= ROWS - 1) ri = ROWS - 2;

        float x00 = cx_table[ci][ri];
        float x10 = cx_table[ci+1][ri];
        float x01 = cx_table[ci][ri+1];
        float x11 = cx_table[ci+1][ri+1];
        
        float y00 = cy_table[ci][ri];
        float y10 = cy_table[ci+1][ri];
        float y01 = cy_table[ci][ri+1];
        float y11 = cy_table[ci+1][ri+1];

        cx_raw = x00*(1-fx)*(1-fy) + x10*fx*(1-fy) + x01*(1-fx)*fy + x11*fx*fy;
        cy_raw = y00*(1-fx)*(1-fy) + y10*fx*(1-fy) + y01*(1-fx)*fy + y11*fx*fy;
    }

    float getPixelsPerMeter() {
        return pixels_per_meter_val;
    }

} // namespace FovCorrection
