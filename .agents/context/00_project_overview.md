# General Project Context — Thermal Door Detector

## System Description
A top-down person counter using an **MLX90640** thermal sensor (32×24 px, 110° FOV) mounted at ~3.6m over a door ~3–4.5m wide. An **ESP32-S3** (dual-core) runs a thermal computer vision pipeline and maintains tactical Web HUD interface, while also supporting wireless flashing (OTA).

## Dual-Core Architecture (FreeRTOS)
1. **Processing Pipeline:** (Core 1)
   - Extract ambient temperature via I2C at 400kHz.
   - Dynamically filter static background using EMA.
   - `IPC_QUEUE_DEPTH`: 4 (Optimized in A1 from 15 to save ~20KB SRAM).
   - `MAX_TRACKS`: 15 (Universal constant for array sizing).
   - `MAX_PEAKS`: 15 (Universal constant for array sizing).
   - ### Tracking System
   The `AlphaBetaTracker` class manages object detection and counts entries and exits based on predefined thermal thresholds.
   - Everything uses static memory (zero fragmentation).
   - `GET_CONFIG`: Returns current JSON configuration. Uses **portENTER_CRITICAL** to ensure atomic reads of internal `ThermalConfig` globals across Core 0/Core 1.

2. **Web UI, Network, and OTA:** (Core 0)
   - Creates a SoftAP ("ThermalCounter") at `192.168.4.1`.
   - Embedded HTTP server serving the Web UI (HTML Canvas 2D Dashboard) with `WebSockets` to broadcast live tracks and temperature matrix at 16 FPS.
   - Listens on POST `/update` for wireless firmware updates (Dual-Bank OTA).

## Milestones
- [x] **Stage A0**: Initial setup and connectivity.
- [x] **Stage A1**: Bugfixes & Thermal Pre-processing (Chess compositor, Kalman filter).
- [ ] **Stage A2**: Tracklet-based tracking (Next).

## Development Environment
- **Framework**: ESP-IDF v5.5 (CMake), pure C++.
- **Build system**: `CMakeLists.txt` in root with binaries in `build/`.
- **Hardware Target**: `esp32s3`.

## Strict Rules for Agents (YOU)
1. **Zero `malloc`/`new` at runtime** — Only allocate objects during initialization (setup).
2. **Do not use `delay()`** — ALWAYS use `vTaskDelay()` or `vTaskDelayUntil()`.
3. **Floating Point**: Use `float` (native single-precision FPU), NEVER use `double`.
4. **Logs**: Use `ESP_LOGI`, `ESP_LOGE` etc., with a generic static `TAG` macro.
5. **Do not take control of the Build process**: NEVER run `idf.py build` or `flash` in terminal without being asked. Simply write the code and notify the user to recompile using their VS Code extension.
6. **Object-Oriented Programming**: FreeRTOS tasks as class methods using `static void TaskWrapper(void*)` and a `this` pointer cast.
7. If the user asks to change I2C speed, pins, or clock, review the **hardware-safety** protocol first.
