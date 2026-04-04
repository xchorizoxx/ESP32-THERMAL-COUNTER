# Etapa A4 — UI: Escala de Color, Visualización de Zonas
## Mejoras al HUD sin hardware adicional

**Dependencias:** Etapa A3 completada  
**Hardware requerido:** Ninguno adicional  
**Tiempo estimado:** 1 día  
**Archivo principal:** `components/web_server/src/web_ui_html.h`

> ⚠️ **Restricción:** La UI está embebida como string C en `web_ui_html.h`. Todo el JS/CSS/HTML debe estar en un único archivo. No hay sistema de archivos para servir múltiples archivos hasta la Fase D.

---

## 1. Cambio 1: Escala de Color Fija 0–50°C

### 1.1 Problema actual

La escala de color se adapta automáticamente al rango de temperaturas del frame (`dispMin`, `dispMax`). Esto hace que en modo Normal la escala cambie constantemente, haciendo imposible comparar frames visualmente.

### 1.2 Solución

Reemplazar la normalización dinámica por una fija en 0–50°C. Temperaturas fuera de rango se clipean al color extremo.

**Buscar en `web_ui_html.h` la función `processFrame` y la sección de colorización:**

```javascript
// ELIMINAR toda la lógica de dispMin/dispMax/smoothMin/smoothMax:
// let minTemp = 9999.0;
// let maxTemp = -9999.0;
// ...
// state.smoothMin = state.smoothMin * 0.8 + dispMin * 0.2;
// ...

// REEMPLAZAR por escala fija:
const TEMP_MIN = 0.0;
const TEMP_MAX = 50.0;
const TEMP_RANGE = TEMP_MAX - TEMP_MIN;  // 50.0

// En el bucle de colorización:
for (let i = 0; i < MIN_PIXELS; i++) {
    // Clampear estrictamente a [0, 50]
    const t = temps[i];
    const clamped = t < TEMP_MIN ? TEMP_MIN : (t > TEMP_MAX ? TEMP_MAX : t);
    const norm = (clamped - TEMP_MIN) / TEMP_RANGE;  // [0.0, 1.0]
    const lutIdx = Math.floor(norm * 255) * 3;
    const px = i * 4;
    d[px]     = INFERNO_LUT[lutIdx];
    d[px + 1] = INFERNO_LUT[lutIdx + 1];
    d[px + 2] = INFERNO_LUT[lutIdx + 2];
    d[px + 3] = 255;
}
```

### 1.3 Actualizar info bar

```javascript
// Cambiar el info bar de "RANGO: X/Y°C" a:
elInfoRange.innerHTML = `Ta: <span style="color:var(--neon-amber)">${frame.ambientTemp.toFixed(1)}°C</span> | ESCALA: 0°C – 50°C`;
```

### 1.4 Agregar leyenda de escala al canvas

Añadir un elemento visual que muestre la escala de colores fija debajo del canvas:

```html
<!-- Añadir después del canvas-frame en index.html: -->
<div class="temp-scale-bar">
  <span class="scale-label">0°C</span>
  <canvas id="scale-canvas" width="400" height="12"></canvas>
  <span class="scale-label">50°C</span>
</div>
```

```javascript
// Dibujar la escala al cargar:
function drawTempScale() {
    const canvas = document.getElementById('scale-canvas');
    const ctx = canvas.getContext('2d');
    const imgData = ctx.createImageData(400, 12);
    
    for (let x = 0; x < 400; x++) {
        const norm = x / 399;
        const lutIdx = Math.floor(norm * 255) * 3;
        for (let y = 0; y < 12; y++) {
            const px = (y * 400 + x) * 4;
            imgData.data[px]     = INFERNO_LUT[lutIdx];
            imgData.data[px + 1] = INFERNO_LUT[lutIdx + 1];
            imgData.data[px + 2] = INFERNO_LUT[lutIdx + 2];
            imgData.data[px + 3] = 255;
        }
    }
    ctx.putImageData(imgData, 0, 0);
}
// Llamar en el boot después de que INFERNO_LUT esté lista:
drawTempScale();
```

---

## 2. Cambio 2: Visualización de Zonas de Puerta en el Canvas

### 2.1 Modificar `renderCountingLines()` para mostrar zonas

