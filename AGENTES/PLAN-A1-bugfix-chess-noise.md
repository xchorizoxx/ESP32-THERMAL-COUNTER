# Etapa A1 — Bugfix Críticos + Chess Corrector + Noise Filter
## Fase A: Algoritmo Core (sin hardware nuevo)

**Dependencias:** Ninguna — puede implementarse sobre el código base actual  
**Hardware requerido:** Solo el MLX90640 existente  
**Tiempo estimado:** 2-3 días  
**Prioridad:** BLOQUEANTE — todas las demás etapas dependen de esta

> ⚠️ **Nota IDF 6.0:** Esta etapa no introduce APIs nuevas. Todos los cambios son en lógica de aplicación.

---

## 1. Bugs a Corregir (en orden de ejecución)

### Bug 1 — Doble inicialización del sensor (CRÍTICO)

**Archivo:** `thermal_pipeline.cpp` — función `run()`

**Problema:** El sensor se inicializa en `app_main()` y el pipeline lo reinicializa de nuevo en su bucle de arranque, realizando `DumpEE` y `ExtractParameters` dos veces. Según el datasheet, el MLX90640 tiene máximo 10 ciclos de escritura EEPROM de por vida.

**Corrección:** Eliminar el bloque de re-init del pipeline. El pipeline debe confiar en que el sensor ya está inicializado. Si falla `readFrame`, usar `resetI2C()` solamente.

```cpp
// thermal_pipeline.cpp — run()
// ELIMINAR este bloque completo:
// int init_retries = 0;
// while (init_retries < 3) {
//     if (sensor_.init() == ESP_OK) { break; }
//     ...
// }

// REEMPLAZAR con:
void ThermalPipeline::run()
{
    // Fase 0: verificar que el sensor ya fue inicializado por app_main()
    sensor_initialized_ = sensor_.isInitialized();
    if (!sensor_initialized_) {
        ESP_LOGE(TAG, "Sensor not initialized by app_main — pipeline in degraded mode");
    } else {
        ESP_LOGI(TAG, "Pipeline ready — sensor pre-initialized");
    }

    // Espera de estabilización térmica (mantener, pero sin re-init)
    ESP_LOGI(TAG, "Waiting 2000ms for thermal stabilization...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    const TickType_t period = pdMS_TO_TICKS(1000 / ThermalConfig::PIPELINE_FREQ_HZ);
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (true) {
        // ... resto del bucle sin cambios ...
    }
}
```

---

### Bug 2 — Estado inicial de track hardcodeado en y<12 (CRÍTICO para conteo)

**Archivo:** `alpha_beta_tracker.cpp` — Phase 4 de `update()`

**Problema:** El threshold `y < 12` ignora las líneas configurables `lineEntryY` y `lineExitY`. Si las líneas se reconfiguran, los tracks nacen con el estado incorrecto y generan conteos espurios.

```cpp
// ANTES (incorrecto):
freeTrack->state_y = (peaks[p].y < 12) ? 0 : 2;

// DESPUÉS (correcto):
// Calcular zona inicial basada en las líneas configuradas
if (peaks[p].y < (float)lineEntryY) {
    freeTrack->state_y = 0;  // Zona superior (entry)
} else if (peaks[p].y > (float)lineExitY) {
    freeTrack->state_y = 2;  // Zona inferior (exit)
} else {
    freeTrack->state_y = 1;  // Zona neutra — track nacido en centro
    // NOTA: Este track será descartado por la FSM en Etapa A3
    // Por ahora, marcarlo como state=1 para que la nueva lógica lo maneje
}
```

**Nota:** La FSM completa (que descarta tracks nacidos en el centro) se implementa en Etapa A3. Este fix solo corrige el threshold.

---

### Bug 3 — IPC Queue depth 15 consume 25 KB de SRAM

**Archivo:** `thermal_config.hpp` + `main.cpp`

**Corrección:**

```cpp
// thermal_config.hpp
constexpr int IPC_QUEUE_DEPTH = 4;  // Era 15. El pipeline descarta si cola llena (timeout=0)
```

Verificar que `main.cpp` usa `ThermalConfig::IPC_QUEUE_DEPTH` y no un literal. Si hay un literal `15`, reemplazarlo.

---

### Bug 4 — MAX_TRACKS hardcodeado en alpha_beta_tracker.cpp

**Archivo:** `alpha_beta_tracker.cpp`

