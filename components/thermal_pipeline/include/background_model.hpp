#pragma once
/**
 * @file background_model.hpp
 * @brief Paso 1 — Actualización de Fondo Dinámico (EMA Selectiva).
 *
 * Actualiza el mapa de fondo usando promedio móvil exponencial,
 * solo en las zonas no bloqueadas por la máscara de tracks activos.
 */

#include <cstdint>

class BackgroundModel {
public:
    /**
     * @brief Actualiza el mapa de fondo con EMA selectiva.
     * @param frame       Frame actual de temperaturas [TOTAL_PIXELS]
     * @param fondo       Mapa de fondo a actualizar in-place [TOTAL_PIXELS]
     * @param mascara     Máscara de bloqueo (0=libre, 1=ocupado) [TOTAL_PIXELS]
     * @param totalPixels Número total de píxeles (768)
     * @param alpha       Constante EMA (ej. 0.05)
     *
     * Fórmula: fondo[i] = alpha * frame[i] + (1 - alpha) * fondo[i]
     * Solo se aplica donde mascara[i] == 0.
     */
    static void update(const float* frame, float* fondo,
                       const uint8_t* mascara, int totalPixels, float alpha);

    /**
     * @brief Inicializa el mapa de fondo copiando el primer frame.
     * @param frame  Frame de temperaturas [TOTAL_PIXELS]
     * @param fondo  Mapa de fondo a inicializar [TOTAL_PIXELS]
     * @param totalPixels Número total de píxeles
     */
    static void initialize(const float* frame, float* fondo, int totalPixels);
};
