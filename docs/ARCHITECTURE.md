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

### Core 1: Vision Pipeline (16 Hz)

Deterministic execution with maximum priority. Never blocks on network operations.

| Stage | Function | Output |
|-------|----------|--------|
| **Acquisition** | MLX90640 I2C read in Chess mode | Alternating sub-frames |
| **Pre-processing** | FrameAccumulator + NoiseFilter (1D Kalman) | Filtered 32×24 frame |
| **Background Modeling** | Selective EMA with feedback mask | Updated background map |
| **Detection** | Peak detection + adaptive NMS | List of thermal peaks |
| **Tracking** | TrackletTracker (20-frame history) | Tracks with velocity |
| **Counting** | TrackletFSM with configurable lines | IN/OUT counters |

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
