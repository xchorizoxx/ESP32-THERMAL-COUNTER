#pragma once
/**
 * @file peak_detector.hpp
 * @brief Paso 2 — Detección de Picos Térmicos (Topología).
 *
 * Detecta máximos locales que superan umbrales biológicos y de contraste.
 */

#include "thermal_types.hpp"

class PeakDetector {
public:
    /**
     * @brief Detecta picos térmicos en el frame actual.
     * @param frame     Frame de temperaturas [TOTAL_PIXELS]
     * @param fondo     Mapa de fondo [TOTAL_PIXELS]
     * @param outPicos  Array de salida [MAX_OBJETIVOS]
     * @param outNum    Número de picos detectados (salida)
     * @param tempMin   Umbral biológico mínimo (°C)
     * @param deltaT    Contraste mínimo vs fondo (°C)
     * @param maxPicos  Capacidad máxima del array outPicos
     *
     * Condiciones para un pico válido:
     *  1. frame[i] > tempMin
     *  2. (frame[i] - fondo[i]) > deltaT
     *  3. frame[i] > sus 8 vecinos (máximo local estricto)
     *
     * Se excluyen bordes (fila 0, fila 23, col 0, col 31) para evitar OOB.
     */
    static void detect(const float* frame, const float* fondo,
                       PicoTermico* outPicos, int* outNum,
                       float tempMin, float deltaT, int maxPicos);
};