Todos los literales `15` que se refieren a MAX_TRACKS deben reemplazarse con la constante:

```cpp
// Añadir al inicio de alpha_beta_tracker.cpp:
#include "thermal_config.hpp"

// Reemplazar en toda la clase:
// - tracks_[15]          → tracks_[ThermalConfig::MAX_TRACKS]
// - for (int i = 0; i < 15; i++)  → for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++)
// - bool peakAssigned[15] = {false}  → bool peakAssigned[ThermalConfig::MAX_PEAKS] = {false}
```

**Nota:** El campo `tracks_[15]` en la declaración del `.hpp` también debe actualizarse:
```cpp
// alpha_beta_tracker.hpp — en la sección private:
Track tracks_[ThermalConfig::MAX_TRACKS];
```

---

### Bug 5 — Race condition en ThermalConfig globals (CRÍTICO)

**Archivos:** `http_server.cpp`, `thermal_config.hpp`, `thermal_pipeline.cpp`

**Problema:** `BIOLOGICAL_TEMP_MIN`, `EMA_ALPHA` etc. son globals leídos por Core 1 y escritos por Core 0 sin sincronización. En Xtensa LX7, una escritura de `float` de 32 bits puede no ser atómica si hay context-switch.

**Solución mínima para esta etapa:** La config queue ya existe (`configQueue_`) y Core 1 la procesa correctamente. El problema está en `loadConfigFromNvs()` que escribe directamente a los globals desde Core 0 **antes** de que el pipeline arranque — esto es seguro porque el pipeline aún no está corriendo en ese momento.

El problema real es la función `GET_CONFIG` del WebSocket que **lee** los globals desde Core 0:

```cpp
// http_server.cpp — handleWebSocketMessage — caso GET_CONFIG
// SOLUCIÓN: usar snapshot atómico

// Añadir a http_server.cpp (al inicio del archivo):
static portMUX_TYPE s_config_mux = portMUX_INITIALIZER_UNLOCKED;

// En GET_CONFIG, leer todos los valores en sección crítica corta:
else if (strcmp(cmd->valuestring, "GET_CONFIG") == 0) {
    // Snapshot atómico de configuración
    float temp_bio, delta_t, alpha_ema;
    int line_entry, line_exit, nms_center, nms_edge, view_mode;
    
    portENTER_CRITICAL(&s_config_mux);
    temp_bio    = ThermalConfig::BIOLOGICAL_TEMP_MIN;
    delta_t     = ThermalConfig::BACKGROUND_DELTA_T;
    alpha_ema   = ThermalConfig::EMA_ALPHA;
    line_entry  = ThermalConfig::DEFAULT_LINE_ENTRY_Y;
    line_exit   = ThermalConfig::DEFAULT_LINE_EXIT_Y;
    nms_center  = ThermalConfig::NMS_RADIUS_CENTER_SQ;
    nms_edge    = ThermalConfig::NMS_RADIUS_EDGE_SQ;
    view_mode   = ThermalConfig::VIEW_MODE;
    portEXIT_CRITICAL(&s_config_mux);
    
    cJSON *resp = cJSON_CreateObject();
    // ... construir JSON con los valores del snapshot ...
}
```

Y en `thermal_pipeline.cpp`, rodear las **escrituras** de config (en el procesamiento de `configQueue_`) con la misma sección crítica. Las lecturas del pipeline son de Core 1 y no necesitan mux porque solo hay un productor (la queue) y un consumidor (el pipeline).

---

## 2. Chess Corrector

### 2.1 Problema real del chess pattern en MLX90640

El MLX90640 en modo chess alterna qué píxeles actualiza en cada sub-frame. Cuando se leen ambos sub-frames y se combinan, puede aparecer un patrón de tablero si los offsets de compensación (coeficientes IL_CHESS_C1, C2, C3) no se aplican correctamente.

**La librería Melexis `MLX90640_CalculateTo()` ya aplica la corrección de chess** en el modo chess configurado. El artefacto visible no es por falta de corrección matemática, sino por **visualizar un solo sub-frame** en lugar de esperar a tener ambos.

**Causa real del artefacto visual:**
```cpp
// En thermal_pipeline.cpp:
if (currentSubPage == 1) {
    memcpy(display_frame_, current_frame_, sizeof(display_frame_));
}
// El display_frame_ solo se actualiza en sub-page 1.
// Los píxeles de sub-page 0 (que se actualizaron en el frame anterior)
// pueden quedar "desincronizados" visualmente.
```

