# Thermal Door Counter — AGENTS.md

ESP32-S3 dual-core person counter using MLX90640 (32×24 thermopile). ESP-IDF 6.0.0, FreeRTOS, C++.

**CRITICAL: AGENTS.md is the primary source of truth.**

---

## Ground Truth

| Fact | Value | Source |
|------|-------|--------|
| ESP-IDF version | **6.0.0** | `dependencies.lock`, `sdkconfig` |
| Pipeline frequency | **32 Hz** | `thermal_config.hpp:112` |
| CPU frequency | **160 MHz** (debug) | `sdkconfig` |
| LibC | **picolibc** | `sdkconfig` |
| Partition table | **partitions_4mb.csv** (4MB OTA, universal) | `sdkconfig.defaults` |
| All components path | `components/` (NOT top-level) | directory listing |
| RTC I2C bus | **I2C1** (GPIO 6/7), NOT shared with MLX | `thermal_config.hpp:133-134` |
| SD card SPI pins | MOSI=13, MISO=14, SCK=12, CS=11 | `thermal_config.hpp:138-141` |

---

## Build Commands

```bash
# Normal code change — just build
idf.py build

# After modifying ANY CMakeLists.txt or adding a component
idf.py fullclean && idf.py build

# After editing idf_component.yml to add a new managed dependency
idf.py update-dependencies && idf.py fullclean && idf.py build
```

**Do NOT run `idf.py build`, `idf.py flash`, or `idf.py monitor` without explicit user permission.**

The VS Code ESP-IDF extension is configured at `~/.espressif/v6.0/esp-idf`, target `esp32s3`, flash via UART `/dev/ttyACM0`.

---

## Universal Rules

1. **Zero `malloc`/`new` at runtime.** All objects instantiated during init. Tasks use `xTaskCreateStatic`.
2. **Never `delay()`.** Always `vTaskDelay()` or `vTaskDelayUntil()`.
3. **Float only, never `double`** (S3 FPU is float-only; double is software emulation).
4. **Log with `ESP_LOGI`/`ESP_LOGW`/`ESP_LOGE`**, never `printf`/`std::cout`. Each module has `static const char* TAG`.
5. **ISRs must be `IRAM_ATTR`, short, delegate work via queues/notifications.**
6. **PSRAM for large buffers:** `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. Verify `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)` before allocating.
7. **Use `door_lines_mux` spinlock** when reading/writing `ThermalConfig::door_lines` across cores.

---

## Architecture

### Core Assignment

| Core | Tasks | Priority |
|------|-------|----------|
| Core 1 (APP_CPU) | ThermalPipeline (vision pipeline) | `configMAX_PRIORITIES - 1` |
| Core 0 (PRO_CPU) | TelemetryTask (IPC consumer, WebSocket broadcast) | `tskIDLE_PRIORITY + 2` |
| Core 0 (PRO_CPU) | HttpServer (HTTP + WebSocket handler) | — |
| Core 0 (PRO_CPU) | PeriphWatchdog (SD/RTC reconnect every 5 min) | `tskIDLE_PRIORITY + 1` |

Core 1 MUST NOT perform network, SD, or camera operations. Core 0 handles all I/O.

### IPC

Two static queues (no heap allocation):
- `ipcQueue` (depth 8): Core 1 → Core 0. Carries `IpcPacket` (frame data + events).
- `configQueue` (depth 10): Core 0 → Core 1. Carries `AppConfigCmd` (parameter updates from web UI).

### Boot Sequence (main.cpp:app_main)
NVS → Watchdog → SD init → RTC init (I2C1, GPIO 6/7) → WiFi SoftAP → MLX90640 (I2C0, GPIO 8/9, 1MHz) → IPC queues → ThermalPipeline task (Core 1) → HttpServer → TelemetryTask (Core 0) → OTA rollback cancel → PeriphWatchdog

### Task Stacks
- ThermalPipeline: 6144 bytes
- TelemetryTask: 3584 bytes
- PeriphWatchdog: 2560 bytes

---

## I2C / IDF 6.0 API

The IDF 6.0 I2C API is different from IDF 5.x. Always use:

```cpp
#include "driver/i2c_master.h"   // ✅ Correct for IDF 6.0

// ❌ NEVER use driver/i2c.h — removed in 6.0
```

The MLX90640 uses `I2C_NUM_0` at 1MHz FM+. The RTC uses `I2C_NUM_1` at standard speed.

---

## Pinout (source of truth: `thermal_config.hpp`)

```
I2C0 (MLX90640):  SDA=GPIO_8,  SCL=GPIO_9,  addr=0x33,  1MHz
I2C1 (DS3231):    SDA=GPIO_7,  SCL=GPIO_6,  VCC=GPIO_15, GND=GPIO_16
SPI2 (SD):        MOSI=13, MISO=14, SCK=12, CS=11
```

Strapping pins (ESP32-S3, never use for logic): GPIO 0, 3, 45, 46

---

## WebSocket Protocol

Binary frames use magic `0x12`, protocol v2. Contains thermal image + track positions + counters. JSON events for crossing (IN/OUT). Worker task uses pool of 4 static 4096-byte buffers with ref-counting and 2-second watchdog recovery.

---

## Disabled Features

- **UDP Transmitter**: Disabled. WebSocket replaces UDP for all data. Source code at `components/telemetry/src/udp_transmitter.cpp` kept for reference but not compiled. History: UDP broadcasts to 255.255.255.255 saturated the TinyUSB NCM driver and broke critical network traffic.

## Key Files

| File | Purpose |
|------|---------|
| `thermal_config.hpp` | ALL configurable parameters, pinout, thresholds |
| `thermal_types.hpp` | Shared data structures (Peak, Track, IpcPacket, ConfigCmd) |
| `thermal_pipeline.hpp` | Pipeline orchestrator, 5-stage vision loop |
| `tracklet_tracker.hpp` | Stage A2: Hungarian matching, 20-frame history |
| `tracklet_fsm.hpp` | Stage A3: line-segment crossing FSM, bidirectional counting |
| `http_server.hpp` | HTTP endpoints + WebSocket + OTA handler |
| `main.cpp` | System entry point, task creation, boot sequence |

---

## Graceful Degradation

System must boot and run even with missing optional hardware:
- No DS3231 → log warning, use relative timers
- No SD card → log warning, disable recording
- No OV2640 → log warning, thermal-only clips
- No PSRAM → `init()` returns `ESP_ERR_NO_MEM`, no crash

## Implementation Workflow (OBLIGATORIO)

Cada fase sigue este ciclo exacto — saltarse pasos causa errores:

1. **Plan** → Presentar plan, esperar aprobación explícita del usuario
2. **Implement** → Hacer cambios en archivos
3. **Report** → Decir "listo, probar con build"
4. **Wait** → USUARIO compila y reporta errores
5. **Fix** → Corregir errores reportados UNO POR UNO
6. **Repeat 4-5** → Hasta que compile sin errores
7. **Confirm** → Esperar confirmación del usuario para pasar a SIGUIENTE fase

**NUNCA ejecutar `idf.py build/flash/clean/monitor`.**  
**NUNCA pasar a la siguiente fase sin confirmación explícita del usuario.**  
**NUNCA implementar dos fases seguidas sin compilación intermedia.**

---

## References

- `.agents/plans/` — Spanish operational blueprint with stage map
- `.agents/workflows/hardware-safety.md` — hardware modification approval protocol
- `sdkconfig.defaults` — Kconfig overrides
