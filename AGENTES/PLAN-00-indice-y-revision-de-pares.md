# PLAN MAESTRO — ESP32 Thermal Counter Enhanced
## Índice, Revisión de Pares y Hoja de Ruta

**Proyecto base:** Alpha 0.6 Tactical Debug  
**Framework:** ESP-IDF v6.0  
**Fecha:** Marzo 2026  
**Estado:** Planificación — listo para implementación iterativa por agente

---

## 1. Revisión Crítica del Plan Anterior (9 archivos)

El agente anterior generó un plan técnicamente ambicioso pero con varios problemas que **este plan corrige**:

### 1.1 Problemas Identificados en el Plan Anterior

| Archivo anterior | Problema crítico | Corrección en este plan |
|---|---|---|
| `02-etapa1-core-algorithm.md` | El `ChessCorrector` propuesto re-lee EEPROM en cada frame aplicación. La EEPROM del MLX90640 solo se lee una vez en init. Pasarla en cada llamada a `apply()` es incorrecto. | Etapa A1: corrector usa coeficientes pre-extraídos en init, no re-lee EEPROM |
| `02-etapa1-core-algorithm.md` | El `TrackletTracker` propuesto mezcla la lógica de conteo **dentro** del tracker. Viola el principio de responsabilidad única y hace imposible el testing. | Etapas A2/A3: tracker y FSM de puerta son clases separadas |
| `02-etapa1-core-algorithm.md` | El estado inicial del track usa `y < 12` hardcodeado ignorando las líneas configurables — el bug #5 de la code review. | Etapa A3: estado inicial usa `lineEntryY`/`lineExitY` configurables |
| `03-etapa2-door-config.md` | Las líneas de conteo son solo horizontales (`y_position`). El usuario pide líneas **manuales** dibujadas en la UI, que pueden ser diagonales o segmentos arbitrarios. | Etapa B1: líneas definidas como segmentos `(x1,y1)→(x2,y2)` |
| `03-etapa2-door-config.md` | `DoorsGlobalConfig` son `extern` globals sin mutex — el race condition crítico identificado en la review. | Etapa B1: ConfigManager con mutex por lectura/escritura |
| `04-etapa3-rtc-sd.md` | El driver RTC no inicializa I2C1 — asume que ya existe el bus. Pero en el proyecto actual solo existe I2C0 (MLX90640). | Etapa C1: init completo de I2C1 independiente |
| `05-etapa4-camera.md` | El pinout propuesto para OV2640 usa GPIO 8 (D4) que ya está ocupado por SDA del MLX90640. Colisión de pines no detectada. | Etapa D1: pinout revisado sin colisiones |
| `05-etapa4-camera.md` | La cámara se inicializa con `esp_camera_init()` dentro de una tarea de Core 1 que ya ejecuta el pipeline térmico. Ambos son CPU-intensivos en el mismo core. | Etapa D1: cámara en Core 0, pipeline en Core 1 |
| `06-etapa5-recording.md` | El buffer circular de grabación almacena `float pixels[768]` por frame = 3KB por frame térmico × 160 frames = 480 KB solo de datos crudos. No encaja en SRAM sin PSRAM. El plan menciona PSRAM pero no implementa `MALLOC_CAP_SPIRAM`. | Etapa E1: buffer explícitamente en PSRAM con `heap_caps_malloc` |
| `07-etapa6-webui.md` | La UI propuesta requiere cargar JS desde múltiples archivos (`api.js`, `websocket.js`, `thermal-canvas.js`...) pero la web está embebida en flash como string C. No hay sistema de archivos para servir múltiples archivos. | Etapa F1: UI en un único archivo HTML comprimido con gzip |
| `08-implementation-guide.md` | No menciona la necesidad de `fullclean` al agregar componentes nuevos a CMakeLists en IDF 6.0 — esto causa errores de build crípticos. | Todos los archivos de etapa incluyen instrucciones de build |

