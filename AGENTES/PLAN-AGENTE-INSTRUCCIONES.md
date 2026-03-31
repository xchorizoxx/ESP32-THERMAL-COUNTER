# PLAN-AGENTE-INSTRUCCIONES.md
## Instrucciones Operacionales para el Agente Implementador
### ESP32 Thermal Counter Enhanced — ESP-IDF v6.0

---

> ⚠️ **LEE ESTE ARCHIVO COMPLETO ANTES DE TOCAR CUALQUIER CÓDIGO.**
> Es corto. No lo saltees.

---

## 0. Protocolo de Trabajo (siempre)

```
1. Leer este archivo
2. Leer el archivo de la etapa asignada completo
3. Verificar que las dependencias de etapa están completas
4. Implementar en el orden indicado dentro de la etapa
5. Compilar después de cada archivo nuevo o modificado
6. Completar el checklist antes de declarar la etapa terminada
7. Reportar: qué se hizo, qué compiló, qué no, qué se desvió del plan
```

**Nunca declares una etapa completa si el checklist tiene ítems sin marcar.**  
**Nunca empieces la siguiente etapa sin que la anterior compiló sin errores.**

---

## 1. Mapa de Etapas — Criterios de Éxito Binarios

Cada etapa tiene un criterio único de éxito. Si no se cumple, la etapa no está terminada.

| Etapa | Archivo | Criterio de éxito (pasa/no pasa) |
|---|---|---|
| **A1** | PLAN-A1-bugfix-chess-noise.md | `idf.py build` sin errores + HWM pipeline > 200 words en monitor |
| **A2** | PLAN-A2-tracker-tracklet.md | IDs estables en cruce manual visible en HUD (sin flickering de IDs) |
| **A3** | PLAN-A3-fsm-puerta-infinita.md | Mano en centro del sensor → no genera conteo |
| **A4** | PLAN-A4-ui-colores-zonas.md | Canvas muestra zonas coloreadas + escala fija 0-50°C en HUD |
| **B1** | PLAN-B1-lineas-dibujables-ui.md | Dibujar línea diagonal en canvas → cruzar con mano → conteo correcto |
| **C1** | PLAN-C1-D1-E1... | Log "DS3231 RTC initialized" o "RTC not available" (ambos son éxito) |
| **D1** | PLAN-C1-D1-E1... | Log "SD mounted" o "SD not available" (ambos son éxito) |
| **E1** | PLAN-C1-D1-E1... | Log "OV2640 camera initialized" o "Camera not available" (ambos son éxito) |
| **F1** | PLAN-F1-F4... | Log "Recording buffer initialized" con tamaño PSRAM |
| **F2** | PLAN-F1-F4... | Archivo `clips/0001/0001_thermal.bin` creado en SD tras cruce |
| **F3** | PLAN-F1-F4... | Archivo `clips/0001/0001_vga.mjpg` creado + metadata.json válido |
| **F4** | PLAN-F1-F4... | Panel de clips visible en UI + descarga de archivo funcional |
| **G1** | PLAN-G1-autenticacion-ota.md | `curl -X POST http://192.168.4.1/update` sin token → respuesta 401 |

---

## 2. Reglas Absolutas de Código (no negociables)

```cpp
// ✅ CORRECTO
float valor = 3.14f;           // float, no double
vTaskDelay(pdMS_TO_TICKS(100)); // no delay()
heap_caps_malloc(size, MALLOC_CAP_SPIRAM); // PSRAM explícita
ESP_LOGI(TAG, "mensaje");       // no printf()

// ❌ INCORRECTO — el agente no debe generar esto
double valor = 3.14;           // software emulation en S3
delay(100);                    // bloqueante, no FreeRTOS
malloc(size);                  // heap sin garantía de PSRAM
printf("mensaje");             // no usa el sistema de log
new MiClase();                 // no en runtime, solo en init
```

**Memoria dinámica:** Solo `malloc`/`new` durante `app_main()` o `init()`. Nunca en bucles de tarea o en el pipeline de 16 Hz.

---

## 3. Reglas ESP-IDF 6.0 (leer antes de escribir drivers)

### 3.1 I2C Master (API cambió en IDF 6.0)