### 2.2 Solución: Acumulador de dos sub-frames

En lugar de un `ChessCorrector` complejo que re-implementa lo que ya hace la librería Melexis, la solución correcta es **mantener un buffer compuesto** que fusiona ambos sub-frames antes de procesar.

**Nuevo archivo:** `components/thermal_pipeline/include/frame_accumulator.hpp`

```cpp
#pragma once
/**
 * @file frame_accumulator.hpp
 * @brief Fusiona sub-frames de chess pattern para visualización sin artefactos.
 *
 * El MLX90640 en modo chess actualiza píxeles alternos en cada sub-frame.
 * Este acumulador mantiene el último valor válido de cada píxel,
 * independientemente del sub-frame en que fue leído.
 *
 * IMPORTANTE: La librería Melexis ya aplica corrección matemática de chess.
 * Este componente solo resuelve el artefacto VISUAL de mostrar un solo sub-frame.
 */
#include <cstdint>
#include <cstring>
#include "thermal_config.hpp"

class FrameAccumulator {
public:
    FrameAccumulator() : initialized_(false) {
        memset(composed_, 0, sizeof(composed_));
    }

    /**
     * @brief Integra un nuevo sub-frame en el buffer compuesto.
     *
     * @param frame     Frame completo de MLX90640_CalculateTo() [768 floats]
     * @param subpage   Sub-frame recibido (0 o 1)
     * @param composed  Buffer de salida con píxeles compuestos [768 floats]
     *
     * En modo chess:
     *   - Sub-page 0 actualiza píxeles donde (row+col) % 2 == 0
     *   - Sub-page 1 actualiza píxeles donde (row+col) % 2 == 1
     * Los píxeles no actualizados en este sub-frame retienen su valor anterior.
     */
    void integrate(const float* frame, uint8_t subpage, float* composed) {
        const int COLS = ThermalConfig::MLX_COLS;  // 32
        const int ROWS = ThermalConfig::MLX_ROWS;  // 24

        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                int idx = row * COLS + col;
                int pixel_subpage = (row + col) % 2;

                if (pixel_subpage == subpage) {
                    // Este píxel fue actualizado en este sub-frame
                    composed_[idx] = frame[idx];
                }
                // Los demás retienen su valor anterior en composed_[]
            }
        }

        if (!initialized_ && subpage == 1) {
            initialized_ = true;  // Tenemos al menos un ciclo completo
        }

        memcpy(composed, composed_, ThermalConfig::TOTAL_PIXELS * sizeof(float));
    }

    /**
     * @brief Indica si se ha completado al menos un ciclo completo (ambos sub-frames).
     */
    bool isReady() const { return initialized_; }

    /**
     * @brief Resetea el acumulador (tras reinicio del sensor).
     */
    void reset() {
        initialized_ = false;
        memset(composed_, 0, sizeof(composed_));
    }

private:
    float composed_[ThermalConfig::TOTAL_PIXELS];
    bool initialized_;
};
```

**Nuevo archivo:** `components/thermal_pipeline/src/frame_accumulator.cpp`

```cpp
// Implementación en el header (clase pequeña, todo inline es aceptable)
// No se necesita .cpp separado para esta clase.
```

### 2.3 Integración en thermal_pipeline.cpp

```cpp
// thermal_pipeline.hpp — añadir miembro privado:
#include "frame_accumulator.hpp"

class ThermalPipeline {
private:
    // ... miembros existentes ...
    FrameAccumulator frame_accumulator_;  // NUEVO
    float composed_frame_[ThermalConfig::TOTAL_PIXELS];  // NUEVO — frame fusionado
};
```

```cpp
// thermal_pipeline.cpp — en run(), sección de frame acquisition:

if (sensor_ok) {
    uint8_t currentSubPage = sensor_.getLastSubPageID();

    // NUEVO: integrar sub-frame en el acumulador
    frame_accumulator_.integrate(current_frame_, currentSubPage, composed_frame_);

    // Solo procesar cuando tenemos un ciclo completo
    if (!frame_accumulator_.isReady()) {
        // Esperar al segundo sub-frame antes de procesar
        goto dispatch;  // O usar flag para saltar el pipeline
    }

    // CAMBIO: usar composed_frame_ en lugar de current_frame_ para el pipeline
    // Paso 1: Background con composed_frame_
    BackgroundModel::update(composed_frame_, background_map_, blocking_mask_, ...);
    
    // Paso 2: Peak detection con composed_frame_
    PeakDetector::detect(composed_frame_, background_map_, peaks_, ...);

    // ... resto sin cambios ...

    // Para el IPC packet, también usar composed_frame_
    for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
        float val = (ThermalConfig::VIEW_MODE == 1) ?
                    (composed_frame_[i] - background_map_[i]) : composed_frame_[i];
        packet.image.pixels[i] = (int16_t)(val * 100.0f);
    }
}
```

