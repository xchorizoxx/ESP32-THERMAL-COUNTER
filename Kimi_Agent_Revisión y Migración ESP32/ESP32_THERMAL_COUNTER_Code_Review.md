# 🔍 REVISIÓN DE CÓDIGO: ESP32-THERMAL-COUNTER
## Sistema de Conteo de Personas basado en Visión Térmica Edge AI

**Repositorio:** https://github.com/xchorizoxx/ESP32-THERMAL-COUNTER  
**Versión analizada:** v0.6.0-alpha.tactical  
**Fecha de revisión:** 26 de Marzo, 2026  
**Revisor:** Análisis automatizado de código embebido

---

# 1. 📋 RESUMEN EJECUTIVO

## Fortalezas Principales

| Área | Evaluación | Comentario |
|------|------------|------------|
| **Arquitectura FreeRTOS** | ⭐⭐⭐⭐⭐ | División asimétrica bien diseñada entre Core 0 (Telemetry) y Core 1 (Vision) |
| **Pipeline de Visión** | ⭐⭐⭐⭐⭐ | Implementación determinística a 16 FPS con EMA, NMS y Alpha-Beta tracking |
| **Gestión de Memoria** | ⭐⭐⭐⭐⭐ | Uso extensivo de static allocation, zero fragmentation |
| **Comunicaciones** | ⭐⭐⭐⭐ | WebSockets + UDP binario, protocolo eficiente |
| **Documentación** | ⭐⭐⭐⭐ | Código bien documentado con headers claros |
| **Seguridad** | ⭐⭐⭐ | SoftAP WPA2, pero sin cifrado adicional en WebSocket |

## Riesgos Críticos Identificados

| Severidad | Issue | Impacto |
|-----------|-------|---------|
| 🔴 **HIGH** | Falta de validación de bounds en algunos loops | Potencial buffer overflow |
| 🔴 **HIGH** | No hay manejo de error si `xQueueSend` falla | Pérdida silenciosa de datos |
| 🟡 **MEDIUM** | Magic numbers hardcodeados (15 tracks, valores de líneas) | Dificulta mantenimiento |
| 🟡 **MEDIUM** | Sin protección contra ID collision en tracks | Posible reutilización prematura de IDs |
| 🟢 **LOW** | No hay persistencia de contadores en NVS | Se pierden en reinicio |

---

# 2. 🔬 ANÁLISIS POR COMPONENTE

## 2.1 ARQUITECTURA EMBEBIDA & FREERTOS

