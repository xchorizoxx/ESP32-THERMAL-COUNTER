#pragma once
/**
 * @file alpha_beta_tracker.hpp
 * @brief Paso 4 — Tracking con Filtro Alpha-Beta + Lógica de Conteo.
 *
 * Rastrea personas entre frames usando un filtro predictivo Alpha-Beta
 * y cuenta entradas/salidas con una máquina de estados de histéresis.
 */

#include "thermal_types.hpp"

class AlphaBetaTracker {
public:
    AlphaBetaTracker();

    /**
     * @brief Actualiza tracks con los picos del frame actual y cuenta cruces.
     * @param picos       Array de picos del frame (incluyendo suprimidos)
     * @param numPicos    Total de picos en el array
     * @param alpha       Ganancia de posición del filtro
     * @param beta        Ganancia de velocidad del filtro
     * @param maxDistSq   Distancia² máxima para emparejar pico con track
     * @param maxAge      Frames máximos sin actualización antes de eliminar track
     * @param lineEntryY  Línea virtual superior (entrada)
     * @param lineExitY   Línea virtual inferior (salida)
     * @param countIn     Contador de entradas (acumulativo, in-out)
     * @param countOut    Contador de salidas (acumulativo, in-out)
     */
    void update(const PicoTermico* picos, int numPicos,
                float alpha, float beta,
                int maxDistSq, int maxAge,
                int lineEntryY, int lineExitY,
                int& countIn, int& countOut);

    /**
     * @brief Acceso de lectura al array de tracks para la máscara.
     */
    const Track* getTracks() const { return tracks_; }

    /**
     * @brief Número máximo de tracks (coincide con MAX_TRACKS).
     */
    int getMaxTracks() const;

    /**
     * @brief Devuelve el número de tracks activos.
     */
    int getActiveCount() const;

private:
    Track   tracks_[15]; // MAX_TRACKS hardcoded para evitar dependencia circular
    uint8_t nextId_;

    /**
     * @brief Busca o crea un track libre para asignar un pico nuevo.
     * @return Puntero al track, o nullptr si no hay espacio.
     */
    Track* findFreeTrack();

    /**
     * @brief Evalúa la máquina de estados de histéresis para contar.
     * @param track      Track a evaluar
     * @param lineEntryY Línea virtual superior
     * @param lineExitY  Línea virtual inferior
     * @param countIn    Contador de entradas
     * @param countOut   Contador de salidas
     */
    void evaluateCountingLogic(Track& track,
                               int lineEntryY, int lineExitY,
                               int& countIn, int& countOut);
};
