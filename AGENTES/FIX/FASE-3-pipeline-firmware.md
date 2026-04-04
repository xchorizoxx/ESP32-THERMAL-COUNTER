# FASE 3 — Mejoras del Pipeline Firmware

## Identidad y alcance

Agente de edición de firmware C++. Esta fase aplica tres mejoras al pipeline de detección
y tracking y sube la frecuencia efectiva del sensor.

**Archivos objetivo (únicos):**
1. `components/thermal_pipeline/src/peak_detector.cpp` — Centroide sub-píxel
2. `components/thermal_pipeline/src/tracklet_fsm.cpp` — Histéresis FSM ≥2 frames
3. `components/thermal_pipeline/include/tracklet_fsm.hpp` — Campo `cross_streak` en FsmMemory
4. `components/thermal_pipeline/include/thermal_config.hpp` — I2C 400 kHz → 1 MHz
5. El driver I2C del sensor MLX (`components/mlx90640_driver/`) — verificar si la frecuencia se toma de `thermal_config.hpp` o si está hardcodeada

**Archivos que NO debes tocar:** `tracklet_tracker.cpp`, `thermal_pipeline.cpp`, `app.js`, `index.html`.

---

## Cambio 3-A — Centroide sub-píxel en `peak_detector.cpp`

### Problema
Actualmente se reporta la coordenada entera del máximo local (columna c, fila r).
Un cuerpo humano a 3-4m del sensor ocupa 4-9 píxeles. El centroide térmico ponderado
del vecindario 3×3 da una posición sub-píxel más estable, reduciendo los saltos discretos
de 1 celda (= 10px en el canvas) cuando la persona se mueve despacio.

### Prerequisito — Verificar firma de `ThermalPeak`

El struct `ThermalPeak` en `thermal_types.hpp` actualmente es:
```cpp
struct ThermalPeak {
    uint8_t x;           // Entero
    uint8_t y;           // Entero
    float   temperature;
    bool    suppressed;
};
```

Para soportar sub-píxel, los campos `x` e `y` deben cambiarse a `float`.
**Verificar en `thermal_types.hpp` si ya son float. Si no, cambiarlos primero.**

### BUSCAR en `thermal_types.hpp`:
```
struct ThermalPeak {
    uint8_t x;              ///< Column [0..31]
    uint8_t y;              ///< Row [0..23]
    float   temperature;    ///< Temperature in °C
    bool    suppressed;     ///< NMS flag: true = suppressed by a hotter peak
};
```

### REEMPLAZAR POR (en `thermal_types.hpp`):
```
struct ThermalPeak {
    float   x;              ///< Column [0.0..31.0] — sub-pixel centroid
    float   y;              ///< Row [0.0..23.0]    — sub-pixel centroid
    float   temperature;    ///< Temperature in °C (peak temperature)
    bool    suppressed;     ///< NMS flag: true = suppressed by a hotter peak
};
```

### BUSCAR en `peak_detector.cpp` (el bloque de detección final):
```
            if (*numPeaks < maxPeaks) {
                peaks[*numPeaks].x = (uint8_t)c;
                peaks[*numPeaks].y = (uint8_t)r;
                peaks[*numPeaks].temperature = val;
                peaks[*numPeaks].suppressed  = false;
                (*numPeaks)++;
            } else {
                return; // Capacity reached
            }
```

### REEMPLAZAR POR:
```
            if (*numPeaks < maxPeaks) {
                // Centroide sub-píxel ponderado por temperatura en el vecindario 3×3.
                // Reduce saltos discretos cuando la persona se mueve entre celdas.
                float sum_w = 0.0f, sum_wx = 0.0f, sum_wy = 0.0f;
                for (int dr2 = -1; dr2 <= 1; dr2++) {
                    for (int dc2 = -1; dc2 <= 1; dc2++) {
                        float w = currentFrame[(r + dr2) * cols + (c + dc2)];
                        if (w < tempMin) w = 0.0f; // no pesar píxeles fríos
                        sum_w  += w;
                        sum_wx += w * (float)(c + dc2);
                        sum_wy += w * (float)(r + dr2);
                    }
                }
                peaks[*numPeaks].x = (sum_w > 0.0f) ? (sum_wx / sum_w) : (float)c;
                peaks[*numPeaks].y = (sum_w > 0.0f) ? (sum_wy / sum_w) : (float)r;
                peaks[*numPeaks].temperature = val;
                peaks[*numPeaks].suppressed  = false;
                (*numPeaks)++;
            } else {
                return; // Capacity reached
            }
```

