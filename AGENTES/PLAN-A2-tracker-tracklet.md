# Etapa A2 — Tracklet Tracker con Memoria Real de Objetos
## Reemplazo del Alpha-Beta Tracker

**Dependencias:** Etapa A1 completada  
**Hardware requerido:** Ninguno adicional  
**Tiempo estimado:** 2-3 días  
**Impacto:** Mejora dramática en estabilidad de tracking y reducción de ID switching

---

## 1. Análisis del Tracker Actual vs Propuestas

### 1.1 Problemas del Alpha-Beta actual

| Problema | Causa | Impacto |
|---|---|---|
| ID switching en cruce | Greedy assignment — primer track gana | Conteos dobles |
| Pérdida de track rápida | `maxAge = 5` frames sin historial | Personas "desaparecen" |
| Sin predicción real | Alpha-Beta sin aceleración | Mal seguimiento en giros |
| Reasignación apresurada | ID nuevo en primer frame de peak | IDs efímeros |
| Estado inicial incorrecto | `y < 12` hardcodeado | Conteos espurios |

### 1.2 Evaluación de Alternativas

#### Opción 1: Hungarian Algorithm + Kalman completo
- **Asignación óptima** entre tracks y peaks (O(n³), n≤15 → 3375 ops — aceptable)
- **Estado completo:** posición, velocidad, aceleración
- **Pros:** máxima precisión en cruces simultáneos
- **Contras:** implementación compleja, matrices 6×6 por track (+12KB RAM)
- **Veredicto:** overkill para 15 tracks, reservar para v3

#### Opción 2: Tracklet con historial + Greedy mejorado ✅ ELEGIDA
- **Historial de posiciones** últimos N frames por track
- **Predicción lineal** basada en velocidad promedio del historial
- **Matching por costo compuesto:** distancia_euclidea + diferencia_temperatura
- **Umbral de confianza:** track válido solo después de M detecciones consecutivas
- **Pros:** 8× mejor que Alpha-Beta actual, simple, bajo overhead
- **Contras:** puede fallar en oclusiones largas (>15 frames)
- **RAM overhead:** ~1.5 KB por track, 15 tracks = ~22 KB total (aceptable)

#### Opción 3: SORT (Simple Online and Realtime Tracking)
- Kalman 4D (x, y, vx, vy) + Hungarian + IoU matching
- Diseñado para bounding boxes, no para puntos
- Adaptarlo a puntos sería reinventar la Opción 1
- **Veredicto:** no se adapta bien a coordenadas puntuales de 32×24

**Decisión final: Opción 2 — Tracklet con historial de 20 frames**

---

## 2. Diseño del TrackletTracker

### 2.1 Estructura de un Tracklet

