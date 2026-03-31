# Etapa B1 — Líneas de Conteo Dibujables desde la UI
## Segmentos Arbitrarios Configurables

**Dependencias:** Etapa A4 completada  
**Hardware requerido:** Ninguno adicional  
**Tiempo estimado:** 2 días

---

## 1. Concepto

En lugar de dos líneas horizontales fijas definidas por un valor Y, las líneas de conteo deben poder ser **segmentos arbitrarios** definidos por dos puntos `(x1,y1)→(x2,y2)` en el espacio del sensor (32×24).

Esto permite:
- Líneas diagonales para puertas oblicuas
- Múltiples líneas para corredores con varias puertas
- Ajuste fino a la geometría real de la instalación

---

## 2. Cambios en el Backend (C++)

### 2.1 Nueva estructura de línea en thermal_types.hpp

```cpp
// Añadir a thermal_types.hpp:

/**
 * @brief Segmento de línea de conteo en coordenadas del sensor (0..31 x 0..23).
 *
 * Define un segmento que, cuando un track lo cruza, dispara un conteo.
 * La dirección del cruce determina si es IN o OUT.
 */
struct __attribute__((packed)) CountingSegment {
    float x1;         // Punto inicio X [0..31]
    float y1;         // Punto inicio Y [0..23]
    float x2;         // Punto fin X [0..31]
    float y2;         // Punto fin Y [0..23]
    uint8_t id;       // ID de la línea (para múltiples líneas)
    char name[16];    // Nombre descriptivo
    bool enabled;     // Activa/inactiva
};

constexpr int MAX_COUNTING_LINES = 4;  // Máximo de líneas por puerta
```

### 2.2 Actualizar DoorConfig en thermal_config.hpp

```cpp
// En thermal_config.hpp, reemplazar las variables de línea simples por:

namespace ThermalConfig {
    // ... mantener DEFAULT_LINE_ENTRY_Y y DEFAULT_LINE_EXIT_Y para compatibilidad
    // con el sistema legado. Las nuevas líneas coexisten.

    struct DoorLineConfig {
        CountingSegment lines[MAX_COUNTING_LINES];
        uint8_t         num_lines;
        bool            use_segments;  // false = usar Y horizontal legacy
    };

    extern DoorLineConfig door_lines;  // Config global de líneas
}
```

### 2.3 Implementar detección de cruce de segmento en TrackletFSM

La detección de cruce de un segmento genérico usa el **producto vectorial** para determinar el lado:

```cpp
// En tracklet_fsm.hpp — añadir método privado:

/**
 * @brief Determina si el track cruzó el segmento (x1,y1)→(x2,y2) entre
 *        su posición anterior y su posición actual.
 *
 * Usa el signo del producto vectorial para determinar el lado.
 *
 * @return  1 si cruzó de izquierda a derecha (según orientación del segmento)
 *         -1 si cruzó de derecha a izquierda
 *          0 si no hubo cruce
 */
static int checkSegmentCrossing(
    float prev_x, float prev_y,   // Posición anterior del track
    float curr_x, float curr_y,   // Posición actual del track
    float sx1, float sy1,          // Punto inicio del segmento
    float sx2, float sy2           // Punto fin del segmento
);
```

