# Etapa A3 — FSM de Puerta Infinita + Validación de Zonas
## Máquina de Estados para Conteo Robusto

**Dependencias:** Etapa A2 completada  
**Hardware requerido:** Ninguno adicional  
**Tiempo estimado:** 1-2 días

---

## 1. Concepto: Máquina de Estados Infinita

La FSM actual tiene 3 estados y cuenta al cruzar dos líneas. El problema: un track que "aparece" en el centro (zona neutra) puede contar aunque nunca haya cruzado ninguna línea real.

La **máquina de estados infinita** resuelve esto con las siguientes reglas:

```
REGLAS DE NACIMIENTO:
  - Track puede nacer en zona NORTE (y < line_entry_Y)
  - Track puede nacer en zona SUR   (y > line_exit_Y)
  - Track nacido en zona NEUTRA → descartado (fantasma)

ESTADOS Y TRANSICIONES:
  NORTE → NEUTRO → SUR = +1 Salida (persona fue de norte a sur)
  SUR   → NEUTRO → NORTE = +1 Entrada (persona fue de sur a norte)

REGLAS DE MUERTE:
  - Track que llega a una zona borde y no sale en N frames → eliminado
  - Esto evita "fantasmas de borde" que nunca cruzan

CICLO INFINITO:
  Una persona puede entrar, salir, volver a entrar, etc.
  Cada cruce completo (borde→centro→borde opuesto) cuenta uno.
  No hay estado "ya contado y bloqueado".
```

---

## 2. TrackletFSM

### 2.1 Nuevo archivo: `components/thermal_pipeline/include/tracklet_fsm.hpp`

```cpp
#pragma once
/**
 * @file tracklet_fsm.hpp
 * @brief FSM de puerta infinita para conteo de personas.
 *
 * Recibe el estado de tracks del TrackletTracker y aplica
 * la lógica de conteo con las reglas de zona.
 *
 * SEPARACIÓN DE RESPONSABILIDADES:
 * - TrackletTracker: gestiona identidades y posiciones
 * - TrackletFSM: gestiona conteo y zonas
 *
 * La configuración de líneas viene de ThermalConfig (actualizable desde UI).
 */
#include "tracklet_tracker.hpp"
#include <cstdint>

// Estados de zona para cada track
enum class TrackZone : uint8_t {
    UNBORN  = 0,  // Track nuevo, zona no asignada aún
    NORTH   = 1,  // En zona norte (y < lineEntryY)
    NEUTRAL = 2,  // En zona neutra (entre las dos líneas)
    SOUTH   = 3,  // En zona sur (y > lineExitY)
    GHOST   = 4,  // Nacido en zona neutra — descartado
    DEAD    = 5   // Eliminado por timeout en borde
};

class TrackletFSM {
public:
    TrackletFSM();

    /**
     * @brief Procesa todos los tracks activos y actualiza contadores.
     *
     * @param tracker     TrackletTracker con el estado actual
     * @param lineEntryY  Línea de entrada (Y, en pixels sensor 0..23)
     * @param lineExitY   Línea de salida (Y, en pixels sensor 0..23)
     * @param countIn     Contador de entradas (acumulativo, modificado in-place)
     * @param countOut    Contador de salidas (acumulativo, modificado in-place)
     */
    void update(TrackletTracker& tracker,
                int lineEntryY, int lineExitY,
                int& countIn, int& countOut);

    /**
     * @brief Resetea los contadores (comando desde UI).
     */
    void resetCounts(int& countIn, int& countOut) const {
        countIn  = 0;
        countOut = 0;
    }

private:
    // Historial de zona por ID de track
    // Mapeamos ID (1..255) → TrackZone usando array indexado por posición en tracker
    struct TrackState {
        uint8_t   id;           // ID del track al que pertenece este slot
        TrackZone zone;         // Zona actual
        uint8_t   border_ticks; // Frames en zona borde sin cruzar
    };

    TrackState states_[ThermalConfig::MAX_TRACKS];

    // Determina la zona de una posición Y
    static TrackZone classifyZone(float y, int lineEntryY, int lineExitY);

    // Encuentra o crea el slot de estado para un track ID
    TrackState* findState(uint8_t id);
    TrackState* allocateState(uint8_t id);

    // Límite de frames en borde antes de considerar track muerto
    static constexpr uint8_t BORDER_TIMEOUT_FRAMES = 20;
};
```

### 2.2 Implementación: `components/thermal_pipeline/src/tracklet_fsm.cpp`