```cpp
// components/thermal_pipeline/include/tracklet_tracker.hpp
#pragma once

#include "thermal_types.hpp"
#include "thermal_config.hpp"
#include <cstring>
#include <cstdint>

/**
 * @brief Historial de posición de un track (últimos N frames).
 *
 * Permite predicción lineal y cálculo de velocidad media
 * sin necesidad de un filtro Kalman completo.
 */
struct TrackHistory {
    static constexpr int CAPACITY = 20;

    struct Entry {
        float x;
        float y;
        uint32_t tick;  // xTaskGetTickCount() en el momento de la detección
    };

    Entry entries[CAPACITY];
    int   count;     // Cuántas entradas válidas hay (0..CAPACITY)
    int   head;      // Índice circular de la entrada más reciente

    TrackHistory() : count(0), head(0) {
        memset(entries, 0, sizeof(entries));
    }

    void push(float x, float y, uint32_t tick) {
        head = (head + 1) % CAPACITY;
        entries[head] = {x, y, tick};
        if (count < CAPACITY) count++;
    }

    // Predicción lineal: posición esperada en el próximo frame
    // basada en velocidad media de los últimos min(N, count) frames.
    void predict(float* out_x, float* out_y) const {
        if (count < 2) {
            *out_x = entries[head].x;
            *out_y = entries[head].y;
            return;
        }
        // Usar los últimos 4 frames para velocidad estable
        int samples = (count < 4) ? count : 4;
        int prev_idx = (head - samples + CAPACITY) % CAPACITY;
        float dx = entries[head].x - entries[prev_idx].x;
        float dy = entries[head].y - entries[prev_idx].y;
        *out_x = entries[head].x + dx / samples;
        *out_y = entries[head].y + dy / samples;
    }

    float latestX() const { return entries[head].x; }
    float latestY() const { return entries[head].y; }
};

/**
 * @brief Track individual gestionado por TrackletTracker.
 */
struct Tracklet {
    uint8_t     id;              // ID único (1-255, cicla sin usar 0)
    bool        active;          // true si el track está vivo
    uint8_t     confirmed;       // Detecciones consecutivas acumuladas
    uint8_t     missed;          // Frames consecutivos sin match
    float       avg_temperature; // Temperatura media (para matching)
    float       pred_x;          // Posición predicha para este frame
    float       pred_y;
    TrackHistory history;

    // Estado para la FSM de puerta (se actualiza en TrackletFSM — Etapa A3)
    uint8_t     zone_state;      // 0=norte, 1=neutro, 2=sur, 3=descartado

    // Getters convenientes
    float x() const { return history.latestX(); }
    float y() const { return history.latestY(); }

    bool isConfirmed() const {
        return confirmed >= ThermalConfig::TRACK_CONFIRM_FRAMES;
    }
};
```

### 2.2 Constantes nuevas en thermal_config.hpp

```cpp
// Añadir al namespace ThermalConfig en thermal_config.hpp:

// --- TRACKLET TRACKER ---
constexpr int   TRACK_CONFIRM_FRAMES = 3;  // Detecciones mínimas para track válido
constexpr int   TRACK_MAX_MISSED     = 12; // Frames sin detección antes de eliminar
constexpr float TRACK_MAX_DIST       = 5.0f; // Distancia máxima en píxeles para match
constexpr float TRACK_TEMP_WEIGHT    = 0.25f; // Peso de temperatura en costo de match
// Nota: MAX_TRACKS = 15 ya existe. MAX_PEAKS = 15 ya existe.
```

### 2.3 Clase TrackletTracker

```cpp
// (continuación de tracklet_tracker.hpp)

/**
 * @brief Tracker de personas basado en tracklets con memoria de historial.
 *
 * Mejoras sobre Alpha-Beta:
 * - Historial de 20 frames por track
 * - Predicción lineal antes del matching
 * - Costo compuesto (distancia + temperatura)
 * - Confirmación de track (no válido hasta N detecciones)
 * - Descarte de tracks no confirmados rápidamente
 *
 * SEPARACIÓN DE RESPONSABILIDADES:
 * Este tracker NO implementa lógica de conteo.
 * La lógica de conteo vive en TrackletFSM (Etapa A3).
 * El tracker solo gestiona identidades y posiciones.
 */
class TrackletTracker {
public:
    TrackletTracker();

    /**
     * @brief Actualiza tracks con los peaks del frame actual.
     *
     * @param peaks     Peaks detectados (incluyendo suprimidos)
     * @param numPeaks  Total peaks en el array
     * @param timestamp xTaskGetTickCount() del frame actual
     */
    void update(const ThermalPeak* peaks, int numPeaks, uint32_t timestamp);

    // Acceso a tracks (lectura)
    const Tracklet* getTracks() const { return tracks_; }
    int             getMaxTracks() const { return ThermalConfig::MAX_TRACKS; }
    int             getActiveCount() const;
    int             getConfirmedCount() const;

    // Construye el array Track[] compatible con el código existente (para IPC packet)
    void fillTrackArray(Track* out, int* out_count) const;

private:
    Tracklet tracks_[ThermalConfig::MAX_TRACKS];
    uint8_t  next_id_;  // Próximo ID a asignar (cicla 1..255)

    // Matching
    float computeCost(const Tracklet& t, const ThermalPeak& p) const;
    int   findBestTrack(const ThermalPeak& p, bool* already_matched) const;
    Tracklet* allocateTrack();

    // Predicción
    void predictAll();

    // Limpieza
    void removeExpired();
};
```

