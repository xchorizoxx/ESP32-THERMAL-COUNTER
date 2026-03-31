# CLAUDE.md — AI Memory & Stewardship Guide
## Project: Thermal Door Detector (Alpha 0.6 Tactical Debug)

This document serves as the high-level technical "brain" for any AI agent working on the Thermal Door Detector project. It summarizes core architecture, strict coding standards, and critical hardware constraints.

---

### 1. System Architecture (Dual-Core ESP32-S3)
The system is partitioned across the two Xtensa LX7 cores to ensure real-time vision processing without networking jitters.

*   **Core 0 (Networking & UI):**
    - **SoftAP**: SSID `ThermalCounter` (IP: `192.168.4.1`).
    - **Web Server**: Serves the Tactical HUD (embedded HTML/JS/CSS).
    - **WebSockets**: Broadcasts thermal frames and tracks at 16 FPS.
    - **OTA Updates**: Handles wireless firmware flashing via `/update`.
*   **Core 1 (Thermal Pipeline):**
    - **Sensor Driver**: MLX90640 acquisition via I2C (400kHz).
    - **Vision Pipeline**: 5-step processing (Background -> Peaks -> NMS -> Tracking -> Counting).
    - **Timing**: Deterministic loop governed by `vTaskDelayUntil`.

---

### 2. Strict Technical Standards
Maintain these rules to ensure system stability and memory safety:

*   **Memory Management**: **Zero dynamic allocation at runtime.** All classes, buffers, and objects must be instantiated during system setup (or as static/global members) to prevent heap fragmentation.
*   **OOP & FreeRTOS**: Use C++ classes for all hardware/logic modules. Tasks inside classes must use a `static void TaskWrapper(void* pvParameters)` that casts the pointer back to `this` to call the instance method.
*   **Non-Blocking Logic**: Traditional `delay()` is forbidden. Use `vTaskDelay()` or `vTaskDelayUntil()`. Critical sections must avoid blocking the scheduler.
*   **Floating Point**: Single-precision `float` only (native S3 FPU support). **Avoid `double`** as it triggers software emulation and reduces performance.
*   **Logging**: Use `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` with a module-specific `TAG`. Avoid `printf` or `Serial.print`.
*   **ISR Safety**: Keep Interrupt Service Routines extremely brief. Use `IRAM_ATTR` and delegate work to tasks via notifications or queues.

---

### 3. Hardware Configuration
*   **Sensor**: MLX90640 (32×24 pixels, 110° FOV).
*   **I2C Pins**: Defined in `main.cpp` or `thermal_config.hpp`.
*   **Strapping Pins Warning**: Exercise extreme caution with GPIO 0, 2, 5, 12, 15 (Strapping) and Input-Only pins (34-39).

---

### System Architecture
1. **Acquisition**: MLX90640 (16Hz raw sub-frames).
2. **Pre-processing**: Chess Accumulator & 1D Kalman Noise Filter (8Hz full frames).
3. **Detection**: Background subtraction, Peak finding, and Multi-stage NMS.
4. **Tracking**: Alpha-Beta filter with identity persistence.
5. **Telemetry**: WebSocket dispatch to Web UI.

## Milestones
- [x] Initial sensor driver.
- [x] Basic alpha-beta tracking.
- [x] **Stage A1**: Stabilized pre-processing & Bugfixes.
- [ ] **Stage A2**: Tracklet-based tracking (Next).
- [ ] **Stage A3**: Finite State Machine (FSM) for directional counting.

---

### 4. Vision Pipeline Logic
1.  **Background Model**: Exponential Moving Average (EMA) with selective masking.
2.  **Peak Detector**: Finds local maxima > biological threshold and > background delta.
3.  **NmsSuppressor**: Adaptive suppression to handle lens distortion at the edges.
4.  **AlphaBetaTracker**: Predictive tracking to maintain person IDs across frames.
5.  **Counting Logic**: Hysteresis state machine using virtual entry/exit lines.

---

### 5. Build & Deployment
*   **Framework**: ESP-IDF v5.x (CMake).
*   **Build Command**: Guided by the user (do not run autonomously unless asked).
*   **OTA**: Flash binary via the Web HUD UI.

---

### 6. AI Deep Context & Knowledge Base
The `.agents/` directory is the core knowledge repository for AI assistants:
- **`.agents/context/`**: Contains extremely detailed technical guides, sensor data structures, and integration plans.
- **`.agents/workflows/`**: Automated protocols for hardware safety, system development, and project maintenance.
*Always refer to these files before making major structural or algorithmic changes.*

---

### 7. File Structure Reference
- `main/main.cpp`: Entry point, Wi-Fi init, and task spawning.
- `components/thermal_pipeline/`: Core algorithm logic.
- `components/web_server/`: HTTP/WS logic and `web_ui_html.h` (HUD).
- `components/mlx90640_driver/`: Thermal sensor communication.
- `components/telemetry/`: Data reporting over UART.
- `docs/`: Technical documentation and hardware datasheets.

---

### 8. Credits & Licensing
-   **Firmware License**: MIT License (for overall project).
-   **Sensor Driver**: **Melexis N.V.** (Apache 2.0). All rights reserved for the MLX90640 API.
-   **Author**: Developed as a high-efficiency thermal vision solution.

---
*Last Updated: March 20, 2026*
