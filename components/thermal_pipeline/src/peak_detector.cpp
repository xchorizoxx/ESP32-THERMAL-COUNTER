#include "peak_detector.hpp"
#include "thermal_config.hpp"

void PeakDetector::detect(const float* frame, const float* fondo,
                          PicoTermico* outPicos, int* outNum,
                          float tempMin, float deltaT, int maxPicos)
{
    *outNum = 0;

    // Iterar excluyendo bordes: y ∈ [1, 22], x ∈ [1, 30]
    for (int y = 1; y <= 22; y++) {
        for (int x = 1; x <= 30; x++) {
            const int i = y * ThermalConfig::MLX_COLS + x;
            const float temp = frame[i];

            // Condición 1: Umbral biológico
            if (temp <= tempMin) continue;

            // Condición 2: Contraste vs fondo
            if ((temp - fondo[i]) <= deltaT) continue;

            // Condición 3: Máximo local estricto (> 8 vecinos)
            const int cols = ThermalConfig::MLX_COLS; // 32
            if (temp <= frame[i - cols - 1]) continue;  // Top-Left
            if (temp <= frame[i - cols])     continue;  // Top
            if (temp <= frame[i - cols + 1]) continue;  // Top-Right
            if (temp <= frame[i - 1])        continue;  // Left
            if (temp <= frame[i + 1])        continue;  // Right
            if (temp <= frame[i + cols - 1]) continue;  // Bottom-Left
            if (temp <= frame[i + cols])     continue;  // Bottom
            if (temp <= frame[i + cols + 1]) continue;  // Bottom-Right

            // Todas las condiciones cumplidas → es un pico
            if (*outNum >= maxPicos) return; // Capacidad alcanzada

            PicoTermico& pico = outPicos[*outNum];
            pico.x = (uint8_t)x;
            pico.y = (uint8_t)y;
            pico.temperatura = temp;
            pico.suprimido = false;
            (*outNum)++;
        }
    }
}
