# Thermal Door Counter — ESP32-S3 + MLX90640

Embedded dual-core person counting system using thermal vision (32×24 pixels). Core 1 runs deterministic 16 Hz vision pipeline; Core 0 handles WiFi/Web/OTA.

---

## Quick Reference

| Parameter | Value |
|-----------|-------|
| **Platform** | ESP32-S3 (Xtensa LX7 dual-core @ 240MHz) |
| **Sensor** | Melexis MLX90640 (32×24 thermopile, I2C 400kHz) |
| **Framework** | ESP-IDF v5.5, FreeRTOS, C++ |
| **Pipeline** | 16 Hz acquisition → 8 Hz processing |
| **Current Stage** | A3 (TrackletFSM with configurable segments) |

---

## Universal Rules (Apply to Every Task)

### 1. Memory Management
- **Zero `malloc`/`new` at runtime.** All objects instantiated during init or as static members.
- Pre-allocated buffers only: `composed_frame_[768]`, `filtered_frame_[768]`, etc.
- FreeRTOS tasks use `xTaskCreateStatic` with pre-allocated stacks.

### 2. Timing & Concurrency
- **Never use `delay()`.** Always `vTaskDelay()` or `vTaskDelayUntil()`.
- Core 1 (Vision): Priority 24, never blocks on network.
- Core 0 (Network): Priority 2-5, best-effort.
- Critical sections: Use `portENTER_CRITICAL` + `portEXIT_CRITICAL`, keep brief.

### 3. Data Types
- **Use `float` only** (native S3 FPU). **Never `double`** (software emulation, slow).
- Fixed-point for network: `int16_t temp_x100` for packed structs.

### 4. Logging
- Use `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` with module-specific `static const char* TAG`.
- Never `printf` or `Serial.print`.

### 5. ISR Safety
- ISRs must be brief with `IRAM_ATTR`.
- Delegate work to tasks via queues/notifications.

### 6. Build Protocol
- **Do NOT run `idf.py build` or `flash` without explicit user permission.**
- Write code, then notify user to recompile via VS Code extension.

---

## Current Implementation

**Stage A3 (Active):**
- TrackletTracker: 20-frame circular history, composite matching (distance + temperature)
- TrackletFSM: Bidirectional counting with up to 4 configurable line segments
- Per-track debounce: Max 1 IN + 1 OUT per frame

## Detailed Documentation

| Topic | Location |
|-------|----------|
| System Architecture | [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — Dual-core design, IPC, pipeline 6 stages |
| Tracking Algorithms | [`docs/ALGORITHM.md`](docs/ALGORITHM.md) — TrackletTracker math, TrackletFSM logic |
| Calibration Guide | [`docs/CONFIGURATION.md`](docs/CONFIGURATION.md) — Web UI parameters, thresholds |
| Hardware Specs | [`docs/HARDWARE.md`](docs/HARDWARE.md) — Pinout, electrical requirements, I2C |
| Operations & OTA | [`docs/OPERATIONS.md`](docs/OPERATIONS.md) — Deployment, troubleshooting, maintenance |
| Quick Pipeline Ref | [`.agents/context/vision_pipeline.md`](.agents/context/vision_pipeline.md) — IA quick reference |

---

## File Structure

```
components/
├── mlx90640_driver/       # I2C sensor driver
├── thermal_pipeline/      # Core 1: Vision pipeline
│   ├── src/tracklet_tracker.cpp    # Stage A2
│   ├── src/tracklet_fsm.cpp        # Stage A3
│   └── deprecated/                 # Legacy Alpha-Beta
telemetry/                 # Core 0: UDP/WebSocket
web_server/                # HTTP + OTA handler

docs/
├── ARCHITECTURE.md        # Full system design
├── ALGORITHM.md           # Tracking & counting algorithms
├── CONFIGURATION.md       # Calibration parameters
├── HARDWARE.md            # Electrical specifications
├── OPERATIONS.md          # OTA & troubleshooting
├── README.md / README_ES.md  # User documentation
└── reference/             # Datasheets & extensions

CLAUDE.md                  # This file — AI quick reference
.agents/context/           # IA context (minimal)
```

---

## License

- **Project**: MIT License
- **MLX90640 Driver**: Apache 2.0 (Melexis N.V.)

---

*Last Updated: March 30, 2026*