```javascript
// Reemplazar renderCountingLines() en su totalidad:

function renderCountingLines(entryY, exitY) {
    ctx.save();
    
    const scaleY = CANVAS_H / SENSOR_ROWS;  // = 480/24 = 20
    
    // === Zona Norte (arriba de la línea de entrada) ===
    // Color verde semitransparente
    ctx.fillStyle = 'rgba(0, 255, 136, 0.08)';
    ctx.fillRect(0, 0, CANVAS_W, entryY * scaleY);
    
    // === Zona Neutra (entre las dos líneas) ===
    // Color blanco muy semitransparente
    ctx.fillStyle = 'rgba(255, 255, 255, 0.05)';
    ctx.fillRect(0, entryY * scaleY, CANVAS_W, (exitY - entryY) * scaleY);
    
    // === Zona Sur (debajo de la línea de salida) ===
    // Color cyan semitransparente
    ctx.fillStyle = 'rgba(0, 212, 255, 0.08)';
    ctx.fillRect(0, exitY * scaleY, CANVAS_W, CANVAS_H - exitY * scaleY);
    
    // === Líneas punteadas ===
    ctx.setLineDash([10, 6]);
    ctx.lineWidth = 2;
    
    // Línea de entrada (verde)
    const pyEntry = entryY * scaleY;
    ctx.strokeStyle = 'rgba(0, 255, 136, 0.90)';
    ctx.shadowColor = '#00ff88';
    ctx.shadowBlur  = 6;
    ctx.beginPath();
    ctx.moveTo(0, pyEntry);
    ctx.lineTo(CANVAS_W, pyEntry);
    ctx.stroke();
    
    // Etiqueta zona norte
    ctx.shadowBlur  = 0;
    ctx.fillStyle   = 'rgba(0, 255, 136, 0.8)';
    ctx.font        = '600 11px JetBrains Mono, monospace';
    ctx.fillText('NORTE / ENTRY', 6, pyEntry - 5);
    
    // Línea de salida (cyan)
    const pyExit = exitY * scaleY;
    ctx.strokeStyle = 'rgba(0, 212, 255, 0.90)';
    ctx.shadowColor = '#00d4ff';
    ctx.shadowBlur  = 6;
    ctx.beginPath();
    ctx.moveTo(0, pyExit);
    ctx.lineTo(CANVAS_W, pyExit);
    ctx.stroke();
    
    ctx.shadowBlur  = 0;
    ctx.fillStyle   = 'rgba(0, 212, 255, 0.8)';
    ctx.fillText('SUR / EXIT', 6, pyExit - 5);
    
    // Etiqueta zona neutra (entre las líneas)
    ctx.fillStyle   = 'rgba(255, 255, 255, 0.5)';
    const neutralMid = (entryY + exitY) / 2 * scaleY;
    ctx.fillText('ZONA NEUTRA', 6, neutralMid + 4);
    
    ctx.restore();
}
```

### 2.2 Indicador de estado de track en el HUD

Modificar `renderTracks()` para colorear tracks según su zona:

```javascript
function renderTracks(tracks) {
    if (!tracks.length) return;
    ctx.save();
    
    tracks.forEach(t => {
        const px = t.x * SCALE_X;
        const py = t.y * SCALE_Y;
        
        // Color según estado de zona (t.zone_state viene en el packet)
        let trackColor;
        switch(t.zone_state) {
            case 1: trackColor = '#00ff88'; break;  // Norte — verde
            case 2: trackColor = '#ffb300'; break;  // Neutra — ámbar
            case 3: trackColor = '#00d4ff'; break;  // Sur — cyan
            case 4: trackColor = '#ff3d5a'; break;  // Ghost — rojo (debería ser raro)
            default: trackColor = '#ffffff';
        }
        
        // Círculo de posición
        ctx.shadowColor = trackColor;
        ctx.shadowBlur  = 10;
        ctx.strokeStyle = trackColor;
        ctx.lineWidth   = 1.5;
        const BOX_HALF  = Math.round(1.5 * SCALE_X);
        ctx.strokeRect(px - BOX_HALF, py - BOX_HALF, BOX_HALF * 2, BOX_HALF * 2);
        
        // Punto central
        ctx.fillStyle = trackColor;
        ctx.beginPath();
        ctx.arc(px, py, 3, 0, Math.PI * 2);
        ctx.fill();
        
        // Vector de velocidad
        const vlen = Math.sqrt(t.vx*t.vx + t.vy*t.vy);
        if (vlen > 0.05) {
            const vxpx = t.vx * SCALE_X * 5;
            const vypy = t.vy * SCALE_Y * 5;
            ctx.strokeStyle = '#ffb300';
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            ctx.moveTo(px, py);
            ctx.lineTo(px + vxpx, py + vypy);
            ctx.stroke();
            
            const angle = Math.atan2(vypy, vxpx);
            ctx.fillStyle = '#ffb300';
            ctx.beginPath();
            ctx.moveTo(px + vxpx, py + vypy);
            ctx.lineTo(px + vxpx - 5 * Math.cos(angle - Math.PI/6), py + vypy - 5 * Math.sin(angle - Math.PI/6));
            ctx.lineTo(px + vxpx - 5 * Math.cos(angle + Math.PI/6), py + vypy - 5 * Math.sin(angle + Math.PI/6));
            ctx.fill();
        }
        
        // Label ID + zona
        ctx.shadowBlur = 0;
        ctx.fillStyle  = 'rgba(0,0,0,0.7)';
        ctx.fillRect(px - BOX_HALF, py - BOX_HALF - 16, 56, 14);
        ctx.fillStyle  = trackColor;
        ctx.font       = 'bold 10px JetBrains Mono, monospace';
        const zoneLabel = ['?','N','C','S','!'][t.zone_state] || '?';
        ctx.fillText(`ID:${t.id}[${zoneLabel}]`, px - BOX_HALF + 2, py - BOX_HALF - 5);
    });
    
    ctx.restore();
}
```

### 2.3 Añadir zone_state al decoder del protocolo WebSocket

En `processFrame()`, el packet actual no incluye `zone_state` por track. Hay dos opciones:

**Opción A (simple):** Inferir la zona en el cliente basándose en `y` y los valores actuales de `lineEntry`/`lineExit`.

```javascript
// En processFrame(), al crear los tracks:
const tracks = [];
for (let i = 0; i < numTracks; i++) {
    // ... parsear x, y, vx, vy como antes ...
    
    // Inferir zona en el cliente
    let zone_state;
    if (y < state.lineEntry) zone_state = 1;       // Norte
    else if (y > state.lineExit) zone_state = 3;    // Sur
    else zone_state = 2;                            // Neutra
    
    tracks.push({ id, x, y, vx, vy, zone_state });
}
```

**Opción B (correcto):** Incluir `zone_state` en el protocolo WebSocket. Requiere modificar `http_server.cpp:broadcastFrame()` para añadir el byte. Se recomienda Opción B para exactitud, pero Opción A es suficiente para esta etapa y no requiere cambiar el protocolo binary.

**Para esta etapa: usar Opción A.** La Opción B se puede incorporar en la Etapa B1 junto con los cambios de protocolo para líneas manuales.

---

## 3. Cambio 3: Corrección del Protocolo WebSocket (header 0x11 vs 0xAA)

La documentación interna (`docs/algorithm/04_system_and_network.md`) dice header `0xAA` pero el código usa `0x11`. Corregir la documentación, no el código — el código ya funciona con los clientes existentes.

```markdown
<!-- En docs/algorithm/04_system_and_network.md — ACTUALIZAR la tabla: -->
| 0 | `uint8` | Header (`0x11`) |  ← Era 0xAA en la doc, el código usa 0x11
```

---

## 4. Checklist

- [ ] Escala de color fija 0–50°C implementada en `processFrame()`
- [ ] Eliminada la lógica de `smoothMin`/`smoothMax` dinámica
- [ ] Info bar muestra "ESCALA: 0°C – 50°C" en lugar del rango dinámico
- [ ] Canvas de escala de colores dibujado al cargar la página
- [ ] `renderCountingLines()` muestra zonas Norte/Neutra/Sur coloreadas
- [ ] `renderTracks()` colorea tracks según zona (verde/ámbar/cyan)
- [ ] Label de track muestra `ID:N[Z]` donde Z es la inicial de la zona
- [ ] Zona_state inferida en cliente desde posición Y
- [ ] Documentación de protocolo actualizada (0x11 no 0xAA)
- [ ] UI probada en Chrome/Firefox en móvil y desktop
- [ ] Sin regresiones en la funcionalidad existente
