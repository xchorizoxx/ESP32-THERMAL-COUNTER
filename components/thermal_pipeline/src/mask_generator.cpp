#include "mask_generator.hpp"
#include "thermal_config.hpp"
#include <cstring>

void MaskGenerator::generate(const Track* tracks, int maxTracks,
                              uint8_t* mask, int halfSize)
{
    // Step 1: Clear full mask
    memset(mask, 0, ThermalConfig::TOTAL_PIXELS);

    // Step 2: Draw square around each active track
    for (int i = 0; i < maxTracks; i++) {
        if (!tracks[i].activo) continue;

        const int cx = (int)tracks[i].x;
        const int cy = (int)tracks[i].y;

        for (int dy = -halfSize; dy <= halfSize; dy++) {
            for (int dx = -halfSize; dx <= halfSize; dx++) {
                const int nx = cx + dx;
                const int ny = cy + dy;

                // Check boundaries (do not exceed the 32x24 sensor pixels)
                if (nx >= 0 && nx < ThermalConfig::MLX_COLS &&
                    ny >= 0 && ny < ThermalConfig::MLX_ROWS) {
                    mask[ny * ThermalConfig::MLX_COLS + nx] = 1;
                }
            }
        }
    }
}