### División Asimétrica de Tareas

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3 DUAL-CORE                       │
├──────────────────────────────┬──────────────────────────────┤
│       CORE 0 (PRO_CPU)       │       CORE 1 (APP_CPU)       │
│                              │                              │
│  ┌──────────────────────┐   │   ┌──────────────────────┐   │
│  │   TelemetryTask      │   │   │   ThermalPipeline    │   │
│  │   - SoftAP WiFi      │   │   │   - 16 Hz determin.  │   │
│  │   - HTTP Server      │   │   │   - Vision Engine    │   │
│  │   - UDP Broadcast    │   │   │   - Background EMA   │   │
│  │   - WebSockets       │   │   │   - Peak Detection   │   │
│  └──────────────────────┘   │   │   - NMS + Tracking   │   │
│           │                  │   └──────────────────────┘   │
│           ▼                  │            │                 │
│  ┌──────────────────────┐   │            ▼                 │
│  │   IPC Queue          │◄──┼──────────┤ xQueueSend       │
│  │   (Static, 4 slots)  │   │                              │
│  └──────────────────────┘   │                              │
└──────────────────────────────┴──────────────────────────────┘
```

**Evaluación:** ✅ **EXCELENTE**

- La división es lógica y aprovecha las fortalezas de cada core
- Core 1 ejecuta el pipeline de visión con máxima prioridad (`configMAX_PRIORITIES - 1`)
- Core 0 maneja E/S de red sin interferir con el procesamiento en tiempo real
- Uso de `vTaskDelayUntil` garantiza determinismo temporal de 16 Hz

### Static Allocation vs Heap Allocation

| Componente | Tipo | Tamaño | Estado |
|------------|------|--------|--------|
| `pipelineStack` | Static | 6,144 bytes | ✅ Correcto |
| `telemetryStack` | Static | 3,584 bytes | ✅ Correcto |
| `ipcQueue` | Static | 4 × sizeof(IpcPacket) ≈ 6.5 KB | ✅ Correcto |
| `configQueue` | Dynamic | 10 × sizeof(AppConfigCmd) | ⚠️ Podría ser static |
| Buffers de imagen | Static | 768 × 4 × 3 = 9,216 bytes | ✅ Correcto |

**Análisis:** El código hace un uso ejemplar de static allocation, eliminando completamente el riesgo de heap fragmentation durante runtime. Esto es crítico para sistemas embebidos de larga duración.

### Sincronización entre Cores

```cpp
// Cola estática para IPC Core 1 → Core 0
static uint8_t queueStorage[ThermalConfig::IPC_QUEUE_DEPTH * sizeof(IpcPacket)];
QueueHandle_t ipcQueue = xQueueCreateStatic(
    ThermalConfig::IPC_QUEUE_DEPTH,  // 4 slots
    sizeof(IpcPacket),               // ~1.6 KB cada uno
    queueStorage,
    &queueBuffer
);
```

**Observaciones:**
- ✅ Uso de `xQueueCreateStatic` previene fragmentación
- ✅ Timeout de 0 en `xQueueSend` evita bloqueo del pipeline
- ⚠️ **Issue:** No se verifica el retorno de `xQueueSend` - datos pueden perderse si la cola está llena

### Watchdog y Manejo de Errores I2C

```cpp
// Configuración del watchdog en main.cpp
esp_task_wdt_config_t wdtCfg = {
    .timeout_ms     = 5000,   // 5 segundos
    .idle_core_mask = 0,      // No monitorea idle tasks
    .trigger_panic  = true    // Reinicio en caso de trigger
};
```

**Evaluación:**
- ✅ WDT correctamente configurado con 5s timeout
- ✅ Reset del WDT en cada iteración del pipeline
- ✅ Stack High Water Mark monitoreado periódicamente
- ⚠️ **Issue:** El timeout de 5s podría ser agresivo para debugging

### Determinismo Temporal

```cpp
const TickType_t period = pdMS_TO_TICKS(1000 / ThermalConfig::PIPELINE_FREQ_HZ);  // 62.5ms
TickType_t lastWakeTime = xTaskGetTickCount();

while (true) {
    // ... procesamiento ...
    vTaskDelayUntil(&lastWakeTime, period);  // Garantiza 16 Hz exactos
}
```

**Evaluación:** ✅ **EXCELENTE** - Uso correcto de `vTaskDelayUntil` para muestreo constante.

---

## 2.2 PIPELINE DE VISIÓN Y ALGORITMOS

### Filtro "De-Chess" para MLX90640

El sensor MLX90640 opera en modo "chess" donde los píxeles se leen en dos sub-frames alternos. El código maneja esto:

```cpp
uint8_t currentSubPage = sensor_.getLastSubPageID();