```cpp
// En tracklet_fsm.cpp:

int TrackletFSM::checkSegmentCrossing(
    float prev_x, float prev_y,
    float curr_x, float curr_y,
    float sx1, float sy1,
    float sx2, float sy2)
{
    // Vector del segmento
    float sdx = sx2 - sx1;
    float sdy = sy2 - sy1;

    // Producto vectorial: segmento × (punto - origen_segmento)
    // Signo positivo = izquierda del segmento, negativo = derecha
    float side_prev = sdx * (prev_y - sy1) - sdy * (prev_x - sx1);
    float side_curr = sdx * (curr_y - sy1) - sdy * (curr_x - sx1);

    // Cruce ocurre cuando el signo cambia
    if (side_prev > 0.0f && side_curr <= 0.0f) return  1;  // Izq→Der
    if (side_prev < 0.0f && side_curr >= 0.0f) return -1;  // Der→Izq

    // Verificar que el punto de cruce esté dentro del segmento
    // (no fuera de sus extremos)
    // Esto evita falsos positivos en extensiones del segmento.
    // Simplificación: si el track está dentro del bounding box del segmento,
    // el cruce es válido.
    float min_x = sx1 < sx2 ? sx1 : sx2;
    float max_x = sx1 > sx2 ? sx1 : sx2;
    float min_y = sy1 < sy2 ? sy1 : sy2;
    float max_y = sy1 > sy2 ? sy1 : sy2;

    // Extender ligeramente para píxeles en el borde del segmento
    const float MARGIN = 0.5f;
    if (curr_x < min_x - MARGIN || curr_x > max_x + MARGIN) return 0;
    if (curr_y < min_y - MARGIN || curr_y > max_y + MARGIN) return 0;

    return 0;
}
```

### 2.4 Actualizar TrackletFSM::update() para soportar segmentos

```cpp
// En TrackletFSM::update(), al procesar cada track confirmado:

// Si está usando segmentos (nuevo modo):
if (ThermalConfig::door_lines.use_segments && ThermalConfig::door_lines.num_lines > 0) {
    // Obtener posición anterior del historial
    if (t.history.count >= 2) {
        int prev_idx = (t.history.head - 1 + TrackHistory::CAPACITY) % TrackHistory::CAPACITY;
        float prev_x = t.history.entries[prev_idx].x;
        float prev_y = t.history.entries[prev_idx].y;

        for (int li = 0; li < ThermalConfig::door_lines.num_lines; li++) {
            const CountingSegment& seg = ThermalConfig::door_lines.lines[li];
            if (!seg.enabled) continue;

            int cross = checkSegmentCrossing(
                prev_x, prev_y, t.x(), t.y(),
                seg.x1, seg.y1, seg.x2, seg.y2
            );

            if (cross == 1) {
                countOut++;
                ESP_LOGI(TAG, "Track ID=%d crossed line '%s' → +1 OUT", t.id, seg.name);
            } else if (cross == -1) {
                countIn++;
                ESP_LOGI(TAG, "Track ID=%d crossed line '%s' → +1 IN", t.id, seg.name);
            }
        }
    }
} else {
    // Modo legacy: usar lineEntryY / lineExitY horizontal
    // ... lógica anterior de zonas ...
}
```

---

## 3. Cambios en el Frontend (UI)

### 3.1 Modo de edición de líneas en el canvas

Añadir un modo de edición donde el usuario puede dibujar segmentos haciendo clic en el canvas:

```javascript
// Estado del editor de líneas
const lineEditor = {
    active: false,
    drawing: false,
    lines: [],          // Array de {x1,y1,x2,y2,id,name,color}
    currentLine: null,  // Línea en proceso de dibujo
    maxLines: 4
};

// Activar modo edición
function toggleLineEditor() {
    lineEditor.active = !lineEditor.active;
    const btn = document.getElementById('btn-edit-lines');
    btn.textContent = lineEditor.active ? '✓ DONE EDITING' : '✏ EDIT LINES';
    btn.classList.toggle('active', lineEditor.active);
    
    if (lineEditor.active) {
        displayCanvas.style.cursor = 'crosshair';
        displayCanvas.addEventListener('mousedown', onLineStart);
        displayCanvas.addEventListener('mousemove', onLinePreview);
        displayCanvas.addEventListener('mouseup', onLineEnd);
        // Touch support
        displayCanvas.addEventListener('touchstart', onLineTouchStart, { passive: false });
        displayCanvas.addEventListener('touchend', onLineTouchEnd, { passive: false });
    } else {
        displayCanvas.style.cursor = '';
        displayCanvas.removeEventListener('mousedown', onLineStart);
        displayCanvas.removeEventListener('mousemove', onLinePreview);
        displayCanvas.removeEventListener('mouseup', onLineEnd);
        displayCanvas.removeEventListener('touchstart', onLineTouchStart);
        displayCanvas.removeEventListener('touchend', onLineTouchEnd);
        sendLines();  // Enviar al ESP32 al salir del modo edición
    }
}

// Convertir coordenadas de canvas a coordenadas sensor
function canvasToSensor(canvasX, canvasY) {
    const rect = displayCanvas.getBoundingClientRect();
    const scaleX = SENSOR_COLS / displayCanvas.width;
    const scaleY = SENSOR_ROWS / displayCanvas.height;
    return {
        x: (canvasX - rect.left) * scaleX * (displayCanvas.width / rect.width),
        y: (canvasY - rect.top)  * scaleY * (displayCanvas.height / rect.height)
    };
}

function onLineStart(e) {
    if (!lineEditor.active) return;
    if (lineEditor.lines.length >= lineEditor.maxLines) {
        showToast('Máximo 4 líneas. Elimina una primero.');
        return;
    }
    const pos = canvasToSensor(e.clientX, e.clientY);
    lineEditor.drawing = true;
    lineEditor.currentLine = {
        x1: pos.x, y1: pos.y, x2: pos.x, y2: pos.y,
        id: Date.now(),
        name: `Línea ${lineEditor.lines.length + 1}`,
        color: ['#00ff88','#00d4ff','#ffb300','#ff3d5a'][lineEditor.lines.length % 4]
    };
}

function onLinePreview(e) {
    if (!lineEditor.drawing || !lineEditor.currentLine) return;
    const pos = canvasToSensor(e.clientX, e.clientY);
    lineEditor.currentLine.x2 = pos.x;
    lineEditor.currentLine.y2 = pos.y;
    // El renderizado de la línea en preview ocurre en el siguiente renderLoop()
}

function onLineEnd(e) {
    if (!lineEditor.drawing || !lineEditor.currentLine) return;
    lineEditor.drawing = false;
    const len = Math.hypot(
        lineEditor.currentLine.x2 - lineEditor.currentLine.x1,
        lineEditor.currentLine.y2 - lineEditor.currentLine.y1
    );
    if (len > 1.0) {
        lineEditor.lines.push({...lineEditor.currentLine});
        updateLineList();
    }
    lineEditor.currentLine = null;
}

// Análogos para touch (llamar a los de mouse mapeando touches[0])
function onLineTouchStart(e) { e.preventDefault(); onLineStart(e.touches[0]); }
function onLineTouchEnd(e)   { e.preventDefault(); onLineEnd(e.changedTouches[0]); }
```

### 3.2 Renderizar líneas en el canvas

Añadir llamada en el renderLoop:

```javascript
function renderUserLines() {
    if (!state.showLines) return;
    
    ctx.save();
    
    // Renderizar líneas guardadas
    lineEditor.lines.forEach(line => {
        const px1 = line.x1 * SCALE_X;
        const py1 = line.y1 * SCALE_Y;
        const px2 = line.x2 * SCALE_X;
        const py2 = line.y2 * SCALE_Y;
        
        ctx.strokeStyle = line.color;
        ctx.lineWidth   = 2.5;
        ctx.setLineDash([10, 5]);
        ctx.shadowColor = line.color;
        ctx.shadowBlur  = 5;
        ctx.beginPath();
        ctx.moveTo(px1, py1);
        ctx.lineTo(px2, py2);
        ctx.stroke();
        
        // Flechas en los extremos para indicar orientación
        const angle = Math.atan2(py2 - py1, px2 - px1);
        ctx.setLineDash([]);
        ctx.beginPath();
        ctx.moveTo(px2, py2);
        ctx.lineTo(px2 - 12 * Math.cos(angle - 0.4), py2 - 12 * Math.sin(angle - 0.4));
        ctx.lineTo(px2 - 12 * Math.cos(angle + 0.4), py2 - 12 * Math.sin(angle + 0.4));
        ctx.closePath();
        ctx.fillStyle = line.color;
        ctx.fill();
        
        // Label
        ctx.shadowBlur = 0;
        ctx.fillStyle  = line.color;
        ctx.font       = 'bold 11px JetBrains Mono';
        ctx.fillText(line.name, (px1 + px2) / 2 + 5, (py1 + py2) / 2 - 5);
    });
    
    // Línea en preview (mientras se dibuja)
    if (lineEditor.drawing && lineEditor.currentLine) {
        const l = lineEditor.currentLine;
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth   = 1.5;
        ctx.setLineDash([5, 5]);
        ctx.globalAlpha = 0.7;
        ctx.beginPath();
        ctx.moveTo(l.x1 * SCALE_X, l.y1 * SCALE_Y);
        ctx.lineTo(l.x2 * SCALE_X, l.y2 * SCALE_Y);
        ctx.stroke();
        ctx.globalAlpha = 1.0;
    }
    
    ctx.restore();
}

// Llamar renderUserLines() en renderLoop() después de renderCountingLines() si use_segments está activo:
if (lineEditor.lines.length > 0) {
    renderUserLines();
} else {
    renderCountingLines(state.lineEntry, state.lineExit);  // Legacy horizontal
}
```

