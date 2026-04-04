# FASE 2 — Rediseño del HUD Web

## Identidad y alcance

Agente de edición de UI. Esta fase refactoriza el HUD web completo para:
1. Ocultar sliders de Norte/Sur cuando `use_segments=true`
2. Suavizar/eliminar las líneas punteadas de zonas muertas en el canvas (menos agresivas)
3. Añadir trail de trayectoria por track (toggleable)
4. Reorganizar Settings en grupos lógicos claros
5. Añadir configuración de tracking editable desde la UI web

**Archivos objetivo (únicos):**
1. `components/web_server/src/web/index.html`
2. `components/web_server/src/web/style.css`
3. `components/web_server/src/web/app.js`

---

## Cambio 2-A — Ocultar sliders legacy Norte/Sur cuando use_segments=true (`app.js`)

### Problema
Los sliders de `line_entry` y `line_exit` (fila Norte/Sur) pierden sentido cuando
el usuario tiene líneas de segmento activas. Deben ocultarse dinámicamente.

### BUSCAR (en la función `updateConfigUI`, bloque exacto):
```
    CONFIG.use_segments = obj.use_segments || false;
```

### REEMPLAZAR POR:
```
    CONFIG.use_segments = obj.use_segments || false;
    // Mostrar/ocultar sliders de modo legacy según el modo activo
    const legacyFields = ['line_entry', 'line_exit'];
    legacyFields.forEach(fid => {
        const group = document.getElementById(`cfg-${fid}`)?.closest('.form-group');
        if (group) group.style.display = CONFIG.use_segments ? 'none' : '';
    });
```

### BUSCAR (en `clearAllLines`, bloque exacto):
```
    CONFIG.use_segments = false;
    updateLineList();
```

### REEMPLAZAR POR:
```
    CONFIG.use_segments = false;
    updateLineList();
    // Restaurar sliders legacy al limpiar líneas
    ['line_entry', 'line_exit'].forEach(fid => {
        const group = document.getElementById(`cfg-${fid}`)?.closest('.form-group');
        if (group) group.style.display = '';
    });
```

---

## Cambio 2-B — Trail de trayectoria toggleable (`app.js`)

### Añadir variable global de estado (después de la línea `let isEditingLines = false;`)

### BUSCAR:
```
let isEditingLines = false;
let currentLine = null;
```

### REEMPLAZAR POR:
```
let isEditingLines = false;
let currentLine = null;
let showTrajectoryTrail = false; // Toggle de trail de trayectoria por track
```

---

### Añadir función `renderTrajectoryTrails` antes de `renderTracks`

### BUSCAR (primera línea de `renderTracks`):
```
function renderTracks(tracks) {
```

### REEMPLAZAR POR:
```
function renderTrajectoryTrails(trackletData) {
    // Dibuja la trayectoria histórica de cada track como línea con opacidad decreciente.
    // trackletData: array de objetos con { id, trailPoints: [{x,y},...] } enviados desde firmware.
    // En esta versión, el trail se construye en el cliente guardando hasta 15 posiciones por ID.
    if (!showTrajectoryTrail) return;
    if (!window.trackTrails) window.trackTrails = {};

    trackletData.forEach(t => {
        if (!window.trackTrails[t.id]) window.trackTrails[t.id] = [];
        const trail = window.trackTrails[t.id];
        trail.push({ x: t.x, y: t.y });
        if (trail.length > 15) trail.shift(); // Máximo 15 puntos

        if (trail.length < 2) return;

        uiCtx.save();
        for (let i = 1; i < trail.length; i++) {
            const alpha = i / trail.length; // 0=transparente (antiguo), 1=sólido (más reciente)
            uiCtx.globalAlpha = alpha * 0.7;
            uiCtx.strokeStyle = '#ffb300'; // Ámbar — mismo color que zona neutral
            uiCtx.lineWidth = 1.5;
            uiCtx.beginPath();
            uiCtx.moveTo(trail[i-1].x * SCALE_X, trail[i-1].y * SCALE_Y);
            uiCtx.lineTo(trail[i].x * SCALE_X, trail[i].y * SCALE_Y);
            uiCtx.stroke();
        }
        uiCtx.restore();
    });

    // Limpiar trails de tracks que ya no están activos
    const activeIds = new Set(trackletData.map(t => t.id));
    Object.keys(window.trackTrails).forEach(id => {
        if (!activeIds.has(parseInt(id))) delete window.trackTrails[id];
    });
}

function renderTracks(tracks) {
```