// Subframe 8Hz visualization sync
if (currentSubPage == 1) {
    memcpy(display_frame_, current_frame_, sizeof(display_frame_));
}
```

**Evaluación:** ⚠️ **PARCIAL** - El código lee ambos sub-frames pero no implementa un verdadero filtro de-interlacing. La visualización solo se actualiza en sub-frame 1, lo que puede causar artefactos visuales.

### Modelo de Background EMA con Selective Learning

```cpp
void BackgroundModel::update(const float* frame, float* background,
                             const uint8_t* mask, int totalPixels, float alpha)
{
    const float oneMinusAlpha = 1.0f - alpha;

    for (int i = 0; i < totalPixels; i++) {
        if (mask[i] == 0) {
            background[i] = alpha * frame[i] + oneMinusAlpha * background[i];
        }
        // Si mask[i] == 1 (bloqueado por track), background[i] permanece inmutable
    }
}
```

**Evaluación:** ✅ **EXCELENTE**

- Selective learning correctamente implementado
- Las áreas con tracks activos no actualizan el background
- Esto previene que las personas estáticas sean absorbidas por el modelo

### Algoritmo de Peak Detection

```cpp
void PeakDetector::detect(const float* currentFrame, const float* backgroundMap,
                          ThermalPeak* peaks, int* numPeaks,
                          float tempMin, float deltaT, int maxPeaks)
{
    for (int r = 1; r < rows - 1; r++) {
        for (int c = 1; c < cols - 1; c++) {
            // Condición 1: Excede temperatura biológica mínima
            if (val < tempMin) continue;

            // Condición 2: Alto contraste vs background
            if (val - backgroundMap[i] < deltaT) continue;

            // Condición 3: Máximo local en vecindario 3x3
            // ... verificación de 8 vecinos ...
        }
    }
}
```

**Evaluación:** ✅ **BUENO**

- Triple verificación: temp mínima, contraste, máximo local
- Límites `r = 1` a `rows - 1` evitan acceso fuera de bounds
- Complejidad O(N) donde N = 768 píxeles

### Non-Maximum Suppression (NMS) Adaptativo

```cpp
void NmsSuppressor::suppress(ThermalPeak* peaks, int numPeaks,
                             int rCenterSq, int rEdgeSq,
                             int centerXMin, int centerXMax)
{
    // Step 1: Insertion sort por temperatura descendente
    // O(N²) óptimo para N≤15

    // Step 2: Supresión - El pico más caliente domina su vecindario
    const int radiusSq = (xj >= centerXMin && xj <= centerXMax)
                         ? rCenterSq
                         : rEdgeSq;
}
```

**Evaluación:** ✅ **EXCELENTE**

- Radio adaptativo compensa distorsión de lente en bordes
- Insertion sort es óptimo para N pequeño (cache-friendly)
- Complejidad O(N²) = O(225) para N=15, insignificante

### Tracker Alpha-Beta

```cpp
void AlphaBetaTracker::update(...) {
    // Fase 1: Predicción & Actualización de Edad
    tracks_[i].x += tracks_[i].v_x;
    tracks_[i].y += tracks_[i].v_y;

    // Fase 2: Asociación de Datos (Greedy Nearest Neighbor)
    // Fase 3: Actualización con Filtro Alpha-Beta
    tracks_[i].x   += alpha * residualX;
    tracks_[i].y   += alpha * residualY;
    tracks_[i].v_x += beta  * residualX;
    tracks_[i].v_y += beta  * residualY;
}
```

**Parámetros:**
- `ALPHA_TRK = 0.6` - Ganancia de posición (alta = respuesta rápida)
- `BETA_TRK = 0.3` - Ganancia de velocidad

**Evaluación:** ✅ **BUENO**

- Alpha=0.6 proporciona buen balance entre suavizado y respuesta
- Greedy NeN es adecuado para baja densidad de personas
- ⚠️ **Limitación:** En alta densidad (>5 personas cercanas), puede ocurrir identity switching

### Lógica de Crossing con Intent Inference

```cpp
void AlphaBetaTracker::evaluateCountingLogic(Track& track, ...) {
    // Máquina de estados con histéresis
    // state_y: 0 (Upper), 1 (Neutral), 2 (Lower)

    if (track.state_y == 0 && currentZone == 2) {
        countIn++;      // Entrada: Upper → Lower
        track.state_y = 2;
    } else if (track.state_y == 2 && currentZone == 0) {
        countOut++;     // Salida: Lower → Upper
        track.state_y = 0;
    }
}
```

**Evaluación:** ✅ **EXCELENTE**

- Máquina de estados con histéresis previene conteos dobles
- Zona neutral evita oscilaciones en la línea
- Lógica direccional clara

---

## 2.3 OPTIMIZACIÓN DE MEMORIA Y RENDIMIENTO

### Estructuras Packed para Transmisión

```cpp
struct __attribute__((packed)) TrackInfo {
    uint8_t  id;
    int16_t  x_100;    // X × 100 (fixed-point)
    int16_t  y_100;    // Y × 100
    int16_t  v_x_100;  // VX × 100
    int16_t  v_y_100;  // VY × 100
};  // 9 bytes por track