```cpp
#include "tracklet_fsm.hpp"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "TRACK_FSM";

TrackletFSM::TrackletFSM() {
    memset(states_, 0, sizeof(states_));
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        states_[i].zone = TrackZone::UNBORN;
    }
}

TrackZone TrackletFSM::classifyZone(float y, int lineEntryY, int lineExitY) {
    if (y < (float)lineEntryY)  return TrackZone::NORTH;
    if (y > (float)lineExitY)   return TrackZone::SOUTH;
    return TrackZone::NEUTRAL;
}

TrackletFSM::TrackState* TrackletFSM::findState(uint8_t id) {
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (states_[i].id == id && states_[i].zone != TrackZone::UNBORN) {
            return &states_[i];
        }
    }
    return nullptr;
}

TrackletFSM::TrackState* TrackletFSM::allocateState(uint8_t id) {
    // Buscar slot libre (UNBORN o DEAD)
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (states_[i].zone == TrackZone::UNBORN || states_[i].zone == TrackZone::DEAD) {
            states_[i].id           = id;
            states_[i].border_ticks = 0;
            return &states_[i];
        }
    }
    return nullptr;
}

void TrackletFSM::update(TrackletTracker& tracker,
                          int lineEntryY, int lineExitY,
                          int& countIn, int& countOut)
{
    const Tracklet* tracks = tracker.getTracks();

    // Limpiar estados de IDs que ya no están activos
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (states_[i].zone == TrackZone::UNBORN || states_[i].zone == TrackZone::DEAD) continue;
        // Verificar si este ID sigue activo en el tracker
        bool found = false;
        for (int j = 0; j < ThermalConfig::MAX_TRACKS; j++) {
            if (tracks[j].active && tracks[j].id == states_[i].id) { found = true; break; }
        }
        if (!found) {
            states_[i].zone = TrackZone::DEAD;
        }
    }

    // Procesar cada track activo y confirmado
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        const Tracklet& t = tracks[i];
        if (!t.active || !t.isConfirmed()) continue;

        TrackZone current_zone = classifyZone(t.y(), lineEntryY, lineExitY);
        TrackState* state = findState(t.id);

        if (state == nullptr) {
            // Track nuevo — asignar estado inicial
            state = allocateState(t.id);
            if (!state) continue;

            if (current_zone == TrackZone::NEUTRAL) {
                // Track nacido en zona neutra → GHOST, ignorar
                state->zone = TrackZone::GHOST;
                ESP_LOGD(TAG, "Track ID=%d born in neutral zone — ghost", t.id);
                // Notificar al tracker para que lo descarte en la próxima iteración
                // Esto se hace via zone_state en el Tracklet
                // (el tracker no descarta por sí mismo, lo marca para que fillTrackArray lo ignore)
                const_cast<Tracklet&>(t).zone_state = 4; // GHOST
                continue;
            }
            state->zone = current_zone;
            const_cast<Tracklet&>(t).zone_state = (uint8_t)current_zone;
            continue;
        }

        // Track fantasma — ignorar siempre
        if (state->zone == TrackZone::GHOST) continue;

        TrackZone prev_zone = state->zone;

        // --- Transiciones de estado y conteo ---

        if (prev_zone == TrackZone::NORTH && current_zone == TrackZone::SOUTH) {
            // Salto directo norte→sur (persona muy rápida o sensor perdió frames)
            // Contar como salida
            countOut++;
            ESP_LOGI(TAG, "Track ID=%d: NORTH→SOUTH (direct) → +1 OUT (%d total)", t.id, countOut);
            state->zone = TrackZone::SOUTH;

        } else if (prev_zone == TrackZone::SOUTH && current_zone == TrackZone::NORTH) {
            // Salto directo sur→norte
            countIn++;
            ESP_LOGI(TAG, "Track ID=%d: SOUTH→NORTH (direct) → +1 IN (%d total)", t.id, countIn);
            state->zone = TrackZone::NORTH;

        } else if (prev_zone == TrackZone::NORTH && current_zone == TrackZone::NEUTRAL) {
            // Norte hacia centro — en tránsito hacia sur
            state->zone = TrackZone::NEUTRAL;
            state->border_ticks = 0;

        } else if (prev_zone == TrackZone::SOUTH && current_zone == TrackZone::NEUTRAL) {
            // Sur hacia centro — en tránsito hacia norte
            state->zone = TrackZone::NEUTRAL;
            state->border_ticks = 0;

        } else if (prev_zone == TrackZone::NEUTRAL && current_zone == TrackZone::SOUTH) {
            // Centro → sur = SALIDA completada
            countOut++;
            ESP_LOGI(TAG, "Track ID=%d: NEUTRAL→SOUTH → +1 OUT (%d total)", t.id, countOut);
            state->zone = TrackZone::SOUTH;

        } else if (prev_zone == TrackZone::NEUTRAL && current_zone == TrackZone::NORTH) {
            // Centro → norte = ENTRADA completada
            countIn++;
            ESP_LOGI(TAG, "Track ID=%d: NEUTRAL→NORTH → +1 IN (%d total)", t.id, countIn);
            state->zone = TrackZone::NORTH;

        } else {
            // Sin transición significativa — actualizar zona
            state->zone = current_zone;
        }

        // Contador de tiempo en borde
        if (current_zone == TrackZone::NORTH || current_zone == TrackZone::SOUTH) {
            state->border_ticks++;
        } else {
            state->border_ticks = 0;
        }

        // Actualizar zone_state en el Tracklet para el IPC packet
        const_cast<Tracklet&>(t).zone_state = (uint8_t)state->zone;
    }
}
```