---

### Llamar `renderTrajectoryTrails` antes de `renderTracks` en el dispatch

### BUSCAR:
```
    renderUserLines();
    renderTracks(tracks);
```

### REEMPLAZAR POR:
```
    renderUserLines();
    renderTrajectoryTrails(tracks);
    renderTracks(tracks);
```

---

## Cambio 2-C — Suavizar visualmente las zonas muertas (`app.js`)

Las líneas punteadas laterales son visualmente agresivas. Se deja solo el relleno
translúcido (sin las líneas discontinuas) en `renderDeadZones`.

### BUSCAR (bloque completo de `renderDeadZones`):
```
function renderDeadZones(dL, dR) {
    // Muestra unicamente las columnas de exclusion lateral.
    // Llamada en modo segmentos, donde renderCountingZones no se ejecuta.
    uiCtx.save();
    uiCtx.fillStyle = 'rgba(255, 0, 0, 0.15)';
    if (dL > 0)  uiCtx.fillRect(0, 0, dL * SCALE_X, 240);
    if (dR < 31) uiCtx.fillRect(dR * SCALE_X, 0, 320 - (dR * SCALE_X), 240);
    uiCtx.setLineDash([4, 4]);
    uiCtx.strokeStyle = 'rgba(255, 50, 50, 0.6)';
    uiCtx.lineWidth = 1;
    if (dL > 0) {
        uiCtx.beginPath();
        uiCtx.moveTo(dL * SCALE_X, 0);
        uiCtx.lineTo(dL * SCALE_X, 240);
        uiCtx.stroke();
    }
    if (dR < 31) {
        uiCtx.beginPath();
        uiCtx.moveTo(dR * SCALE_X, 0);
        uiCtx.lineTo(dR * SCALE_X, 240);
        uiCtx.stroke();
    }
    uiCtx.restore();
}
```

### REEMPLAZAR POR:
```
function renderDeadZones(dL, dR) {
    // Columnas de exclusion lateral: solo relleno suave, sin líneas discontinuas.
    if (dL <= 0 && dR >= 31) return; // Nada que dibujar
    uiCtx.save();
    uiCtx.fillStyle = 'rgba(255, 0, 0, 0.10)';
    if (dL > 0)  uiCtx.fillRect(0, 0, dL * SCALE_X, 240);
    if (dR < 31) uiCtx.fillRect(dR * SCALE_X, 0, 320 - (dR * SCALE_X), 240);
    uiCtx.restore();
}
```

---

## Cambio 2-D — Mismo suavizado en `renderCountingZones` (`app.js`)

La función de modo legacy también usa las líneas discontinuas verticales de zonas muertas.
Reemplazar el bloque de líneas discontinuas por el mismo relleno suave.

### BUSCAR (el bloque de límites verticales dentro de `renderCountingZones`):
```
    // Límites Verticales (Líneas)
    uiCtx.setLineDash([4, 4]);
    uiCtx.strokeStyle = 'rgba(255, 50, 50, 0.6)';
    uiCtx.lineWidth = 1;
    if (dL > 0) {
        uiCtx.beginPath(); uiCtx.moveTo(dL * SCALE_X, 0); uiCtx.lineTo(dL * SCALE_X, 240); uiCtx.stroke();
    }
    if (dR < 31) {
        uiCtx.beginPath(); uiCtx.moveTo(dR * SCALE_X, 0); uiCtx.lineTo(dR * SCALE_X, 240); uiCtx.stroke();
    }
```

### REEMPLAZAR POR:
```
    // Límites Verticales: solo relleno suave (sin líneas discontinuas agresivas)
    // Las líneas discontinuas de conteo horizontal siguen intactas debajo.
```

---

## Cambio 2-E — Botón toggle de trail en `index.html`

### BUSCAR (el bloque de vision-controls en el Live Feed):
```
                    <div class="vision-controls">
                        <button class="btn-pill active" data-mode="normal" onclick="setVisionMode('normal')">Normal</button>
                        <button class="btn-pill" data-mode="raw" onclick="setVisionMode('raw')">Raw</button>
                        <button class="btn-pill" data-mode="diff" onclick="setVisionMode('diff')">Diferencial</button>
                    </div>
```