### 3.3 Panel de gestión de líneas

```html
<!-- Añadir al panel de calibración, después de los sliders de líneas Y: -->
<hr class="divider">
<div class="calib-section">
    <div class="calib-section-title">Líneas de Conteo Personalizadas</div>
    <button class="btn btn-primary" id="btn-edit-lines" onclick="toggleLineEditor()">
        ✏ EDITAR LÍNEAS
    </button>
    <div id="lines-list" style="margin-top: 8px; font-size: 0.7rem;">
        <!-- Poblado por updateLineList() -->
    </div>
    <button class="btn btn-dim" onclick="clearAllLines()" style="margin-top: 6px;">
        🗑 BORRAR TODAS
    </button>
</div>
```

```javascript
function updateLineList() {
    const container = document.getElementById('lines-list');
    container.innerHTML = lineEditor.lines.map((l, i) => `
        <div style="display:flex;align-items:center;gap:6px;margin:4px 0">
            <span style="width:10px;height:10px;border-radius:2px;background:${l.color};flex-shrink:0"></span>
            <span style="flex:1;color:var(--text-primary)">${l.name}</span>
            <button onclick="removeLine(${i})" style="background:transparent;border:none;color:var(--neon-red);cursor:pointer">✕</button>
        </div>
    `).join('') || '<span style="color:var(--text-muted)">Sin líneas — dibuja en el canvas</span>';
}

function removeLine(index) {
    lineEditor.lines.splice(index, 1);
    updateLineList();
    // Re-numerar nombres
    lineEditor.lines.forEach((l, i) => l.name = `Línea ${i+1}`);
}

function clearAllLines() {
    lineEditor.lines = [];
    updateLineList();
    sendLines();
}

function sendLines() {
    // Enviar líneas al ESP32 como comando WebSocket
    sendWs({
        cmd: 'SET_COUNTING_LINES',
        lines: lineEditor.lines.map(l => ({
            x1: l.x1, y1: l.y1, x2: l.x2, y2: l.y2,
            id: l.id, name: l.name
        }))
    });
}
```

### 3.4 Handler en http_server.cpp

