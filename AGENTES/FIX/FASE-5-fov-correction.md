# FASE 5 — Corrección Geométrica FOV (Perspectiva con slider de altura)

## Identidad y alcance

Agente de implementación de geometría. Esta fase añade una corrección de perspectiva
paramétrica basada en la altura del sensor sobre el suelo. Transforma las coordenadas
discretas del grid térmico (0..31, 0..23) en coordenadas métricas consistentes en el
plano del suelo, normalizadas de vuelta al espacio del grid para el tracker.

**Archivos objetivo (únicos):**
1. `components/thermal_pipeline/include/thermal_config.hpp` — altura del sensor
2. `components/thermal_pipeline/src/thermal_pipeline.cpp` — tabla de corrección
3. `components/thermal_pipeline/src/peak_detector.cpp` — aplicar corrección al centroide
4. `components/web_server/src/web/index.html` — slider de altura en settings
5. `components/web_server/src/web/app.js` — enviar nuevo parámetro `sensor_height`

**Archivos que NO debes tocar:** `tracklet_tracker.*`, `tracklet_fsm.*`, `thermal_types.hpp`.

---

## Teoría de la corrección geométrica

El sensor MLX90640 tiene un FOV de **110° horizontal × 75° vertical**, montado
mirando hacia abajo en el centro de la puerta a una altura H sobre el suelo.

Para el píxel en columna `c` (0..31) y fila `r` (0..23):
```
angle_x(c) = (c - 15.5) × (55° / 16)   [de -55° a +55°]
angle_y(r) = (r - 11.5) × (37.5° / 12) [de -37.5° a +37.5°]

x_suelo(c, r) = H × tan(angle_x(c))                         [metros]
y_suelo(c, r) = H × tan(angle_y(r)) / cos(angle_x(c))       [metros, con corrección doble-eje]
```

Luego se re-normalizan las coordenadas métricas al espacio del grid [0..31, 0..23]
para que el tracker y el HUD sigan funcionando sin cambios.

La tabla de corrección es una bijección: para cada (c,r) entero en el grid,
se calcula la posición corregida (cx, cy) en coordenadas sub-grid.

---

## Cambio 5-A — Añadir `SENSOR_HEIGHT_M` configurable en `thermal_config.hpp`

### BUSCAR:
```
constexpr float DOOR_HEIGHT_M = 3.6f;    // Sensor height above floor [m]
```

### REEMPLAZAR POR:
```
extern float SENSOR_HEIGHT_M;  // Altura del sensor sobre el suelo [m]. Configurable desde UI.
constexpr float DOOR_HEIGHT_M = 3.6f;    // Solo referencia de diseño (no usado en cálculos)
```

---

## Cambio 5-B — Inicializar `SENSOR_HEIGHT_M` y tabla FOV en `thermal_pipeline.cpp`

### BUSCAR (el bloque de inicialización de variables extern en el namespace):
```
namespace ThermalConfig {
    float EMA_ALPHA = 0.05f;
```

### REEMPLAZAR POR:
```
namespace ThermalConfig {
    float EMA_ALPHA = 0.05f;
    float SENSOR_HEIGHT_M = 3.0f; // Valor inicial por defecto [m]. Ajustar desde UI.
```

### Añadir tabla de corrección FOV como static dentro de `ThermalPipeline` (después del bloque de inicialización):

### BUSCAR (después de la llave cierre del namespace, antes de `static const char* TAG`):
```
static const char* TAG = "PIPELINE";
```

