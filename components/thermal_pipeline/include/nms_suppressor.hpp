#pragma once
/**
 * @file nms_suppressor.hpp
 * @brief Paso 3 — Supresión de No-Máximos (NMS) Adaptativa.
 *
 * Suprime picos cercanos al pico más caliente, usando un radio
 * variable para compensar la distorsión de lente en los bordes.
 */

#include "thermal_types.hpp"

class NmsSuppressor {
public:
    /**
     * @brief Aplica NMS adaptativa sobre el array de picos.
     * @param picos       Array de picos [numPicos], modificado in-place
     * @param numPicos    Número de picos en el array
     * @param rCenterSq   Radio² para zona central del lente (ej. 16 = radio 4)
     * @param rEdgeSq     Radio² para bordes del lente (ej. 4 = radio 2)
     * @param centerXMin  Columna X mínima de la zona central (ej. 8)
     * @param centerXMax  Columna X máxima de la zona central (ej. 23)
     *
     * Algoritmo:
     * 1. Insertion sort por temperatura descendente (O(N²), óptimo para N≤15)
     * 2. Para cada pico j no suprimido: suprimir todos los k>j con D²≤R²
     */
    static void suppress(PicoTermico* picos, int numPicos,
                         int rCenterSq, int rEdgeSq,
                         int centerXMin, int centerXMax);
};