### Ajustes de cascada por cambio de tipo en `ThermalPeak::x/y`

Después de cambiar `uint8_t x/y` a `float x/y` en el struct, buscar en todos los
archivos que leen `peaks[i].x` o `peaks[i].y` y eliminar los casts `(float)p.x`:

- `tracklet_tracker.cpp` línea `const float dx = t.pred_x - (float)p.x;`
  → cambiar a `const float dx = t.pred_x - p.x;`
- `tracklet_tracker.cpp` línea `slot->pred_x = (float)peaks[p].x;`
  → cambiar a `slot->pred_x = peaks[p].x;`
- `tracklet_tracker.cpp` linea `t.history.push((float)peaks[p].x, (float)peaks[p].y, timestamp);`
  → cambiar a `t.history.push(peaks[p].x, peaks[p].y, timestamp);`
- `nms_suppressor.cpp` usa `peaks[j].x` como `int` para el cálculo de distancia.
  Cambiar el tipo de `xj/yj` a `float` y actualizar los `dx/dy/d2` a float:

### BUSCAR en `nms_suppressor.cpp`:
```
        const int xj = peaks[j].x;
        const int yj = peaks[j].y;
```

### REEMPLAZAR POR:
```
        const float xj = peaks[j].x;
        const float yj = peaks[j].y;
```

### BUSCAR en `nms_suppressor.cpp`:
```
            const int dx = (int)xj - (int)peaks[k].x;
            const int dy = (int)yj - (int)peaks[k].y;
            const int d2 = dx * dx + dy * dy;

            if (d2 <= radiusSq) {
```

### REEMPLAZAR POR:
```
            const float dx = xj - peaks[k].x;
            const float dy = yj - peaks[k].y;
            const float d2 = dx * dx + dy * dy;

            if (d2 <= (float)radiusSq) {
```

---

## Cambio 3-B — Histéresis FSM de cruce ≥2 frames en `tracklet_fsm.hpp`

### Problema
Un cruce de línea se registra en el mismo frame que ocurre. Si hay ruido de posición,
un rebote en la misma línea puede generar 2 conteos en 2 frames. La histéresis exige
que el cruce se mantenga en la misma dirección por ≥2 frames consecutivos.

### BUSCAR en `tracklet_fsm.hpp` la definición de `FsmMemory`:
```
    struct FsmMemory {
        uint8_t   id;
        FsmState  state;
        uint8_t   zone;  // Para modo legacy (zona Y)
    };
```

> **Nota:** Si `FsmMemory` tiene una estructura diferente a la mostrada, reportar
> el contenido exacto antes de aplicar el cambio.

### REEMPLAZAR POR:
```
    struct FsmMemory {
        uint8_t   id;
        FsmState  state;
        uint8_t   zone;          // Para modo legacy (zona Y)
        int8_t    cross_streak;  // +N = N frames cruzando en sentido +1, -N en sentido -1
        int       last_line_idx; // Índice de la última línea cruzada (para no contar 2 veces)
    };
```

### BUSCAR en `tracklet_fsm.cpp` (bloque de detección de cruce en modo segmentos):
```
                    if (cross == 1 && !already_counted_out) {
                        countOut++;
                        already_counted_out = true;
                        ESP_LOGI(TAG, "Track ID=%d crossed line '%s' -> +1 OUT", t.id, seg.name);
                    } else if (cross == -1 && !already_counted_in) {
                        countIn++;
                        already_counted_in = true;
                        ESP_LOGI(TAG, "Track ID=%d crossed line '%s' -> +1 IN", t.id, seg.name);
                    }
```