### REEMPLAZAR POR:
```
// =========================================================================
//  Tabla de corrección geométrica FOV (32×24 → coordenadas sub-grid métricas)
//  Se recalcula cada vez que cambia SENSOR_HEIGHT_M.
// =========================================================================

struct FovCorrection {
    float cx[ThermalConfig::MLX_COLS][ThermalConfig::MLX_ROWS]; // X corregida [0..31]
    float cy[ThermalConfig::MLX_COLS][ThermalConfig::MLX_ROWS]; // Y corregida [0..23]
    bool  ready;
};

static FovCorrection fov_table = { {}, {}, false };

// Recalcula la tabla de corrección cuando cambia la altura del sensor.
// FOV: 110° H × 75° V → semiángulos 55° H, 37.5° V.
static void recomputeFovTable(float height_m)
{
    constexpr float H_FOV_HALF_RAD = 55.0f  * (float)M_PI / 180.0f; // 55°
    constexpr float V_FOV_HALF_RAD = 37.5f  * (float)M_PI / 180.0f; // 37.5°
    constexpr int   COLS = ThermalConfig::MLX_COLS; // 32
    constexpr int   ROWS = ThermalConfig::MLX_ROWS; // 24

    // Dimensiones físicas proyectadas en el suelo en los extremos del FOV [metros]
    const float ground_w = 2.0f * height_m * tanf(H_FOV_HALF_RAD);
    const float ground_h = 2.0f * height_m * tanf(V_FOV_HALF_RAD);

    for (int c = 0; c < COLS; c++) {
        for (int r = 0; r < ROWS; r++) {
            // Ángulo de este píxel
            float ax = ((float)c - (COLS - 1) / 2.0f) / ((COLS - 1) / 2.0f) * H_FOV_HALF_RAD;
            float ay = ((float)r - (ROWS - 1) / 2.0f) / ((ROWS - 1) / 2.0f) * V_FOV_HALF_RAD;

            // Posición métrica en el suelo
            float x_m = height_m * tanf(ax);
            float y_m = height_m * tanf(ay) / cosf(ax); // corrección doble eje

            // Re-normalizar al espacio del grid [0..31, 0..23]
            // Centrado en la mitad del ground_w/ground_h
            fov_table.cx[c][r] = ((x_m + ground_w / 2.0f) / ground_w) * (COLS - 1);
            fov_table.cy[c][r] = ((y_m + ground_h / 2.0f) / ground_h) * (ROWS - 1);

            // Clamp por seguridad
            if (fov_table.cx[c][r] < 0.0f) fov_table.cx[c][r] = 0.0f;
            if (fov_table.cx[c][r] > COLS - 1) fov_table.cx[c][r] = COLS - 1;
            if (fov_table.cy[c][r] < 0.0f) fov_table.cy[c][r] = 0.0f;
            if (fov_table.cy[c][r] > ROWS - 1) fov_table.cy[c][r] = ROWS - 1;
        }
    }
    fov_table.ready = true;
    ESP_LOGI("PIPELINE", "FOV table recalculated for H=%.2fm (ground %.2fm x %.2fm)",
             height_m, ground_w, ground_h);
}

static const char* TAG = "PIPELINE";
```

### Llamar `recomputeFovTable` en la inicialización del pipeline y cuando cambia la altura:

### BUSCAR (en `ThermalPipeline::init()`):
```
    ESP_LOGI(TAG, "Pipeline initialized (Standard stack: %.1f KB)",
```

### REEMPLAZAR POR:
```
    recomputeFovTable(ThermalConfig::SENSOR_HEIGHT_M);
    ESP_LOGI(TAG, "Pipeline initialized (Standard stack: %.1f KB)",
```

---

## Cambio 5-C — Aplicar corrección FOV en `peak_detector.cpp` al reportar el centroide

Esta corrección se aplica **después** del cálculo del centroide sub-píxel (Fase 3).
El centroide se calcula en coordenadas de grid `(cx_raw, cy_raw)` y luego se corrige.

### BUSCAR (al final de la detección válida, antes de `(*numPeaks)++`):
```
                peaks[*numPeaks].x = (sum_w > 0.0f) ? (sum_wx / sum_w) : (float)c;
                peaks[*numPeaks].y = (sum_w > 0.0f) ? (sum_wy / sum_w) : (float)r;
```

### REEMPLAZAR POR:
```
                float cx_raw = (sum_w > 0.0f) ? (sum_wx / sum_w) : (float)c;
                float cy_raw = (sum_w > 0.0f) ? (sum_wy / sum_w) : (float)r;

                // Corrección FOV: interpolar la tabla de corrección geométrica.
                // La tabla está indexada en enteros; se hace interpolación bilineal
                // para posiciones sub-píxel.
                if (fov_table.ready) {
                    int ci = (int)cx_raw;
                    int ri = (int)cy_raw;
                    float fx = cx_raw - (float)ci;
                    float fy = cy_raw - (float)ri;
                    // Clamp a límites de tabla
                    if (ci < 0) ci = 0;
                    if (ci >= cols - 1) ci = cols - 2;
                    if (ri < 0) ri = 0;
                    if (ri >= rows - 1) ri = rows - 2;
                    // Bilineal 2×2
                    float x00 = fov_table.cx[ci][ri];
                    float x10 = fov_table.cx[ci+1][ri];
                    float x01 = fov_table.cx[ci][ri+1];
                    float x11 = fov_table.cx[ci+1][ri+1];
                    float y00 = fov_table.cy[ci][ri];
                    float y10 = fov_table.cy[ci+1][ri];
                    float y01 = fov_table.cy[ci][ri+1];
                    float y11 = fov_table.cy[ci+1][ri+1];
                    cx_raw = x00*(1-fx)*(1-fy) + x10*fx*(1-fy) + x01*(1-fx)*fy + x11*fx*fy;
                    cy_raw = y00*(1-fx)*(1-fy) + y10*fx*(1-fy) + y01*(1-fx)*fy + y11*fx*fy;
                }

                peaks[*numPeaks].x = cx_raw;
                peaks[*numPeaks].y = cy_raw;
```