---

## 3. Noise Filter (Kalman 1D por pixel)

### 3.1 Justificación

El MLX90640 tiene un NETD de ~0.1K a 1Hz y ~0.5K a 16Hz. A 16 FPS, el ruido es visible. Un filtro Kalman 1D independiente por píxel es el equilibrio correcto entre latencia y suavizado.

### 3.2 Nuevo archivo: `components/thermal_pipeline/include/noise_filter.hpp`

```cpp
#pragma once
/**
 * @file noise_filter.hpp
 * @brief Filtro Kalman 1D por píxel para reducción de ruido térmico.
 *
 * Aplica un filtro Kalman escalar a cada uno de los 768 píxeles
 * de forma independiente. Asumimos:
 *   - Modelo de proceso: temperatura es constante entre frames (proceso lento)
 *   - Ruido de proceso Q: varianza de cambio térmico real entre frames
 *   - Ruido de medición R: varianza del sensor (NETD²)
 *
 * A 16 FPS, NETD típico del MLX90640BAA ≈ 0.5K → R = 0.25
 * Tasa de cambio real de escena ≈ 0.1K/frame → Q = 0.01
 *
 * Memoria: 768 × 2 floats = 6 KB (en stack de la clase, no heap).
 */
#include "thermal_config.hpp"
#include <cstring>

class NoiseFilter {
public:
    static constexpr float DEFAULT_Q = 0.01f;   // Ruido de proceso
    static constexpr float DEFAULT_R = 0.25f;   // Ruido de medición (NETD² a 16Hz)
    static constexpr float INIT_P    = 1.0f;    // Covarianza inicial (alta incertidumbre)

    NoiseFilter() : q_(DEFAULT_Q), r_(DEFAULT_R), initialized_(false) {
        memset(x_, 0, sizeof(x_));
        for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
            p_[i] = INIT_P;
        }
    }

    void setParameters(float q, float r) { q_ = q; r_ = r; }

    /**
     * @brief Inicializa el estado del filtro con el primer frame.
     *
     * Debe llamarse con el primer frame compuesto válido.
     * Después de init, el filtro está listo para apply().
     */
    void init(const float* frame) {
        memcpy(x_, frame, ThermalConfig::TOTAL_PIXELS * sizeof(float));
        for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
            p_[i] = INIT_P;
        }
        initialized_ = true;
    }

    /**
     * @brief Aplica el filtro Kalman al frame de entrada.
     *
     * @param frame_in  Frame compuesto de entrada [TOTAL_PIXELS]
     * @param frame_out Frame filtrado de salida [TOTAL_PIXELS]
     *
     * Si no está inicializado, copia frame_in a frame_out sin filtrar.
     */
    void apply(const float* frame_in, float* frame_out) {
        if (!initialized_) {
            init(frame_in);
            memcpy(frame_out, frame_in, ThermalConfig::TOTAL_PIXELS * sizeof(float));
            return;
        }

        for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
            // Predicción
            // x_pred = x_[i]  (modelo de velocidad cero)
            float p_pred = p_[i] + q_;

            // Actualización
            float k = p_pred / (p_pred + r_);   // Ganancia de Kalman
            x_[i] = x_[i] + k * (frame_in[i] - x_[i]);
            p_[i] = (1.0f - k) * p_pred;

            frame_out[i] = x_[i];
        }
    }

    /**
     * @brief Resetea el filtro (tras reinicio del sensor o cambio brusco de escena).
     */
    void reset() {
        initialized_ = false;
        for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
            p_[i] = INIT_P;
        }
    }

    bool isInitialized() const { return initialized_; }

private:
    float x_[ThermalConfig::TOTAL_PIXELS];  // Estado estimado (temperatura)
    float p_[ThermalConfig::TOTAL_PIXELS];  // Covarianza del error
    float q_;
    float r_;
    bool initialized_;
};
```

### 3.3 Integración en thermal_pipeline

