# FASE 1 — Quick Wins: Corrección de EMA + Velocidad + Cleanup

## Identidad y alcance

Agente de edición quirúrgica. Esta fase corrige tres problemas independientes de alta
prioridad que no requieren cambios de arquitectura. Todos los cambios son reversibles.

**Archivos objetivo:**
1. `components/web_server/src/web/app.js` — eliminar EMA doble + suavizar velocidad
2. `components/thermal_pipeline/include/thermal_config.hpp` — ajustar `TRACK_DISPLAY_SMOOTH`
3. `components/thermal_pipeline/src/tracklet_tracker.cpp` — suavizar velocidad en firmware
4. `components/thermal_pipeline/src/alpha_beta_tracker.cpp` — **ELIMINAR**
5. `components/thermal_pipeline/include/alpha_beta_tracker.hpp` — **ELIMINAR**

**Archivos que NO debes tocar:** ningún otro.

---

## Cambio 1-A — Eliminar el EMA doble en `app.js`

### Problema
El firmware ya envía `display_x/display_y` suavizados con α=0.4. El cliente JS
aplica un segundo EMA (α=0.3) encima, creando un lag en cascada de 300-500 ms.

### BUSCAR (bloque exacto en la sección `processBinaryFrame`):
```
        let t;
        if(window.clientTracks[tid]) {
            t = window.clientTracks[tid];
            t.x = t.x * 0.7 + tx * 0.3;     // EMA espacial suaviza saltos enteros
            t.y = t.y * 0.7 + ty * 0.3;
            t.vx = t.vx * 0.8 + tvx * 0.2;  // EMA vector de velocidad suaviza brincos
            t.vy = t.vy * 0.8 + tvy * 0.2;
        } else {
            t = { id: tid, x: tx, y: ty, vx: tvx, vy: tvy };
            window.clientTracks[tid] = t;
        }
```

### REEMPLAZAR POR:
```
        let t;
        if(window.clientTracks[tid]) {
            t = window.clientTracks[tid];
            // El firmware ya envía display_x/y suavizados con EMA α=0.65.
            // El cliente usa directamente la posición recibida sin segundo filtro.
            t.x  = tx;
            t.y  = ty;
            // Suavizado mínimo de velocidad en cliente para estabilidad visual del vector.
            t.vx = t.vx * 0.5 + tvx * 0.5;
            t.vy = t.vy * 0.5 + tvy * 0.5;
        } else {
            t = { id: tid, x: tx, y: ty, vx: tvx, vy: tvy };
            window.clientTracks[tid] = t;
        }
```

---

## Cambio 1-B — Ajustar `TRACK_DISPLAY_SMOOTH` en `thermal_config.hpp`

### Problema
Con el EMA del cliente eliminado (Cambio 1-A), el único suavizado es el del firmware.
α=0.4 sigue siendo demasiado lento. Subir a 0.65 reduce el lag sin perder estabilidad.

### BUSCAR (línea exacta):
```
constexpr float TRACK_DISPLAY_SMOOTH  = 0.4f;  ///< EMA alpha for HUD display position (0=frozen, 1=raw)
```

### REEMPLAZAR POR:
```
constexpr float TRACK_DISPLAY_SMOOTH  = 0.65f; ///< EMA alpha for HUD display position. 0.65: reactive but stable. (0=frozen, 1=raw)
```

---

## Cambio 1-C — Suavizar velocidad transmitida en `tracklet_tracker.cpp`

### Problema
En `fillTrackArray()`, la velocidad se calcula como la diferencia del último par de
frames (`entries[head] - entries[head-1]`). Es extremadamente ruidosa porque es solo
1 muestra. El historial de 20 frames existe pero no se usa.

### BUSCAR (bloque exacto en `fillTrackArray`):
```
        // Velocity: difference between the two most-recent history entries
        if (tracks_[i].history.count >= 2) {
            const int h    = tracks_[i].history.head;
            const int prev = (h - 1 + TrackHistory::CAPACITY) % TrackHistory::CAPACITY;
            t.v_x = tracks_[i].history.entries[h].x - tracks_[i].history.entries[prev].x;
            t.v_y = tracks_[i].history.entries[h].y - tracks_[i].history.entries[prev].y;
        } else {
            t.v_x = 0.0f;
            t.v_y = 0.0f;
        }
```

### REEMPLAZAR POR:
```
        // Velocity: media de las últimas min(count,4) muestras del historial.
        // Más robusta que el diff de 1 frame. El historial de 20 frames ya existe.
        if (tracks_[i].history.count >= 2) {
            const int samples = (tracks_[i].history.count < 4) ? tracks_[i].history.count : 4;
            const int h       = tracks_[i].history.head;
            const int prev    = (h - samples + TrackHistory::CAPACITY) % TrackHistory::CAPACITY;
            t.v_x = (tracks_[i].history.entries[h].x - tracks_[i].history.entries[prev].x) / (float)samples;
            t.v_y = (tracks_[i].history.entries[h].y - tracks_[i].history.entries[prev].y) / (float)samples;
        } else {
            t.v_x = 0.0f;
            t.v_y = 0.0f;
        }
```

---

## Cambio 1-D — Eliminar `AlphaBetaTracker` (código muerto)

`AlphaBetaTracker` fue reemplazado por `TrackletTracker` en la revisión A2.
Ya no se instancia ni se llama en `thermal_pipeline.cpp`. Su presencia solo
confunde y aumenta el tamaño del binario.

### Acción: Eliminar estos dos archivos del sistema de archivos:
- `components/thermal_pipeline/src/alpha_beta_tracker.cpp`
- `components/thermal_pipeline/include/alpha_beta_tracker.hpp`

### Verificar que `thermal_pipeline.cpp` NO incluye `alpha_beta_tracker.hpp`:
Hacer grep del string `alpha_beta` en `thermal_pipeline.cpp`. Si aparece, reportar
antes de borrar. Si no aparece (comportamiento esperado), proceder con la eliminación.

### Acción: Eliminar de `CMakeLists.txt` si aparece listado:
Buscar `alpha_beta_tracker.cpp` en `components/thermal_pipeline/CMakeLists.txt`.
Si aparece en el listado de SRCS, eliminar esa línea. Si no aparece, no tocar el archivo.

---

## Verificación tras Fase 1

1. El agente de build debe reportar que `alpha_beta_tracker.cpp` ya no existe y
   que el CMake no lo referencia.
2. En el monitor serial, el vector de velocidad de los tracks debe moverse de forma
   más suave entre frames.
3. El delay visual del cuadrito de tracking debe reducirse notablemente (de ~500ms a
   ~100ms).
4. Notificar al usuario para compilar y flashear antes de pasar a Fase 2.