```cpp
// ✅ IDF 6.0 — API nueva (usar esto)
#include "driver/i2c_master.h"

i2c_master_bus_config_t bus_cfg = {
    .i2c_port            = I2C_NUM_0,
    .sda_io_num          = GPIO_NUM_8,
    .scl_io_num          = GPIO_NUM_9,
    .clk_source          = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt   = 7,
    .intr_priority       = 0,
    .flags.enable_internal_pullup = true,
};
i2c_master_bus_handle_t bus;
i2c_new_master_bus(&bus_cfg, &bus);

i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address  = 0x68,
    .scl_speed_hz    = 100000,
    .scl_wait_us     = 0,
    .flags           = { .disable_ack_check = false },
};
i2c_master_dev_handle_t dev;
i2c_master_bus_add_device(bus, &dev_cfg, &dev);

// Leer:
i2c_master_transmit_receive(dev, &reg, 1, data, len, pdMS_TO_TICKS(50));

// Escribir:
i2c_master_transmit(dev, buf, len, pdMS_TO_TICKS(50));

// ❌ IDF 5.x — NO usar esto
#include "driver/i2c.h"        // API legacy — eliminada en 6.0
i2c_driver_install(...)        // no existe en 6.0
i2c_master_write_to_device(...)// no existe en 6.0
```

**El DS3231 comparte el bus I2C0 con el MLX90640** (0x33 vs 0x68 — sin colisión). No crear I2C1. Usar `MLX90640_GetBusHandle()` para obtener el handle existente.

### 3.2 SPI / SD Card (IDF 6.0)

```cpp
// ✅ IDF 6.0 — SD via SPI
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"

sdmmc_host_t host = SDSPI_HOST_DEFAULT();
// host.slot es el SPI host (SPI2_HOST por defecto)

spi_bus_config_t bus = {
    .mosi_io_num     = GPIO_NUM_11,
    .miso_io_num     = GPIO_NUM_13,
    .sclk_io_num     = GPIO_NUM_12,
    .quadwp_io_num   = GPIO_NUM_NC,
    .quadhd_io_num   = GPIO_NUM_NC,
    .max_transfer_sz = 4096,
};
spi_bus_initialize((spi_host_device_t)host.slot, &bus, SDSPI_DEFAULT_DMA);

sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
slot.gpio_cs  = GPIO_NUM_4;
slot.host_id  = (spi_host_device_t)host.slot;

esp_vfs_fat_sdmmc_mount_config_t mount = {
    .format_if_mount_failed = false,
    .max_files              = 8,
    .allocation_unit_size   = 16 * 1024,
};
sdmmc_card_t* card;
esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &mount, &card);

// ❌ IDF 5.x diferencias:
// En IDF 6.0, SDSPI_DEFAULT_DMA reemplaza SDSPI_DEFAULT_HOST
// mount_config ya no tiene campo 'disk_status_check_enable' en algunos builds
```

### 3.3 Cámara OV2640 (esp32-camera component)

```yaml
# idf_component.yml — agregar dependencia
dependencies:
  espressif/esp32-camera: ">=2.0.9"
```

```bash
# Después de editar idf_component.yml:
idf.py update-dependencies
idf.py fullclean   # OBLIGATORIO después de agregar componentes
idf.py build
```

```cpp
// ✅ Configuración correcta para PSRAM
camera_config_t config = {};
config.fb_location  = CAMERA_FB_IN_PSRAM;  // ← CRÍTICO, sin esto usa SRAM
config.fb_count     = 2;
config.pixel_format = PIXFORMAT_JPEG;
config.frame_size   = FRAMESIZE_VGA;
config.jpeg_quality = 15;
config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
```

### 3.4 PSRAM — Asignación Explícita

```cpp
// ✅ Siempre especificar PSRAM para buffers grandes
#include "esp_heap_caps.h"

void* buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
if (!buf) {
    ESP_LOGE(TAG, "PSRAM allocation failed (%zu bytes)", size);
    return ESP_ERR_NO_MEM;
}

// Verificar PSRAM disponible antes de asignar buffers grandes:
size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
ESP_LOGI(TAG, "Free PSRAM: %zu KB", free_psram / 1024);
```

### 3.5 CMakeLists.txt — Regla de Oro IDF 6.0

```
Cada vez que modifiques CMakeLists.txt en cualquier componente → idf.py fullclean antes de build.
Sin fullclean, IDF 6.0 puede tener dependencias stale que dan errores crípticos.
```