```cpp
// thermal_pipeline.hpp — añadir:
#include "noise_filter.hpp"

class ThermalPipeline {
private:
    NoiseFilter noise_filter_;
    float filtered_frame_[ThermalConfig::TOTAL_PIXELS];  // NUEVO
};
```

```cpp
// thermal_pipeline.cpp — en run(), después de frame_accumulator_.integrate():

if (frame_accumulator_.isReady()) {
    // Aplicar filtro de ruido al frame compuesto
    noise_filter_.apply(composed_frame_, filtered_frame_);

    // El pipeline usa filtered_frame_ para background y detección
    // (NO para el IPC image packet — enviar el frame sin filtrar para visualización fiel)
    BackgroundModel::update(filtered_frame_, background_map_, blocking_mask_, ...);
    PeakDetector::detect(filtered_frame_, background_map_, peaks_, ...);
    // ...

    // El IPC packet puede usar composed_frame_ o filtered_frame_ según VIEW_MODE
    // En modo Normal: composed_frame_ (más fiel a la realidad)
    // En modo Radar: (filtered_frame_ - background_map_) para subtracción limpia
}
```

**Nota:** Al resetear el sensor (comando `RETRY_SENSOR`), también resetear ambos:
```cpp
case ConfigCmdType::RETRY_SENSOR:
    if (sensor_.init() == ESP_OK) {
        sensor_initialized_ = true;
        frame_accumulator_.reset();
        noise_filter_.reset();
        bg_init_ = false;  // Forzar re-inicialización del background
    }
    break;
```

---

## 4. Modificaciones a CMakeLists.txt

```cmake
# components/thermal_pipeline/CMakeLists.txt
idf_component_register(
    SRCS "src/background_model.cpp"
         "src/peak_detector.cpp"
         "src/nms_suppressor.cpp"
         "src/alpha_beta_tracker.cpp"
         "src/mask_generator.cpp"
         "src/thermal_pipeline.cpp"
         # frame_accumulator y noise_filter son header-only, no agregar aquí
    INCLUDE_DIRS "include"
    REQUIRES mlx90640_driver freertos esp_timer
)
```

No es necesario agregar `.cpp` para `FrameAccumulator` y `NoiseFilter` — son clases completamente inline en sus headers. Si el agente decide separarlas en `.cpp`, actualizar `CMakeLists.txt` en consecuencia.

---

## 5. Orden de Implementación

1. **Aplicar Bug 3 primero** (IPC_QUEUE_DEPTH = 4) — libera SRAM antes de cualquier otra cosa.
2. **Aplicar Bug 4** (MAX_TRACKS hardcoded) — corrección de constantes, sin impacto funcional.
3. **Aplicar Bug 1** (doble init sensor) — eliminar el bloque de re-init en `run()`.
4. **Aplicar Bug 2** (estado inicial track) — depende de tener las líneas como parámetros, que ya existen.
5. **Compilar y probar** — el sistema debe funcionar igual que antes, sin regresiones.
6. **Implementar FrameAccumulator** — crear el header, integrarlo en el pipeline.
7. **Compilar y verificar** que desaparecen los artefactos visuales de chess.
8. **Implementar NoiseFilter** — crear el header, integrarlo después del acumulador.
9. **Compilar y verificar** HWM de stack del pipeline.
10. **Aplicar Bug 5** (race condition config) — añadir sección crítica en GET_CONFIG.

---

## 6. Checklist

- [ ] `IPC_QUEUE_DEPTH` cambiado a 4 en `thermal_config.hpp`
- [ ] Literales `15` de MAX_TRACKS reemplazados por `ThermalConfig::MAX_TRACKS`
- [ ] Bloque de re-init eliminado de `thermal_pipeline.cpp::run()`
- [ ] Estado inicial de track usa `lineEntryY`/`lineExitY` en lugar de `y < 12`
- [ ] `FrameAccumulator` creado en `include/frame_accumulator.hpp`
- [ ] `NoiseFilter` creado en `include/noise_filter.hpp`
- [ ] Ambos integrados en `thermal_pipeline.hpp` y `thermal_pipeline.cpp`
- [ ] `RETRY_SENSOR` resetea acumulador y filtro
- [ ] Race condition en `GET_CONFIG` mitigado con sección crítica
- [ ] `idf.py build` pasa sin errores
- [ ] HWM del pipeline >200 words (verificar en monitor serial)
- [ ] Artefacto chess no visible en el HUD
- [ ] Imagen más suave (filtro Kalman activo, verificar en modo Radar)
