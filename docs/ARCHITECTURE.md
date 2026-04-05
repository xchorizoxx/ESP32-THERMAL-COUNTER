# System Architecture

## Overview

Dual-core embedded system for person counting using Melexis MLX90640 thermal sensor (32×24 pixels). The ESP32-S3 runs two isolated main tasks: vision processing on Core 1 and communications on Core 0.

## Dual-Core Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         ESP32-S3                            │
├──────────────────────────┬──────────────────────────────────┤
│      Core 1 (APP)        │          Core 0 (PRO)            │
│   Priority: 24 (Max)     │      Priority: 2-5             │
├──────────────────────────┼──────────────────────────────────┤
│  ThermalPipeline         │  TelemetryTask                   │
│  ├── MLX90640 Driver     │  ├── SoftAP "ThermalCounter"     │
│  │   └── I2C 400kHz     │  ├── HTTP Server                 │
│  ├── FrameAccumulator    │  │   ├── WebSocket (binary)      │
│  ├── NoiseFilter         │  │   └── REST API                │
│  ├── BackgroundModel     │  └── UDP Broadcast (optional)    │
│  ├── PeakDetector        │                                  │
│  ├── NmsSuppressor       │  FreeRTOS Queue (IPC)            │
│  ├── TrackletTracker     │  └───────────────────────────────┘
│  └── TrackletFSM         │
└──────────────────────────┴──────────────────────────────────┘
```

### Hardware Abstraction

| Core | Task | Priority | Responsibility | Frequency |
| :--- | :--- | :--- | :--- | :--- |
| **APP_CPU (1)** | **ThermalPipe** | **MAX (24)** | MLX Acquisition + Vision Engine | **32 Hz** (sub-frames) / **16 Hz** (pipe) |
| **PRO_CPU (0)** | **TelemetryTX** | **Med (2)** | UDP broadcast + Web Server | Varies |

---

## Reliability & Hardening (Alpha 0.8 / Stage A3-B1)
The system has been hardened to ensure 24/7 industrial stability:

### 1. Task Monitoring (Watchdog)
A hardware Task Watchdog (WDT) monitors the **TelemetryTask**. If the network stack or server hangs for more than 5 seconds, the ESP32 performs an automatic reboot to recover the service.

### 2. Thread Safety (Concurrency)
To prevent race conditions between the vision engine (Core 1) and the web server (Core 0), the system uses:
- **Spinlocks**: The global configuration `door_lines` is protected by `ThermalConfig::door_lines_mux`. Any read or write to the virtual counting lines must enter a critical section.
- **IPC Queues**: Data from Core 1 is sent via a static queue to Core 0 to ensure safety.

### 3. I2C Fast-Mode Plus (1 MHz)
The MLX90640 is driven at **1,000,000 Hz** with 1kΩ pull-up resistors. This allows the acquisition task to retrieve sub-frames at 32 Hz, reducing motion blur and tracking jitter.

---

## Component Distribution

| Component | Responsibilities | Core |
| :--- | :--- | :--- |
| **UsbNetwork** | TinyUSB (ECM/RNDIS) bridge for Ethernet-over-USB. | 0 |
| **StatusLed** | System state visual indicator (NeoPixel RMT). | Any |
| **HttpServer** | Web UI host and API handler. | 0 |
| **WifiSoftAp** | WiFi access point management. | 0 |


---

## Connectivity Control Flow
1. **Startup**: WifiSoftAp starts; UsbNetwork remains **UNINITIALIZED** for hardware safety (OTG).
2. **Monitoring**: `BootBtnListener` task (Core 0) monitors GPIO 0.
3. **Trigger**: If button held for 2s:
   - `StatusLed` switches to Purple.
   - `UsbNetwork::init()` is called.
   - `HttpServer` becomes accessible via USB (192.168.4.1 / 192.168.7.1).
4. **Completion**: The listener task is deleted to save resources.

### Core 0: Network and Web (Best-effort)

Manages communications without interfering with vision pipeline.

- **SoftAP**: "ThermalCounter" @ 192.168.4.1
- **WebSocket**: Binary frames ~1.5 KB @ 16 FPS
- **UDP**: Optional telemetry broadcast (port 4210)
- **OTA**: POST `/update` endpoint for firmware updates

## Memory Management

**Total static allocation policy:**

- Task stacks pre-allocated at compile time (`xTaskCreateStatic`)
- FreeRTOS queues with fixed size (IPC_QUEUE_DEPTH = 4)
- Static image buffers (no malloc at runtime)
- Heap untouched during continuous operation (prevents fragmentation)

## Pipeline Details

### 1. Acquisition (16 Hz)

MLX90640 in "Chess" mode: even pixels in sub-frame 0, odd pixels in sub-frame 1. Each sub-frame has half the matrix, interleaved like a chessboard.

### 2. FrameAccumulator

Fusion of consecutive sub-frames into full 32×24 frames. Keeps most recent value per pixel regardless of sub-frame origin.

### 3. NoiseFilter (1D Kalman per pixel)

Individual scalar Kalman filter for each of the 768 pixels:
- Process: temperature approximately constant between frames
- Measurement: sensor NETD noise (~0.5°C typical)
- Output: filtered frame with attenuated noise

### 4. BackgroundModel (Selective EMA)

Exponential background update: `B(t) = α·I(t) + (1-α)·B(t-1)`

Where α = EMA_ALPHA (~0.05-0.10). Pixels marked in `blocking_mask_` (zones with active tracks) are excluded from update to prevent static persons from being absorbed into background.

### 5. Peak Detection

Local maxima detection on difference (frame - background):
1. Absolute temperature > `BIOLOGICAL_TEMP_MIN` (~25°C)
2. Contrast vs background > `BACKGROUND_DELTA_T` (~1.5-2.5°C)
3. Strict local maximum in 3×3 neighborhood

### 6. Adaptive NMS

Non-maximum suppression with variable radius:
- Lens center (x=8..23): larger radius (less distortion)
- Edges (x<8, x>23): smaller radius (more distortion)

Algorithm: sort by descending temperature, for each unsuppressed peak, eliminate neighbors within adaptive radius.

### 7. TrackletTracker (Stage A2)

Tracking based on 20-frame circular history:

**Data structure:**
```cpp
struct Tracklet {
    uint8_t id;                    // Persistent ID
    float x, y;                    // Current position
    float vx, vy;                  // Estimated velocity
    TrackHistory history;          // Circular 20-position buffer
    uint8_t confirm_count;          // Consecutive detection frames
    uint8_t missed_count;           // Frames without detection
    bool confirmed;                // Gate: requires 3 consecutive detections
}
```

**Composite matching:**
```
cost = distance_weight × euclidean_distance + 
       temp_weight × |temp_track - temp_peak|