### REEMPLAZAR POR:
```
                    // Histéresis: solo contar si el cruce persiste ≥2 frames consecutivos
                    // en la misma dirección, para filtrar rebotes y ruido de posición.
                    if (cross != 0) {
                        if (mem->cross_streak == 0 || (mem->cross_streak > 0) == (cross > 0)) {
                            // Misma dirección: acumular streak
                            mem->cross_streak += cross;
                        } else {
                            // Dirección opuesta: resetear
                            mem->cross_streak = cross;
                        }

                        const int HYSTERESIS_FRAMES = 2;
                        if (mem->cross_streak >= HYSTERESIS_FRAMES && !already_counted_out) {
                            countOut++;
                            already_counted_out = true;
                            mem->cross_streak = 0;
                            ESP_LOGI(TAG, "Track ID=%d crossed line '%s' -> +1 OUT", t.id, seg.name);
                        } else if (mem->cross_streak <= -HYSTERESIS_FRAMES && !already_counted_in) {
                            countIn++;
                            already_counted_in = true;
                            mem->cross_streak = 0;
                            ESP_LOGI(TAG, "Track ID=%d crossed line '%s' -> +1 IN", t.id, seg.name);
                        }
                    } else {
                        // Sin cruce este frame: decay del streak
                        if (mem->cross_streak > 0) mem->cross_streak--;
                        else if (mem->cross_streak < 0) mem->cross_streak++;
                    }
```

> **Nota:** El campo `mem` debe obtenerse para el modo segmentos también.
> Actualmente el bloque de segmentos no usa `FsmMemory`. Añadir `findState`/
> `allocateState` para el track antes del bucle `for (li...)`.
> Ver el bloque de modo legacy como referencia de cómo obtener `mem`.

---

## Cambio 3-C — Subir I2C a 1 MHz en `thermal_config.hpp`

### Investigación previa (OBLIGATORIA)
Antes de aplicar este cambio, el agente debe localizar dónde se inicializa el I2C
en el driver del MLX90640:
```
grep -r "I2C_FREQ_HZ\|clk_speed\|i2c_param_config\|i2c_master" components/mlx90640_driver/
```
Si `I2C_FREQ_HZ` de `thermal_config.hpp` se usa directamente en la inicialización → cambiar
solo el valor en `thermal_config.hpp`. Si está hardcodeado en el driver → cambiar ahí.

### BUSCAR en `thermal_config.hpp`:
```
constexpr int I2C_FREQ_HZ = 400000; // 400 kHz Fast Mode
```

### REEMPLAZAR POR:
```
// Fast-Mode Plus (FM+): 1 MHz.
// Compatible con MLX90640 (datasheet §7.4) y ESP32-S3 I2C hardware.
// REQUISITO HARDWARE: pull-ups externos de 1kΩ en SDA y SCL.
// Con cables cortos (<10cm) puede operar a 1MHz sin problemas.
// Si hay errores I2C frecuentes, bajar a 800000 (800 kHz).
constexpr int I2C_FREQ_HZ = 1000000; // 1 MHz Fast-Mode Plus
```

### Ajustar frecuencia del sensor MLX90640 a 32 Hz

Buscar en `components/mlx90640_driver/` donde se configura el refresh rate del sensor.
Típicamente es una llamada a `MLX90640_SetRefreshRate(addr, rate)` donde rate es una
constante de la librería:
- `0x00` = 0.5 Hz, `0x01` = 1 Hz, `0x02` = 2 Hz, `0x03` = 4 Hz,
- `0x04` = 8 Hz, `0x05` = 16 Hz, `0x06` = 32 Hz, `0x07` = 64 Hz

### Acción: localizar la llamada y cambiar el rate a `0x06` (32 Hz) si actualmente es `0x05` (16 Hz).

### Cálculo de validación:
- A 1 MHz I2C y 32 Hz sensor: ventana = 31.25 ms por sub-página
- Tiempo de lectura: 1664 bytes × 10 bits / 1,000,000 = 16.6 ms < 31.25 ms ✅
- Margen: 14.6 ms para procesamiento del pipeline. Suficiente.

---

## Verificación tras Fase 3

1. Compilar y confirmar que no hay errores de tipo por el cambio `uint8_t` → `float` en ThermalPeak.
2. En el serial monitor, verificar que los tracks reportan posiciones con decimales (sub-píxel).
3. Los cruces de línea en segmentos deben registrarse solo tras 2 frames consecutivos de cruce.
4. Si el hardware lo soporta (pull-ups adecuados), verificar que el sensor opera a 32 Hz revisando el FPS en el HUD.
5. Notificar al usuario para compilar y flashear antes de pasar a Fase 4.

> **⚠️ WARNING DE HARDWARE:** El cambio a 1 MHz I2C requiere pull-up resistors de
> valor apropiado (≤1kΩ en SDA y SCL). Si el bus tiene resistencias de 4.7kΩ o 10kΩ,
> las señales tendrán flancos lentos y la comunicación fallará de forma intermitente.
> Verificar con osciloscopio o logic analyzer si hay errores I2C tras el cambio.