struct __attribute__((packed)) TelemetryPayload {
    uint32_t  frame_id;
    float     ambient_temp;
    int16_t   count_in;
    int16_t   count_out;
    uint8_t   num_tracks;
    TrackInfo tracks[MAX_TRACKS];  // 15 × 9 = 135 bytes
};  // ~148 bytes total
```

**Evaluación:** ✅ **EXCELENTE**

- Uso de fixed-point (×100) evita floats en red
- `__attribute__((packed))` elimina padding
- Transmisión binaria eficiente (no JSON/texto)

### Zero-Copy entre Pipeline y Red

```cpp
// IpcPacket compartido entre cores
struct IpcPacket {
    bool sensor_ok;
    TelemetryPayload telemetry;
    ImagePayload image;
};  // ~1.6 KB

// Uso de static en el dispatcher
static IpcPacket packet;  // static: reduce stack usage
memset(&packet, 0, sizeof(IpcPacket));
```

**Evaluación:** ✅ **BUENO**

- Estructura compartida evita copias innecesarias
- Uso de `static` en lugar de stack es correcto
- ⚠️ **Nota:** `memset` de 1.6 KB en cada frame (16 Hz) = 25.6 KB/s de escritura

### Pre-allocación Estática de Buffers

```cpp
// En ThermalPipeline (clase)
float   current_frame_[ThermalConfig::TOTAL_PIXELS];    // 768 × 4 = 3,072 B
float   display_frame_[ThermalConfig::TOTAL_PIXELS];    // 3,072 B
float   background_map_[ThermalConfig::TOTAL_PIXELS];   // 3,072 B
uint8_t blocking_mask_[ThermalConfig::TOTAL_PIXELS];    // 768 B
ThermalPeak peaks_[ThermalConfig::MAX_PEAKS];           // 15 × 8 = 120 B
Track   tracks_[15];                                     // 15 × 24 = 360 B
```

**Total de buffers estáticos:** ~10.5 KB por instancia de pipeline

**Evaluación:** ✅ **EXCELENTE** - Zero heap allocation durante runtime.

### Uso de Memoria en ESP32-S3

| Componente | Memoria | Notas |
|------------|---------|-------|
| Pipeline Task Stack | 6,144 B | Suficiente para procesamiento |
| Telemetry Task Stack | 3,584 B | Adecuado para E/S |
| IpcQueue | 6,500 B | 4 slots × ~1.6 KB |
| Buffers de imagen | 10,464 B | 3× arrays float + 1× byte |
| Tracks + Peaks | ~500 B | Estructuras de tracking |
| **Total estimado** | **~27 KB** | Bien dentro de los límites del S3 |

El ESP32-S3 tiene 512 KB SRAM, por lo que el uso de ~27 KB es muy conservador.

---

## 2.4 COMUNICACIONES Y RED

### Servidor HTTP Embebido

El sistema implementa un servidor HTTP embebido con:
- Página web HUD (HTML Canvas 2D)
- WebSockets para streaming en tiempo real
- Endpoints REST para configuración
- Endpoint POST `/update` para OTA

**Evaluación:** ✅ **BUENO**

- Interfaz web integrada elimina necesidad de app nativa
- WebSockets más eficiente que polling HTTP

### Protocolo WebSocket Binario

```cpp
// Transmisión de frame térmico
HttpServer::broadcastFrame(packet.image, packet.telemetry, packet.sensor_ok);