### REEMPLAZAR POR:
```
                    <div class="vision-controls">
                        <button class="btn-pill active" data-mode="normal" onclick="setVisionMode('normal')">Normal</button>
                        <button class="btn-pill" data-mode="raw" onclick="setVisionMode('raw')">Raw</button>
                        <button class="btn-pill" data-mode="diff" onclick="setVisionMode('diff')">Diferencial</button>
                        <button class="btn-pill" id="btn-trail" onclick="toggleTrail()">Trail</button>
                    </div>
```

---

### Añadir función `toggleTrail` en `app.js` (antes de `toggleLineEditor`):

### BUSCAR:
```
function toggleLineEditor(forceOff = false) {
```

### REEMPLAZAR POR:
```
function toggleTrail() {
    showTrajectoryTrail = !showTrajectoryTrail;
    const btn = document.getElementById('btn-trail');
    if (btn) {
        btn.classList.toggle('active', showTrajectoryTrail);
    }
    if (!showTrajectoryTrail && window.trackTrails) {
        window.trackTrails = {}; // Limpiar trails al desactivar
    }
}

function toggleLineEditor(forceOff = false) {
```

---

## Cambio 2-F — Reorganizar Settings en `index.html`

Los sliders de Norte/Sur se agrupan bajo un divisor colapsable `<details>` en el
panel de configuración básica, para que aparezcan como modo legacy opcional.

### BUSCAR (el bloque de sliders de entrada/salida dentro de `set-basic`):
```
                    <div class="form-group" style="padding-top:10px; border-top:1px solid rgba(255,255,255,0.1);">
                        <label>Línea de Entrada (Norte): Fila <span id="val-line_entry" class="val-badge">8</span></label>
                        <input type="range" id="cfg-line_entry" min="0" max="23" step="1" oninput="document.getElementById('val-line_entry').innerText=this.value">
                        <small>Fila Y donde comienza el conteo de ingreso (0-23).</small>
                    </div>
                    <div class="form-group">
                        <label>Línea de Salida (Sur): Fila <span id="val-line_exit" class="val-badge">16</span></label>
                        <input type="range" id="cfg-line_exit" min="0" max="23" step="1" oninput="document.getElementById('val-line_exit').innerText=this.value">
                        <small>Fila Y donde comienza el conteo de egreso (0-23).</small>
                    </div>
```

### REEMPLAZAR POR:
```
                    <details style="margin-top:10px; border-top:1px solid rgba(255,255,255,0.1); padding-top:10px;">
                        <summary style="cursor:pointer; font-size:0.85rem; color:var(--text-muted); margin-bottom:10px;">
                            ⚙ Modo Legacy — Líneas Horizontales Norte/Sur
                        </summary>
                        <div class="form-group">
                            <label>Línea de Entrada (Norte): Fila <span id="val-line_entry" class="val-badge">8</span></label>
                            <input type="range" id="cfg-line_entry" min="0" max="23" step="1" oninput="document.getElementById('val-line_entry').innerText=this.value">
                            <small>Solo aplica cuando NO hay líneas de segmento dibujadas.</small>
                        </div>
                        <div class="form-group">
                            <label>Línea de Salida (Sur): Fila <span id="val-line_exit" class="val-badge">16</span></label>
                            <input type="range" id="cfg-line_exit" min="0" max="23" step="1" oninput="document.getElementById('val-line_exit').innerText=this.value">
                            <small>Solo aplica cuando NO hay líneas de segmento dibujadas.</small>
                        </div>
                    </details>
```

---

## Verificación tras Fase 2

1. Al cargar con segmentos activos, el bloque `<details>` de Norte/Sur debe estar colapsado.
2. El botón "Trail" en Live Feed debe activar/desactivar las líneas de trayectoria.
3. Las zonas muertas en los bordes del canvas deben ser un relleno rojo suave sin líneas discontinuas.
4. Al borrar todas las líneas, el `<details>` de Norte/Sur vuelve a estar visible/expandible.
5. Notificar al usuario para compilar y flashear antes de pasar a Fase 3.