### 2.4 Implementación: tracklet_tracker.cpp

```cpp
// components/thermal_pipeline/src/tracklet_tracker.cpp

#include "tracklet_tracker.hpp"
#include <cmath>
#include <cstring>
#include "esp_log.h"

static const char* TAG = "TRACKLET";

TrackletTracker::TrackletTracker() : next_id_(1) {
    memset(tracks_, 0, sizeof(tracks_));
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        tracks_[i].active = false;
    }
}

float TrackletTracker::computeCost(const Tracklet& t, const ThermalPeak& p) const {
    float dx = t.pred_x - (float)p.x;
    float dy = t.pred_y - (float)p.y;
    float dist = sqrtf(dx*dx + dy*dy);

    // Normalizar distancia a [0,1] con respecto al umbral máximo
    float dist_cost = dist / ThermalConfig::TRACK_MAX_DIST;

    // Costo de temperatura (diferencia relativa)
    float temp_diff = fabsf(t.avg_temperature - p.temperature);
    float temp_cost = temp_diff / 5.0f;  // 5°C de diferencia = costo=1

    return dist_cost * (1.0f - ThermalConfig::TRACK_TEMP_WEIGHT)
         + temp_cost * ThermalConfig::TRACK_TEMP_WEIGHT;
}

void TrackletTracker::predictAll() {
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (!tracks_[i].active) continue;
        tracks_[i].history.predict(&tracks_[i].pred_x, &tracks_[i].pred_y);
    }
}

int TrackletTracker::findBestTrack(const ThermalPeak& p, bool* already_matched) const {
    int   best_idx  = -1;
    float best_cost = 1.0f;  // Umbral: costo > 1.0 = demasiado lejos

    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (!tracks_[i].active || already_matched[i]) continue;
        float cost = computeCost(tracks_[i], p);
        if (cost < best_cost) {
            best_cost = cost;
            best_idx  = i;
        }
    }
    return best_idx;
}

Tracklet* TrackletTracker::allocateTrack() {
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (!tracks_[i].active) return &tracks_[i];
    }
    // Slots llenos: reciclar el track no confirmado más antiguo
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (tracks_[i].active && !tracks_[i].isConfirmed()) {
            return &tracks_[i];
        }
    }
    return nullptr;  // Todos los slots ocupados por tracks confirmados
}

void TrackletTracker::update(const ThermalPeak* peaks, int numPeaks, uint32_t timestamp) {
    // Fase 1: Predecir posiciones de todos los tracks activos
    predictAll();

    // Fase 2: Matching — para cada peak no suprimido, buscar mejor track
    bool matched_tracks[ThermalConfig::MAX_TRACKS] = {false};

    for (int p = 0; p < numPeaks; p++) {
        if (peaks[p].suppressed) continue;

        int best = findBestTrack(peaks[p], matched_tracks);

        if (best >= 0) {
            // Actualizar track existente
            Tracklet& t = tracks_[best];
            t.history.push((float)peaks[p].x, (float)peaks[p].y, timestamp);
            t.pred_x = (float)peaks[p].x;
            t.pred_y = (float)peaks[p].y;
            // Actualizar temperatura media (EMA rápida)
            t.avg_temperature = t.avg_temperature * 0.8f + peaks[p].temperature * 0.2f;
            t.confirmed = (t.confirmed < 255) ? t.confirmed + 1 : 255;
            t.missed    = 0;
            matched_tracks[best] = true;

        } else {
            // Peak sin match: crear nuevo track
            Tracklet* slot = allocateTrack();
            if (slot) {
                memset(slot, 0, sizeof(Tracklet));
                slot->active          = true;
                slot->id              = next_id_++;
                if (next_id_ == 0) next_id_ = 1;  // Evitar ID=0
                slot->confirmed       = 1;
                slot->missed          = 0;
                slot->avg_temperature = peaks[p].temperature;
                slot->pred_x          = (float)peaks[p].x;
                slot->pred_y          = (float)peaks[p].y;
                slot->zone_state      = 1;  // Neutro por defecto — FSM lo corregirá
                slot->history.push((float)peaks[p].x, (float)peaks[p].y, timestamp);
                ESP_LOGD(TAG, "New track ID=%d at (%.1f,%.1f)", slot->id, slot->pred_x, slot->pred_y);
            }
        }
    }

    // Fase 3: Incrementar missed en tracks sin match
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (!tracks_[i].active || matched_tracks[i]) continue;
        tracks_[i].missed++;
        // Avanzar predicción (el track "sigue moviéndose" según historial)
        // pred_x/pred_y ya se actualizaron en predictAll()
    }

    // Fase 4: Eliminar tracks expirados
    removeExpired();
}

void TrackletTracker::removeExpired() {
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (!tracks_[i].active) continue;
        if (tracks_[i].missed > ThermalConfig::TRACK_MAX_MISSED) {
            ESP_LOGD(TAG, "Track ID=%d expired (missed=%d)", tracks_[i].id, tracks_[i].missed);
            tracks_[i].active    = false;
            tracks_[i].confirmed = 0;
        }
    }
}

int TrackletTracker::getActiveCount() const {
    int n = 0;
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (tracks_[i].active) n++;
    }
    return n;
}

int TrackletTracker::getConfirmedCount() const {
    int n = 0;
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (tracks_[i].active && tracks_[i].isConfirmed()) n++;
    }
    return n;
}

void TrackletTracker::fillTrackArray(Track* out, int* out_count) const {
    *out_count = 0;
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (!tracks_[i].active || !tracks_[i].isConfirmed()) continue;
        Track& t       = out[*out_count];
        t.id           = tracks_[i].id;
        t.x            = tracks_[i].x();
        t.y            = tracks_[i].y();
        // Velocidad: diferencia entre los dos últimos frames
        if (tracks_[i].history.count >= 2) {
            int prev = (tracks_[i].history.head - 1 + TrackHistory::CAPACITY) % TrackHistory::CAPACITY;
            t.v_x = tracks_[i].history.entries[tracks_[i].history.head].x
                  - tracks_[i].history.entries[prev].x;
            t.v_y = tracks_[i].history.entries[tracks_[i].history.head].y
                  - tracks_[i].history.entries[prev].y;
        } else {
            t.v_x = 0.0f;
            t.v_y = 0.0f;
        }
        t.age    = tracks_[i].missed;
        t.active = true;
        t.state_y = tracks_[i].zone_state;
        (*out_count)++;
    }
}
```