### 1.2 Omisiones Críticas del Plan Anterior

1. **No corrige los bugs conocidos** — el plan anterior asume código base correcto, pero hay 3 bugs críticos que deben corregirse antes de cualquier extensión.
2. **No define degradación graceful** — el sistema debe funcionar sin SD, sin RTC, sin cámara. El plan anterior hace dependencias duras entre etapas.
3. **No menciona IDF 6.0** — hay cambios en la API de I2C master, camera driver, y SDSPI que difieren de IDF 5.x. El plan anterior usa sintaxis de IDF 5.x.
4. **IPC Queue depth 15 = 25KB SRAM** — no se menciona reducirlo. Este plan lo fija en 4.
5. **Sin autenticación OTA** — el plan anterior no aborda el endpoint `/update` abierto.

---

## 2. Principios de Este Plan

### 2.1 Degradación Graceful (CRÍTICO)
Cada módulo hardware es **opcional**. El sistema debe arrancar y funcionar en modo básico sin SD, sin RTC, sin cámara. Los archivos de etapa indican explícitamente qué falla si el hardware no está presente y cómo el sistema lo maneja.

```
Modo mínimo (hardware actual):
  ✅ Pipeline térmico 16 Hz
  ✅ Detección y tracking mejorado
  ✅ Conteo IN/OUT
  ✅ WebSocket HUD
  ✅ Configuración de puertas desde UI
  ✅ NVS persistencia

Modo extendido (con SD + RTC):
  + Timestamping real
  + Clips térmicos

Modo completo (con SD + RTC + cámara):
  + Clips sincronizados térmico+VGA
  + Dataset para ML futuro
```

### 2.2 Orden de Implementación
**Un agente debe tomar UN archivo de etapa y completarlo antes de pasar al siguiente.**
Las etapas están numeradas con letras+números para mostrar dependencias:

```
FASE A — Bugs y algoritmo core (sin hardware nuevo)
  A1 — Corrección de bugs críticos + chess + noise filter
  A2 — Nuevo tracker (Tracklet con memoria)
  A3 — FSM de puerta infinita + validación de zonas
  A4 — UI: escala de color, líneas manuales, visualización de zonas

FASE B — Configuración avanzada de puertas
  B1 — Líneas de conteo como segmentos dibujables en UI
  B2 — Persistencia de config de puertas en NVS

FASE C — Módulo RTC DS3231
  C1 — Driver RTC + I2C1 init
  C2 — Integración con pipeline (timestamps en IpcPacket)
  C3 — UI: mostrar hora RTC, sync NTP

FASE D — Módulo MicroSD
  D1 — Driver SD + mount FAT32
  D2 — Logger de eventos a CSV en SD
  D3 — UI: panel de logs, espacio libre

FASE E — Cámara OV2640
  E1 — Driver cámara + captura básica
  E2 — Stream VGA en WebSocket (preview en UI)
  E3 — Sincronización temporal térmico+VGA

FASE F — Sistema de grabación
  F1 — Buffer circular en PSRAM
  F2 — Escritura de clips térmicos a SD
  F3 — Escritura de clips VGA + metadata JSON
  F4 — UI: galería de clips, descarga, etiquetas

FASE G — Seguridad y hardening
  G1 — Autenticación OTA
  G2 — OTA con verificación de firma
```

---

## 3. Reglas para el Agente Implementador

### 3.1 Antes de cada etapa
1. Leer **este archivo** para entender el contexto global.
2. Leer el archivo de la etapa asignada completamente antes de escribir código.
3. Verificar que las dependencias de la etapa están completas.
4. Hacer `idf.py fullclean` si se modifican `CMakeLists.txt`.

### 3.2 Durante la implementación
- **No usar `double`** — solo `float`. El FPU del S3 es de precisión simple.
- **No usar `new`/`malloc` en runtime** — solo durante init.
- **No usar `delay()`** — solo `vTaskDelay()` o `vTaskDelayUntil()`.
- **No romper el sistema base** — si el hardware adicional no está presente, el sistema debe seguir funcionando.
- **Compilar después de cada archivo nuevo** antes de continuar.