### 3.6 FreeRTOS en IDF 6.0

```cpp
// ✅ xTaskCreatePinnedToCore — sin cambios en IDF 6.0
// ✅ xQueueCreateStatic — sin cambios
// ✅ portENTER_CRITICAL / portEXIT_CRITICAL — sin cambios

// ⚠️ CAMBIO en IDF 6.0: configSTACK_DEPTH_TYPE ahora es uint32_t
// Si ves warnings de tipo en xTaskCreateStaticPinnedToCore, castear el stack size a uint32_t
xTaskCreateStaticPinnedToCore(
    fn, "name",
    (uint32_t)(stack_size / sizeof(StackType_t)),  // cast explícito
    param, priority, stack, &tcb, core
);
```

---

## 4. Pinout de Referencia (no tocar sin consultar)

```
GPIO 8  — MLX90640 SDA (I2C0)        ← NO reutilizar
GPIO 9  — MLX90640 SCL (I2C0)        ← NO reutilizar
GPIO 11 — SD Card MOSI (SPI2)
GPIO 12 — SD Card SCK  (SPI2)
GPIO 13 — SD Card MISO (SPI2)
GPIO 4  — SD Card CS   (SPI2)
GPIO 35 — OV2640 SIOD (SCCB SDA)
GPIO 36 — OV2640 SIOC (SCCB SCL)
GPIO 6  — OV2640 VSYNC
GPIO 7  — OV2640 HREF
GPIO 15 — OV2640 PCLK
GPIO 16 — OV2640 XCLK
GPIO 17 — OV2640 D0 (Y2)
GPIO 18 — OV2640 D1 (Y3)
GPIO 19 — OV2640 D2 (Y4)
GPIO 20 — OV2640 D3 (Y5)
GPIO 21 — OV2640 D4 (Y6)
GPIO 22 — OV2640 D5 (Y7)
GPIO 23 — OV2640 D6 (Y8)
GPIO 25 — OV2640 D7 (Y9)
GPIO 26 — OV2640 PWDN
GPIO 27 — OV2640 RESET

DS3231 SDA → GPIO 8  (comparte I2C0 con MLX90640, dirección 0x68 vs 0x33)
DS3231 SCL → GPIO 9  (comparte I2C0 con MLX90640)

Pines strapping — NUNCA usar para lógica: GPIO 0, 2, 5, 12, 15
Pines input-only en ESP32 (no en S3): GPIO 34-39 (en S3 son bidireccionales)
```

---

## 5. Separación de Responsabilidades del Sistema

```
Core 1 (APP_CPU) — SOLO estas tareas:
  - ThermalPipeline (pipeline de visión a 16 Hz)
  - FrameAccumulator, NoiseFilter, TrackletTracker, TrackletFSM
  - MaskGenerator, BackgroundModel, PeakDetector, NmsSuppressor
  - Máxima prioridad (configMAX_PRIORITIES - 1)
  - NO hacer operaciones de red, SD, o cámara aquí

Core 0 (PRO_CPU) — SOLO estas tareas:
  - TelemetryTask (UDP + WebSocket)
  - HttpServer (HTTP + OTA)
  - CameraDriver (captura VGA)
  - RecordingManager::writerTask (escritura a SD)
  - Prioridades bajas (2-5)
```

---

## 6. Degradación Graceful — Comportamiento Esperado

El sistema DEBE arrancar y funcionar aunque falte hardware:

| Hardware ausente | Comportamiento esperado |
|---|---|
| Sin DS3231 | Log warning + timestamps relativos desde boot. Sin crashes. |
| Sin SD Card | Log warning + grabación deshabilitada. Sin crashes. |
| Sin OV2640 | Log warning + clips solo térmicos. Sin crashes. |
| Sin PSRAM | Log error en F1 + RecordingManager::init() retorna ESP_ERR_NO_MEM. Sin crashes. |

Si el agente escribe código que crashea por ausencia de hardware opcional, **la etapa no está completa**.

---

## 7. UI Embebida — Restricciones Importantes

La UI está en `components/web_server/src/web_ui_html.h` como un string C.