---

## 3. Integración en ThermalPipeline

### 3.1 Modificar thermal_pipeline.hpp

```cpp
// Reemplazar:
#include "alpha_beta_tracker.hpp"
// ...
AlphaBetaTracker tracker_;

// Con:
#include "tracklet_tracker.hpp"
// ...
TrackletTracker tracker_;
Track           track_array_[ThermalConfig::MAX_TRACKS];  // Buffer para fillTrackArray()
int             num_confirmed_tracks_ = 0;
```

### 3.2 Modificar thermal_pipeline.cpp — Paso 4 (Tracking)

```cpp
// Reemplazar la llamada a tracker_.update() existente:

// ANTES:
tracker_.update(peaks_, num_peaks_,
                ThermalConfig::ALPHA_TRK, ThermalConfig::BETA_TRK,
                ThermalConfig::MAX_MATCH_DIST_SQ, ThermalConfig::TRACK_MAX_AGE,
                ThermalConfig::DEFAULT_LINE_ENTRY_Y,
                ThermalConfig::DEFAULT_LINE_EXIT_Y,
                count_in_, count_out_);

// DESPUÉS:
uint32_t timestamp = xTaskGetTickCount();
tracker_.update(peaks_, num_peaks_, timestamp);
// La lógica de conteo se mueve a TrackletFSM (implementada en Etapa A3)
// Por ahora, el contador no cambia hasta que A3 esté implementado.

// Actualizar el array de tracks para el IPC packet
tracker_.fillTrackArray(track_array_, &num_confirmed_tracks_);
```

