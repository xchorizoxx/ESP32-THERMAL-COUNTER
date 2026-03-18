#include "mask_generator.hpp"
#include "thermal_config.hpp"
#include <cstring>

void MaskGenerator::generate(const Track* tracks, int maxTracks,
                              uint8_t* mascara, int halfSize)
{
    // Paso 1: Limpiar máscara completa
    memset(mascara, 0, ThermalConfig::TOTAL_PIXELS);

    // Paso 2: Dibujar cuadrado alrededor de cada track activo
    for (int i = 0; i < maxTracks; i++) {
        if (!tracks[i].activo) continue;

        const int cx = (int)tracks[i].x;
        const int cy = (int)tracks[i].y;

        for (int dy = -halfSize; dy <= halfSize; dy++) {
            for (int dx = -halfSize; dx <= halfSize; dx++) {
                const int nx = cx + dx;
                const int ny = cy + dy;

                // Verificar límites (no exceder los 32×24 píxeles del sensor)
                if (nx >= 0 && nx < ThermalConfig::MLX_COLS &&
                    ny >= 0 && ny < ThermalConfig::MLX_ROWS) {
                    mascara[ny * ThermalConfig::MLX_COLS + nx] = 1;
                }
            }
        }
    }
}