> **Nota:** `fov_table` debe ser visible en `peak_detector.cpp`. Declarar como
> `extern FovCorrection fov_table;` en un nuevo header `fov_correction.hpp` o
> pasarla como parámetro a `PeakDetector::detect()`. **Elegir la opción más limpia
> según la arquitectura existente** y reportar al usuario.

---

## Cambio 5-D — Añadir parámetro `sensor_height` al protocolo WebSocket

### Añadir al enum `ConfigCmdType` en `thermal_types.hpp`:

### BUSCAR:
```
    APPLY_CONFIG    ///< Batch-apply all parameters (handled in Core 0)
```

### REEMPLAZAR POR:
```
    APPLY_CONFIG,   ///< Batch-apply all parameters (handled in Core 0)
    SET_SENSOR_HEIGHT ///< Altura del sensor sobre el suelo [m]
```

### Añadir al switch de configuración en `thermal_pipeline.cpp`:

### BUSCAR:
```
                default: break;
```

### REEMPLAZAR POR:
```
                case ConfigCmdType::SET_SENSOR_HEIGHT:
                    ThermalConfig::SENSOR_HEIGHT_M = cmd.value;
                    recomputeFovTable(cmd.value);
                    ESP_LOGI(TAG, "Sensor height updated: %.2f m", cmd.value);
                    break;
                default: break;
```

---

## Cambio 5-E — Slider de altura en `index.html` (panel Settings > Básica)

### BUSCAR (justo antes del `<hr>` que separa las zonas muertas):
```
                    <hr style="border:0; border-top:1px solid rgba(255,255,255,0.1); margin:15px 0;">
                    <div class="form-group">
                        <label style="color:var(--neo-green); font-weight:bold;">Líneas de Conteo Personalizadas (Segmentos)</label>
```

### REEMPLAZAR POR:
```
                    <hr style="border:0; border-top:1px solid rgba(255,255,255,0.1); margin:15px 0;">
                    <div class="form-group">
                        <label>Altura Sensor: <span id="val-sensor_height" class="val-badge">3.0</span> m</label>
                        <input type="range" id="cfg-sensor_height" min="1.5" max="5.0" step="0.1"
                               value="3.0"
                               oninput="document.getElementById('val-sensor_height').innerText=parseFloat(this.value).toFixed(1)"
                               onchange="sendParam('sensor_height', parseFloat(this.value))">
                        <small>Altura del sensor térmico sobre el suelo. Afecta la corrección geométrica de perspectiva.</small>
                    </div>
                    <hr style="border:0; border-top:1px solid rgba(255,255,255,0.1); margin:15px 0;">
                    <div class="form-group">
                        <label style="color:var(--neo-green); font-weight:bold;">Líneas de Conteo Personalizadas (Segmentos)</label>
```

### Añadir función helper `sendParam` en `app.js` (antes de `saveConfig`):

### BUSCAR:
```
function saveConfig() {
```

### REEMPLAZAR POR:
```
function sendParam(key, val) {
    if (ws && ws.readyState === 1) {
        ws.send(JSON.stringify({ cmd: "SET_PARAM", param: key, val: val }));
        logMsg(`Set ${key}=${val}`);
    }
}

function saveConfig() {
```

---

## Verificación tras Fase 5

1. Al cambiar el slider de altura, el log serial debe mostrar `FOV table recalculated for H=X.XXm`.
2. Con altura=2m vs altura=4m, los tracks de personas en los bordes del canvas deben moverse
   a posiciones ligeramente distintas (la corrección geométrica es notable en los extremos del FOV).
3. El centro del canvas debe permanecer igual para cualquier altura (la corrección es identidad en el centro).
4. Compilar y flashear para validar en hardware.