```
Restricciones:
- Todo HTML + CSS + JS en un único string
- Sin archivos externos (no hay filesystem para servirlos hasta Fase D)
- Tamaño máximo recomendado: 80 KB sin comprimir
- Si supera 80 KB: aplicar gzip y servir con Content-Encoding: gzip

Cómo verificar tamaño:
$ wc -c components/web_server/src/web_ui_html.h
```

Si el agente necesita comprimir la UI:
```bash
# Comprimir a gzip y convertir a array C:
gzip -9 -c ui.html > ui.html.gz
xxd -i ui.html.gz > web_ui_html_gz.h
# Luego servir con: httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
```

---

## 8. Kalman y Separación de Blobs

### 8.1 Lo que está implementado

- `NoiseFilter` — Kalman 1D **por píxel** (ruido temporal del sensor)
- `FrameAccumulator` — fusión de sub-frames chess
- `TrackletTracker` — matching con costo compuesto (distancia + temperatura)

### 8.2 Limitación conocida: blobs solapados

Si dos personas están a menos de `NMS_RADIUS_CENTER_SQ` píxeles de distancia, el NMS puede fusionarlas en un solo peak. **No hay blob labeling implementado en este plan.**

Workaround disponible: reducir `NMS_RADIUS_CENTER_SQ` en la UI de calibración cuando se detecte el problema. Un blob labeling completo queda para una versión futura.

---

## 9. Qué Modelo Usar por Etapa

| Etapa | Modelo | Razón |
|---|---|---|
| A1 — Bugfix + chess + noise | **Claude Sonnet** | Bugs sutiles, razonamiento sobre código existente |
| A2 — Tracklet tracker | **Claude Sonnet** | Algoritmo con coherencia entre múltiples archivos |
| A3 — FSM puerta | **Gemini Flash** | Lógica clara y bien especificada, seguir instrucciones |
| A4 — UI colores/zonas | **Gemini Flash** | JS con instrucciones exactas |
| B1 — Líneas dibujables | **Gemini Pro o Sonnet** | JS canvas complejo + C++ backend, coherencia entre ambos |
| C1/D1/E1 — Drivers | **Gemini Flash** | Código casi completo en el plan, transcripción + adaptación |
| F1-F4 — Grabación | **Gemini Pro** | Muchos archivos interdependientes, contexto largo |
| G1 — Seguridad | **Gemini Flash** | Instrucciones claras, código sencillo |

---

## 10. Errores Frecuentes a Evitar

| Error | Consecuencia | Prevención |
|---|---|---|
| No hacer `fullclean` al tocar CMakeLists | Build errors crípticos | Siempre `fullclean` al agregar componentes |
| Usar `driver/i2c.h` (legacy) | No compila en IDF 6.0 | Usar `driver/i2c_master.h` |
| Buffer de cámara en SRAM | Crash por OOM | Siempre `CAMERA_FB_IN_PSRAM` |
| Escribir a SD desde Core 1 | Jitter en pipeline | Escritura SD solo desde Core 0 |
| `malloc()` sin `MALLOC_CAP_SPIRAM` | Buffer en SRAM, OOM | `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` |
| Modificar ThermalConfig globals sin mutex | Race condition | Usar configQueue para enviar al pipeline |
| Usar `delay()` en lugar de `vTaskDelay()` | Bloquea el scheduler | Siempre `vTaskDelay(pdMS_TO_TICKS(N))` |
| Hardcodear `15` en lugar de `MAX_TRACKS` | Bug silencioso | Usar siempre la constante |
| No verificar retorno de `xQueueSend` | Pérdida silenciosa de frames | Log warning si retorna `pdFALSE` |

---

## 11. Reporte al Finalizar Cada Etapa

Al terminar una etapa, el agente debe reportar exactamente:

```
ETAPA [nombre] — COMPLETADA

✅ Archivos creados:
  - [lista de archivos nuevos con ruta]

✅ Archivos modificados:
  - [lista de archivos modificados]

✅ Compilación: idf.py build — SIN ERRORES

✅ Criterio de éxito: [descripción de la prueba realizada y resultado]

⚠️ Desviaciones del plan (si las hay):
  - [qué se hizo diferente y por qué]

❌ Pendiente (si algo quedó sin hacer):
  - [qué y por qué]
```

---

*Versión 2.0 — Marzo 2026*  
*Leer antes de cada sesión de implementación*