---

## 3. Integración en ThermalPipeline

### 3.1 thermal_pipeline.hpp — añadir miembro

```cpp
#include "tracklet_fsm.hpp"

class ThermalPipeline {
private:
    TrackletTracker tracker_;
    TrackletFSM     door_fsm_;   // NUEVO
    // ...
};
```

### 3.2 thermal_pipeline.cpp — reemplazar sección de tracking

```cpp
// Donde antes estaba tracker_.update() con countIn/countOut:

// Paso 4a: Tracking (solo posiciones e identidades)
uint32_t timestamp = xTaskGetTickCount();
tracker_.update(peaks_, num_peaks_, timestamp);

// Paso 4b: FSM de puerta (conteo)
door_fsm_.update(tracker_,
                 ThermalConfig::DEFAULT_LINE_ENTRY_Y,
                 ThermalConfig::DEFAULT_LINE_EXIT_Y,
                 count_in_, count_out_);

// Paso 4c: Preparar array de tracks para IPC
tracker_.fillTrackArray(track_array_, &num_confirmed_tracks_);
```

### 3.3 Comando RESET_COUNTS

```cpp
case ConfigCmdType::RESET_COUNTS:
    count_in_  = 0;
    count_out_ = 0;
    // No resetear la FSM — los tracks existentes mantienen su estado de zona
    // para evitar conteos dobles al resetear
    ESP_LOGI(TAG, "Counters reset to 0");
    break;
```

---

## 4. Actualizar CMakeLists.txt

```cmake
idf_component_register(
    SRCS "src/background_model.cpp"
         "src/peak_detector.cpp"
         "src/nms_suppressor.cpp"
         "src/tracklet_tracker.cpp"
         "src/tracklet_fsm.cpp"         # NUEVO
         "src/mask_generator.cpp"
         "src/thermal_pipeline.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mlx90640_driver freertos esp_timer
)
```

---

## 5. Tests de Verificación

Verificar manualmente en el HUD los siguientes escenarios:

### Escenario 1: Persona cruzando normalmente
1. Persona entra desde el borde norte, pasa por el centro, sale por el borde sur
2. Esperado: +1 OUT al cruzar la línea sur
3. Verificar: el ID no cambia durante el recorrido

### Escenario 2: Objeto apareciendo en el centro
1. Colocar mano directamente en el centro del campo visual
2. Esperado: track creado pero marcado como GHOST, no cuenta
3. Verificar: en el HUD, el track aparece brevemente pero no genera conteo

### Escenario 3: Persona que se devuelve
1. Persona cruza hacia el sur, luego regresa hacia el norte
2. Esperado: +1 OUT al ir al sur, +1 IN al regresar al norte
3. El ciclo puede repetirse indefinidamente (FSM infinita)

### Escenario 4: Dos personas cruzando simultáneamente
1. Dos personas cruzando en direcciones opuestas al mismo tiempo
2. Esperado: IDs distintos, conteos separados
3. Verificar: sin intercambio de IDs (mejora sobre el tracker anterior)

---

## 6. Checklist

- [ ] `TrackZone` enum definido en `tracklet_fsm.hpp`
- [ ] `TrackletFSM` clase implementada en `tracklet_fsm.cpp`
- [ ] `TrackState` correctamente gestiona el historial de zonas por ID
- [ ] Tracks GHOST no generan conteos
- [ ] Transición NORTH→NEUTRAL→SOUTH genera +1 OUT
- [ ] Transición SOUTH→NEUTRAL→NORTH genera +1 IN
- [ ] Salto directo NORTH→SOUTH también cuenta (caso borde)
- [ ] `thermal_pipeline.hpp` incluye `TrackletFSM`
- [ ] `thermal_pipeline.cpp` llama a `door_fsm_.update()` después del tracker
- [ ] `CMakeLists.txt` incluye `tracklet_fsm.cpp`
- [ ] `idf.py build` pasa sin errores
- [ ] Los 4 escenarios de verificación manual pasan