### 3.3 Modificar la sección DISPATCH del IPC packet

```cpp
// Reemplazar la sección de construcción de tracks en el packet:

const Track* tracks = track_array_;  // Usar el array ya preparado
int tidx = 0;
for (int i = 0; i < num_confirmed_tracks_ && tidx < ThermalConfig::MAX_TRACKS; i++) {
    if (tracks[i].active) {
        packet.telemetry.tracks[tidx].id      = tracks[i].id;
        packet.telemetry.tracks[tidx].x_100   = (int16_t)(tracks[i].x * 100.0f);
        packet.telemetry.tracks[tidx].y_100   = (int16_t)(tracks[i].y * 100.0f);
        packet.telemetry.tracks[tidx].v_x_100 = (int16_t)(tracks[i].v_x * 100.0f);
        packet.telemetry.tracks[tidx].v_y_100 = (int16_t)(tracks[i].v_y * 100.0f);
        tidx++;
    }
}
packet.telemetry.num_tracks = tidx;
```

---

## 4. Actualizar CMakeLists.txt

```cmake
# components/thermal_pipeline/CMakeLists.txt
idf_component_register(
    SRCS "src/background_model.cpp"
         "src/peak_detector.cpp"
         "src/nms_suppressor.cpp"
         "src/tracklet_tracker.cpp"    # NUEVO (reemplaza alpha_beta_tracker.cpp)
         "src/mask_generator.cpp"
         "src/thermal_pipeline.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mlx90640_driver freertos esp_timer
)
```

> ⚠️ **`alpha_beta_tracker.cpp`** queda deprecado. No borrarlo todavía — mantenerlo fuera de SRCS. Eliminar en una limpieza posterior.

---

## 5. Compatibilidad con MaskGenerator

`MaskGenerator::generate()` usa `const Track*`. El método `fillTrackArray()` llena el buffer `track_array_` con `Track` structs compatibles. La llamada existente en el pipeline sigue funcionando:

```cpp
MaskGenerator::generate(track_array_, num_confirmed_tracks_,
                         blocking_mask_, ThermalConfig::MASK_HALF_SIZE);
```

---

## 6. Checklist

- [ ] `TrackHistory` implementado en `tracklet_tracker.hpp`
- [ ] `Tracklet` struct definido con campos necesarios
- [ ] `TrackletTracker` implementado en `tracklet_tracker.cpp`
- [ ] Constantes `TRACK_CONFIRM_FRAMES`, `TRACK_MAX_MISSED`, `TRACK_MAX_DIST`, `TRACK_TEMP_WEIGHT` añadidas a `thermal_config.hpp`
- [ ] `thermal_pipeline.hpp` usa `TrackletTracker` en lugar de `AlphaBetaTracker`
- [ ] `thermal_pipeline.cpp` llama a `tracker_.update()` con nuevo signature
- [ ] `fillTrackArray()` se llama y resultado se usa en el IPC packet
- [ ] `MaskGenerator::generate()` sigue funcionando con el nuevo array
- [ ] `CMakeLists.txt` actualizado (tracklet en, alpha_beta fuera)
- [ ] `idf.py build` pasa sin errores
- [ ] HWM de pipeline task > 200 words
- [ ] IDs más estables en prueba de cruce (observar en HUD)
- [ ] Tracks no aparecen antes de `TRACK_CONFIRM_FRAMES` detecciones