```cpp
// En handleWebSocketMessage(), añadir caso:

else if (strcmp(cmd->valuestring, "SET_COUNTING_LINES") == 0) {
    cJSON* lines_arr = cJSON_GetObjectItem(root, "lines");
    if (cJSON_IsArray(lines_arr)) {
        int n = cJSON_GetArraySize(lines_arr);
        n = n > MAX_COUNTING_LINES ? MAX_COUNTING_LINES : n;
        
        ThermalConfig::door_lines.num_lines = 0;
        
        for (int i = 0; i < n; i++) {
            cJSON* line = cJSON_GetArrayItem(lines_arr, i);
            if (!cJSON_IsObject(line)) continue;
            
            CountingSegment& seg = ThermalConfig::door_lines.lines[i];
            cJSON* x1 = cJSON_GetObjectItem(line, "x1");
            cJSON* y1 = cJSON_GetObjectItem(line, "y1");
            cJSON* x2 = cJSON_GetObjectItem(line, "x2");
            cJSON* y2 = cJSON_GetObjectItem(line, "y2");
            
            if (cJSON_IsNumber(x1) && cJSON_IsNumber(y1) &&
                cJSON_IsNumber(x2) && cJSON_IsNumber(y2)) {
                seg.x1 = (float)x1->valuedouble;
                seg.y1 = (float)y1->valuedouble;
                seg.x2 = (float)x2->valuedouble;
                seg.y2 = (float)y2->valuedouble;
                seg.enabled = true;
                seg.id = i + 1;
                snprintf(seg.name, sizeof(seg.name), "Linea %d", i + 1);
                ThermalConfig::door_lines.num_lines++;
            }
        }
        
        // Activar modo segmentos si hay líneas
        ThermalConfig::door_lines.use_segments = (ThermalConfig::door_lines.num_lines > 0);
        
        // Propagar al pipeline vía configQueue
        if (s_configQueue) {
            AppConfigCmd cmd;
            cmd.type  = ConfigCmdType::APPLY_CONFIG;
            cmd.value = 0;
            xQueueSend(s_configQueue, &cmd, 0);
        }
        
        ESP_LOGI(TAG, "Counting lines updated: %d segments, use_segments=%d",
                 ThermalConfig::door_lines.num_lines,
                 ThermalConfig::door_lines.use_segments);
    }
}
```

---

## 4. Persistencia en NVS (mínima para esta etapa)

Las líneas deben sobrevivir a un reinicio. Guardar como JSON compacto en NVS:

```cpp
// En saveConfigToNvs() — añadir:
if (ThermalConfig::door_lines.num_lines > 0) {
    cJSON* lines = cJSON_CreateArray();
    for (int i = 0; i < ThermalConfig::door_lines.num_lines; i++) {
        const CountingSegment& s = ThermalConfig::door_lines.lines[i];
        cJSON* l = cJSON_CreateObject();
        cJSON_AddNumberToObject(l, "x1", s.x1);
        cJSON_AddNumberToObject(l, "y1", s.y1);
        cJSON_AddNumberToObject(l, "x2", s.x2);
        cJSON_AddNumberToObject(l, "y2", s.y2);
        cJSON_AddItemToArray(lines, l);
    }
    char* lines_str = cJSON_PrintUnformatted(lines);
    nvs_set_str(h, "seg_lines", lines_str);
    free(lines_str);
    cJSON_Delete(lines);
    nvs_set_i8(h, "use_segments", ThermalConfig::door_lines.use_segments ? 1 : 0);
}

// En loadConfigFromNvs() — añadir:
size_t lines_len = 512;
char lines_buf[512];
if (nvs_get_str(h, "seg_lines", lines_buf, &lines_len) == ESP_OK) {
    // Parsear y aplicar...
    // (código análogo al handler de SET_COUNTING_LINES)
}
```

---

## 5. Checklist

- [ ] `CountingSegment` struct añadido a `thermal_types.hpp`
- [ ] `DoorLineConfig` y `door_lines` añadidos a `thermal_config.hpp`
- [ ] `checkSegmentCrossing()` implementado en `tracklet_fsm.cpp`
- [ ] `TrackletFSM::update()` usa segmentos cuando `use_segments == true`
- [ ] Handler `SET_COUNTING_LINES` añadido en `http_server.cpp`
- [ ] `lineEditor` y funciones de dibujo implementadas en la UI
- [ ] `renderUserLines()` renderiza líneas con orientación (flechas)
- [ ] Panel de gestión de líneas visible en el calibration panel
- [ ] Líneas persisten en NVS (sobreviven reinicio)
- [ ] `GET_CONFIG` devuelve también las líneas guardadas
- [ ] Sistema legacy (Y horizontal) sigue funcionando cuando `use_segments == false`
- [ ] Prueba: dibujar línea diagonal, cruzar con mano, verificar conteo
- [ ] `idf.py build` pasa sin errores
