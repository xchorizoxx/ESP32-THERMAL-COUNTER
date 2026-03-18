#pragma once
/**
 * @file mask_generator.hpp
 * @brief Paso 5 — Generación de Máscara de Retroalimentación.
 *
 * Genera la máscara de bloqueo que protege las zonas con personas
 * activas en el siguiente ciclo del pipeline (feedback loop).
 */

#include "thermal_types.hpp"

class MaskGenerator {
public:
    /**
     * @brief Regenera la máscara de bloqueo desde los tracks activos.
     * @param tracks      Array de tracks [maxTracks]
     * @param maxTracks   Número total de slots en el array
     * @param mascara     Máscara de salida [TOTAL_PIXELS], reseteada y escrita
     * @param halfSize    Radio del cuadrado dibujado (1 = cuadrado 3×3)
     *
     * Flujo:
     * 1. memset(mascara, 0, TOTAL_PIXELS)
     * 2. Para cada track activo: dibujar un cuadrado de (2*halfSize+1)² px
     *    con comprobación de límites.
     */
    static void generate(const Track* tracks, int maxTracks,
                         uint8_t* mascara, int halfSize);
};