// Estructura ImagePayload
struct ImagePayload {
    uint32_t frame_id;
    int16_t  pixels[TOTAL_PIXELS];  // 768 × 2 = 1,536 bytes (fixed-point ×100)
};  // ~1.5 KB por frame
```

**Evaluación:** ⚠️ **PARCIAL**

- Formato binario eficiente
- ⚠️ **Issue:** No hay compresión de imagen - 1.5 KB × 16 FPS = 24 KB/s de tráfico WebSocket
- Para uso remoto, esto puede saturar conexiones lentas

### Seguridad del SoftAP

```cpp
// Configuración en thermal_config.hpp
constexpr char SOFTAP_SSID[] = "ThermalCounter";
constexpr char SOFTAP_PASS[] = "12345678";  // ⚠️ Contraseña débil
constexpr int  SOFTAP_CHANNEL = 1;
constexpr int  SOFTAP_MAX_CONN = 4;
```

**Evaluación:** 🔴 **CRÍTICO**

- Contraseña WPA2 de solo 8 caracteres numéricos
- Sin opción de configurar credenciales personalizadas
- Red abierta a cualquier cliente en rango
- ⚠️ **Recomendación:** Implementar configuración de SSID/pass vía NVS

### Sistema de OTA

```cpp
// En main.cpp
esp_err_t ota_valid = esp_ota_mark_app_valid_cancel_rollback();
if (ota_valid == ESP_OK) {
    ESP_LOGI(TAG, "[OTA] Partition marked as VALID");
}
```

**Evaluación:** ✅ **BUENO**

- Uso correcto de `esp_ota_mark_app_valid_cancel_rollback()`
- Sistema anti-bootloop implementado
- ⚠️ **Falta:** Validación de firma de firmware

---

## 2.5 CALIBRACIÓN Y CONFIGURACIÓN

### Persistencia en NVS Flash

**Análisis:** El código inicializa NVS (`nvs_flash_init()`) pero **no persiste** los parámetros de configuración:

```cpp
// Parámetros que deberían persistirse pero no lo hacen:
extern float EMA_ALPHA;
extern float BIOLOGICAL_TEMP_MIN;
extern float BACKGROUND_DELTA_T;
extern int   NMS_RADIUS_CENTER_SQ;
extern int   NMS_RADIUS_EDGE_SQ;
extern int   DEFAULT_LINE_ENTRY_Y;
extern int   DEFAULT_LINE_EXIT_Y;
```

**Evaluación:** 🟡 **MEDIUM** - Los cambios de configuración se pierden en reinicio.

### Lógica de Calibración de Umbrales

```cpp
// Valores por defecto razonables
float BIOLOGICAL_TEMP_MIN = 25.0f;   // °C - Umbral de detección humana
float BACKGROUND_DELTA_T = 1.5f;      // °C - Contraste mínimo vs fondo
```

**Evaluación:** ✅ **BUENO** - Valores iniciales adecuados para entornos interiores.

### Flexibilidad de Parámetros

| Parámetro | Tipo | Ajustable vía Web | Persistente |
|-----------|------|-------------------|-------------|
| EMA_ALPHA | float | ✅ Sí | ❌ No |
| BIOLOGICAL_TEMP_MIN | float | ✅ Sí | ❌ No |
| BACKGROUND_DELTA_T | float | ✅ Sí | ❌ No |
| NMS_RADIUS | int | ✅ Sí | ❌ No |
| Líneas de conteo | int | ✅ Sí | ❌ No |
| VIEW_MODE | int | ✅ Sí | ❌ No |

---

## 2.6 CALIDAD DE CÓDIGO Y BUENAS PRÁCTICAS

### Cumplimiento de Estándares

| Estándar | Cumplimiento | Notas |
|----------|--------------|-------|
| MISRA-C | ⚠️ Parcial | Algunas desviaciones justificadas |
| ESP-IDF Style | ✅ Bueno | Consistente con ejemplos oficiales |
| Documentación | ✅ Excelente | Headers con Doxygen-style comments |
| Modularización | ✅ Excelente | Separación clara en components/ |

### Modularización

```
components/
├── mlx90640_driver/     # Driver I2C del sensor
│   ├── include/
│   └── src/
├── thermal_pipeline/    # Pipeline de visión
│   ├── include/
│   └── src/
├── telemetry/           # WiFi + UDP
│   ├── include/
│   └── src/
└── web_server/          # HTTP + WebSockets
    ├── include/
    └── src/