### 3.3 Convenciones de código
- Clases: `PascalCase`
- Métodos/variables: `camelCase` o `snake_case` según el archivo existente (mantener consistencia)
- Miembros privados: `trailing_underscore_`
- Constantes: `UPPER_SNAKE_CASE`
- Tags de log: string estático `"COMPONENT_NAME"`

### 3.4 Al finalizar cada etapa
- Verificar que `idf.py build` pasa sin errores ni warnings nuevos.
- Verificar HWM (High Water Mark) de stack de tareas afectadas.
- Marcar checklist de la etapa.
- **No empezar la siguiente etapa sin completar el checklist.**

---

## 4. Mapa de Archivos del Plan

| Archivo | Etapa | Fase | Dependencias |
|---|---|---|---|
| `PLAN-00-indice-y-revision-de-pares.md` | Índice | — | — |
| `PLAN-A1-bugfix-chess-noise.md` | Bugs + chess + noise | A | Ninguna |
| `PLAN-A2-tracker-tracklet.md` | Nuevo tracker | A | A1 |
| `PLAN-A3-fsm-puerta-infinita.md` | FSM de puerta | A | A2 |
| `PLAN-A4-ui-colores-lineas.md` | UI mejoras | A | A3 |
| `PLAN-B1-lineas-dibujables-ui.md` | Líneas manuales | B | A4 |
| `PLAN-B2-persistencia-config.md` | NVS config puertas | B | B1 |
| `PLAN-C1-driver-rtc.md` | Driver RTC DS3231 | C | A1 |
| `PLAN-C2-rtc-pipeline.md` | RTC en pipeline | C | C1 |
| `PLAN-C3-rtc-ui.md` | UI RTC | C | C2 |
| `PLAN-D1-driver-sd.md` | Driver SD Card | D | C1 |
| `PLAN-D2-logger-csv.md` | Logger eventos | D | D1 |
| `PLAN-D3-ui-sd.md` | UI panel SD | D | D2 |
| `PLAN-E1-driver-camara.md` | Driver OV2640 | E | D1 |
| `PLAN-E2-stream-vga.md` | Stream VGA WebSocket | E | E1 |
| `PLAN-E3-sync-thermal-vga.md` | Sincronización | E | E2 |
| `PLAN-F1-buffer-circular.md` | Buffer PSRAM | F | E3 |
| `PLAN-F2-clips-termicos.md` | Clips térmicos SD | F | F1 |
| `PLAN-F3-clips-vga-metadata.md` | Clips VGA + metadata | F | F2 |
| `PLAN-F4-ui-galeria.md` | UI galería de clips | F | F3 |
| `PLAN-G1-autenticacion-ota.md` | Auth OTA | G | A1 |

---

## 5. Estimación de Recursos por Fase

| Fase | RAM extra (SRAM) | RAM extra (PSRAM) | CPU overhead |
|---|---|---|---|
| A (bugs + algoritmo) | −20 KB (reducir queue) | 0 | +2ms/frame (tracklet) |
| B (config puertas) | +2 KB | 0 | ~0 |
| C (RTC) | +1 KB | 0 | ~0 |
| D (SD) | +8 KB (buffer FAT) | 0 | async |
| E (cámara) | +4 KB | +300 KB (frame buffers) | +15ms/frame VGA |
| F (grabación) | +2 KB | +2–5 MB (circular buffer) | async |
| G (seguridad) | +1 KB | 0 | ~0 |

**Total estimado con todo activo:** ~270 KB SRAM + ~5 MB PSRAM — dentro del presupuesto del ESP32-S3 con PSRAM de 8 MB.

---

*Plan generado con revisión por pares de los 9 archivos del agente anterior.*  
*Versión: 2.0 — Marzo 2026*
