# System and Network Architecture

This document details the technical infrastructure that allows the ESP32-S3 to simultaneously handle the heavy thermal vision pipeline and web communication.

## 1. Multi-Core Design (Asymmetric)

The ESP32-S3 has two cores (Core 0 and Core 1). We have assigned tasks to maximize determinism:

*   **Core 1 (APP_CPU):** Runs the `ThermalPipe` task. It is a critical real-time task. Its priority is maximum (24) to ensure the I2C bus never waits.
*   **Core 0 (PRO_CPU):** Runs the `TelemetryTask` and the `HTTP_SERVER`. It handles the WiFi and TCP/IP network stack.

### Inter-Core Communication (IPC)
To pass data from Core 1 to Core 0 safely without blocking (Race Conditions), we use **FreeRTOS Queues**.
1. Core 1 generates an `IpcPacket`.
2. It sends it to the `ipcQueue` with a timeout of 0 (if the queue is full, the frame is discarded to not delay vision).
3. Core 0 receives the packet, packages it into binary, and sends it via WebSocket.

## 2. Binary WebSockets Protocol

To save bandwidth and CPU, we do not use JSON to send the thermal image (which are 768 floating-point numbers). Instead, we send a **Binary Buffer**.

### Frame Structure (Little-Endian):
| Position | Type | Description |
|----------|------|-------------|
| 0 | `uint8` | Header (`0xAA`) |
| 1 | `uint8` | Sensor OK (0/1) |
| 2-5 | `float32` | Ambient Temperature (`Ta`) |
| 6-7 | `uint16` | Entry Count |
| 8-9 | `uint16` | Exit Count |
| 10 | `uint8` | Number of active tracks (`N`) |
| 11+ | `TrackInfo[N]` | Array of peaks (ID, X, Y, VX, VY) |
| final | `int16[768]` | Thermal intensity (Temp * 100) |

## 3. Configuration Persistence (NVS)

We use the ESP32's **Non-Volatile Storage (NVS)** system to save calibration parameters.
- Values are saved in the `"thcfg"` namespace.
- Floats (like `EMA_ALPHA`) are saved as `int32_t` scaled by 1000, as NVS does not natively support floats.
- **Lifecycle:** Upon startup (`vTaskStartScheduler`), Core 0 loads values from NVS and sends them to Core 1. When the user clicks "Save" on the web, Core 0 writes to flash and notifies the vision core.

## 4. Integrated Web Server

The web server runs on the `esp_http_server` stack.
- **Memory:** HTML/JS/CSS code is embedded in the binary (`web_ui_html.h`) as compressed text constants. It does not depend on an SD card.
- **Concurrency:** Supports up to 4 simultaneous clients, although connecting only 1 or 2 is recommended to not saturate the ESP32-S3's RAM.