```

Maximum match distance: `TRACK_MAX_DIST` (8 pixels).

**Proportional memory:**
- Young tracks (< 1s confirmed): 3-5 lost frames tolerance
- Mature tracks (> 3s): up to 12 frames tolerance (750ms)

### 8. TrackletFSM (Stage A3)

Finite state machine for bidirectional counting:

**Segment mode** (configurable lines):
- Supports up to 4 arbitrary line segments (not just horizontal)
- Crossing detection via vector cross product
- Debounce: maximum one IN and one OUT count per track per frame

**Legacy mode** (horizontal Y lines):
- States: UNBORN → TRACKING_IN ↔ TRACKING_OUT
- Lateral dead zones: exclusion by X coordinate

## Inter-Core Communication (IPC)

```cpp
struct IpcPacket {
    TelemetryPayload telemetry;    // Counters + tracks array
    ImagePayload image;          // 768 pixels × int16 (temp × 100)
    bool sensor_ok;              // MLX90640 sensor status
};
```

Core 1 produces → Queue (timeout 0, drop if full) → Core 0 consumes → WebSocket/UDP.

Packet size: ~1.6 KB. Queue depth: 4 elements (~6.4 KB total).

## References

- Pipeline implementation: `components/thermal_pipeline/`
- Sensor driver: `components/mlx90640_driver/`
- Network stack: `components/telemetry/` + `components/web_server/`