```

**Evaluación:** ✅ **EXCELENTE** - Estructura de proyecto profesional siguiendo convenciones ESP-IDF.

### Manejo de Errores

```cpp
// Patrón común en el código
esp_err_t ret = sensor.init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "FAILED to initialize sensor");
    // Continúa en lugar de abortar - permite retry vía UI
}
```

**Evaluación:** ⚠️ **PARCIAL**

- Buen logging de errores
- Algunos errores no son fatales (permite recuperación)
- ⚠️ **Issue:** No hay mecanismo de retry automático para I2C

### Documentación Inline

**Ejemplo de buena documentación:**
```cpp
/**
 * @brief Step 4 — Tracking with Alpha-Beta Filter + Counting Logic.
 *
 * Tracks people between frames using an Alpha-Beta predictive filter
 * and counts entries/exits with a hysteresis state machine.
 */
```

**Evaluación:** ✅ **EXCELENTE** - Comentarios claros y descriptivos.

---

## 2.7 ROBUSTEZ Y CASOS ESQUINA

### Race Conditions

**Análisis de sincronización:**

```cpp
// Core 1 (Vision) produce:
xQueueSend(ipcQueue_, &packet, 0);  // Non-blocking

// Core 0 (Telemetry) consume:
xQueueReceive(ipcQueue_, &packet, portMAX_DELAY);  // Blocking
```

**Evaluación:** ⚠️ **Riesgo Identificado**

- El productor no bloquea (timeout 0), pero tampoco verifica el retorno
- Si la cola está llena, el frame se pierde silenciosamente
- **Recomendación:** Verificar retorno y loggear pérdidas

### Fallos de Sensor I2C

```cpp
// Manejo actual
bool sensor_ok = false;
if (sensor_initialized_ && (sensor_.readFrame(current_frame_) == ESP_OK)) {
    sensor_ok = true;
} else {
    sensor_initialized_ = false;  // Deshabilita lecturas
}
```

**Evaluación:** ⚠️ **PARCIAL**

- Detección de fallo correcta
- Permite retry manual vía UI
- ⚠️ **Falta:** Retry automático con backoff exponencial

### Gestión de Límites en Tracking

```cpp
Track* AlphaBetaTracker::findFreeTrack() {
    for (int i = 0; i < 15; i++) {  // MAX_TRACKS hardcodeado
        if (!tracks_[i].active) return &tracks_[i];
    }
    return nullptr;  // No hay espacio - nuevo track rechazado
}
```

**Evaluación:** ⚠️ **PARCIAL**

- Límite de 15 tracks puede ser insuficiente en alta densidad
- Sin política de reemplazo (LRU) cuando se alcanza el límite
- Ghost tracks manejados por `maxAge`

### Precisión en Alta Densidad

**Escenario de prueba:** 10 personas simultáneas en campo de visión

| Aspecto | Comportamiento | Evaluación |
|---------|----------------|------------|
| Detección | MAX_PEAKS=15, suficiente | ✅ |
| Tracking | Greedy NeN puede fallar | ⚠️ |
| Identity | Alto riesgo de switching | 🔴 |
| Conteo | Depende de separación espacial | ⚠️ |

---

## 2.8 MEJORAS SUGERIDAS

### Optimizaciones de Rendimiento

1. **Compresión de imagen térmica**
   - Implementar delta-encoding entre frames
   - Reducir tráfico WebSocket en ~70%

2. **Vectorización**
   - Usar instrucciones SIMD del ESP32-S3 para EMA
   - Potencial speedup de 2-4x en operaciones de punto flotante

3. **Reducción de frecuencia de transmisión**
   - Transmitir imagen completa a 8 Hz en lugar de 16 Hz
   - Mantener telemetry a 16 Hz

### Mejoras en Arquitectura

1. **Persistencia NVS**
   ```cpp
   // Implementar guardado/carga de configuración
   void ConfigManager::saveToNvs();
   void ConfigManager::loadFromNvs();
   ```

2. **Sistema de Eventos**
   - Reemplazar polling de configQueue con eventos asíncronos
   - Reducir latencia de respuesta a comandos UI

3. **Múltiples Perfiles**
   - Perfiles de configuración para diferentes escenarios
   - Ej: "Oficina", "Almacén", "Exterior"

### Hardening Industrial

1. **Cifrado de comunicaciones**
   - TLS para WebSockets (wss://)
   - Autenticación de clientes

2. **Watchdog de aplicación**
   - Monitoreo de health del pipeline
   - Auto-reinicio si no hay frames procesados en X segundos

3. **Logging persistente**
   - Almacenar logs en NVS o SPIFFS
   - Facilitar debugging remoto

### Testing Unitario

**Herramientas recomendadas:**

| Herramienta | Propósito | Integración |
|-------------|-----------|-------------|
| **Unity** | Framework de testing | Incluido en ESP-IDF |
| **CMock** | Mocking de funciones | Para aislar componentes |
| **gcov** | Code coverage | Análisis de cobertura |

**Casos de prueba prioritarios:**

```cpp
// Test de PeakDetector
TEST(PeakDetector, DetectsSinglePeak) {
    float frame[768] = {0};
    frame[100] = 35.0f;  // Pico caliente
    // ... verificar detección correcta
}

// Test de AlphaBetaTracker
TEST(Tracker, CountsEntryCorrectly) {
    // Simular trayectoria de entrada
    // Verificar count_in incrementado
}

// Test de NMS
TEST(NMS, SuppressesNearbyPeaks) {
    // Dos picos cercanos
    // Verificar que solo uno sobrevive
}
```

---

# 3. 🐛 ISSUES ENCONTRADOS

## 🔴 CRÍTICO (1)

### ISSUE-001: Sin validación de retorno de xQueueSend

**Ubicación:** `thermal_pipeline.cpp:run()`

```cpp
xQueueSend(ipcQueue_, &packet, 0);  // Retorno ignorado
```

**Impacto:** Pérdida silenciosa de frames si la cola está llena

**Recomendación:**
```cpp
if (xQueueSend(ipcQueue_, &packet, 0) != pdTRUE) {
    ESP_LOGW(TAG, "IPC queue full, frame %lu dropped", frame_id_);
    dropped_frames_++;
}
```

---

## 🟡 HIGH (3)

### ISSUE-002: Contraseña WPA2 débil y hardcodeada

**Ubicación:** `thermal_config.hpp`

```cpp
constexpr char SOFTAP_PASS[] = "12345678";
```

**Impacto:** Seguridad comprometida, acceso no autorizado

**Recomendación:** Permitir configuración vía NVS con validación de fortaleza

### ISSUE-003: Magic numbers en código

**Ubicación:** `alpha_beta_tracker.cpp`

```cpp
Track   tracks_[15];  // ¿Por qué 15?
// ...
if (tracks_[i].age > (uint8_t)maxAge)  // maxAge viene de config
```

**Impacto:** Dificulta mantenimiento, inconsistencias potenciales

**Recomendación:** Usar `constexpr int MAX_TRACKS = 15;` centralizado

### ISSUE-004: Sin persistencia de configuración

**Ubicación:** Todo el sistema

**Impacto:** Configuración se pierde en cada reinicio

**Recomendación:** Implementar capa de persistencia NVS

---

## 🟢 MEDIUM (4)

### ISSUE-005: No hay compresión de imagen térmica

**Impacto:** Alto uso de ancho de banda (24 KB/s)

**Recomendación:** Implementar delta-encoding o cuantización

### ISSUE-006: Sin retry automático de sensor I2C

**Impacto:** Requiere intervención manual tras fallo de sensor

**Recomendación:** Implementar retry con backoff exponencial

### ISSUE-007: Política de reemplazo de tracks

**Impacto:** En alta densidad, nuevos tracks pueden no crearse

**Recomendación:** Implementar LRU para reemplazar tracks inactivos

### ISSUE-008: Filtro De-Chess incompleto

**Impacto:** Posibles artefactos visuales en streaming

**Recomendación:** Implementar interpolación temporal entre sub-frames

---

## 🟣 LOW (2)

### ISSUE-009: WDT timeout agresivo para debugging

**Impacto:** Dificulta debugging con breakpoints

**Recomendación:** Configurar timeout más largo en modo DEBUG

### ISSUE-010: Contadores no persisten en reinicio

**Impacto:** Pérdida de estadísticas históricas

**Recomendación:** Guardar contadores periódicamente en NVS

---

# 4. ✅ RECOMENDACIONES DE MEJORA (Priorizadas)

## Prioridad 1: Seguridad y Robustez

1. **Implementar configuración de WiFi vía NVS**
   - Permitir cambiar SSID y contraseña
   - Validar fortaleza de contraseña

2. **Agregar validación de retornos de cola**
   - Monitorear pérdida de frames
   - Alertar si la tasa de pérdida es alta

3. **Persistencia de configuración**
   - Guardar todos los parámetros ajustables en NVS
   - Cargar al inicio

## Prioridad 2: Rendimiento

4. **Compresión de imagen térmica**
   - Reducir tráfico de red
   - Mejorar experiencia en conexiones lentas

5. **Optimización del filtro De-Chess**
   - Interpolación temporal para suavizado visual

## Prioridad 3: Funcionalidad

6. **Retry automático de sensor**
   - Backoff exponencial
   - Logging de intentos

7. **Mejor gestión de tracks**
   - Política LRU para reemplazo
   - Configuración dinámica de MAX_TRACKS

8. **Testing unitario**
   - Framework Unity
   - Cobertura de algoritmos críticos

---

# 5. 📊 CONCLUSIÓN Y VEREDICTO

## Resumen de Calidad

| Categoría | Puntuación | Comentario |
|-----------|------------|------------|
| **Arquitectura** | 9/10 | Diseño dual-core bien pensado |
| **Algoritmos** | 8/10 | Implementaciones sólidas, algunas mejoras posibles |
| **Memoria** | 10/10 | Uso ejemplar de static allocation |
| **Comunicaciones** | 7/10 | Funcional pero falta seguridad |
| **Robustez** | 7/10 | Buen manejo de errores, faltan casos esquina |
| **Código** | 9/10 | Limpio, bien documentado, modular |
| **Testing** | 4/10 | Sin tests unitarios visibles |
| **Seguridad** | 5/10 | Contraseña débil, sin cifrado |

**Puntuación Global: 7.4/10**

## ¿Listo para Producción?

### ✅ Listo para:
- Demostraciones y pruebas de concepto
- Entornos controlados con acceso restringido
- Despliegues con supervisión humana

### ⚠️ Requiere trabajo antes de producción industrial:
- [ ] Configuración de seguridad del WiFi
- [ ] Persistencia de configuración en NVS
- [ ] Testing unitario de algoritmos críticos
- [ ] Hardening de comunicaciones (TLS)
- [ ] Documentación de deployment

### 🔴 No listo para:
- Entornos de alta seguridad sin modificaciones
- Despliegues desatendidos a largo plazo
- Aplicaciones con requisitos de certificación

## Fortalezas Clave

1. **Arquitectura FreeRTOS bien diseñada** - División asimétrica óptima
2. **Pipeline de visión determinístico** - 16 FPS consistentes
3. **Gestión de memoria excepcional** - Zero heap fragmentation
4. **Código limpio y documentado** - Fácil de mantener y extender

## Áreas de Mejora Críticas

1. **Seguridad del SoftAP** - Prioridad alta
2. **Persistencia de configuración** - Esencial para producción
3. **Testing automatizado** - Necesario para calidad industrial
4. **Compresión de imagen** - Mejoraría experiencia de usuario

---

# APÉNDICE: Métricas de Código

```
Lenguajes:     C (53.5%), C++ (43.6%), Python (2.3%), CMake (0.6%)
Líneas totales: ~3,500 (estimado)
Componentes:    4 principales
Tareas FreeRTOS: 2 + idle
Frecuencia:     16 Hz determinístico
Memoria usada:  ~27 KB RAM
```

---

*Informe generado mediante análisis automatizado de código.*  
*Para consultas o aclaraciones, referirse al repositorio oficial.*
