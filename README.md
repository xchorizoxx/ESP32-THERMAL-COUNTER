# Thermal Door Counter — ESP32-S3 + MLX90640

Embedded person counting system using thermal vision (32×24 pixels). Zero optical cameras: 100% privacy, works in total darkness.

## Technical Specifications

| Parameter | Value |
|-----------|-------|
| Sensor | Melexis MLX90640 (32×24 thermopile, 110° FOV) |
| Processor | ESP32-S3 dual-core @ 240MHz |
| Acquisition | 16 Hz (sub-frames) |
| Processing | 8 Hz (full composed frames) |
| Architecture | Core 1 (Vision) + Core 0 (Network/Web) |
| Tracking | TrackletTracker with 20-frame circular history (Stage A2) |
| Counting | TrackletFSM with configurable line segments (Stage A3) |
| Interface | Web UI via SoftAP (192.168.4.1) |
| Updates | OTA via `/update` endpoint |

## Software Architecture

```
[Core 1] ThermalPipeline (priority 24, 16 Hz)
  ├── MLX90640 Driver (I2C 400kHz, Fast Mode)
  ├── FrameAccumulator (Chess mode sub-frame fusion)
  ├── NoiseFilter (1D Kalman per pixel)
  ├── BackgroundModel (selective EMA update)
  ├── PeakDetector (local maxima detection)
  ├── NmsSuppressor (adaptive radius: center vs edges)
  ├── TrackletTracker (20-frame history, composite matching)
  └── TrackletFSM (bidirectional counting, dead zones)

[Core 0] TelemetryTask + HTTP Server (priority 2-5)
  ├── WiFi SoftAP "ThermalCounter"
  ├── Binary WebSocket (1.5 KB/frame, 16 FPS)
  ├── UDP broadcast (optional, port 4210)
  └── OTA handler (/update)

IPC: FreeRTOS Queue (depth 4, static allocation)
```

## Quick Start

1. **Hardware**: Connect ESP32-S3 to MLX90640 via I2C (GPIO 8/9, 400kHz)
2. **Flash**: VS Code + ESP-IDF extension → "Build, Flash and Monitor"
3. **Connect**: Join WiFi "ThermalCounter" / password: `counter1234`
4. **Configure**: Open http://192.168.4.1 → adjust thresholds → Save to Flash

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for detailed system design.

## Critical Configuration Parameters

| Parameter | Description | Typical Range |
|-----------|-------------|---------------|
| Biological Temp | Human temperature threshold | 25-30°C |
| Background Delta T | Contrast vs learned background | 1.5-2.5°C |
| EMA Alpha | Background adaptation speed | 0.05-0.10 |
| NMS Radius Center | Suppression radius (center zone) | 4-8 pixels |
| NMS Radius Edge | Suppression radius (edge zones) | 2-4 pixels |
| Dead Zone Left/Right | Horizontal exclusion zones | 0-8 pixels |

Calibration guide: [`docs/CONFIGURATION.md`](docs/CONFIGURATION.md)

## Vision Pipeline (5 Stages)

**Stage 1 — Acquisition**: MLX90640 in Chess mode, 16 Hz raw sub-frames (alternating even/odd pixels).

**Stage 2 — Pre-processing**: 
- FrameAccumulator: Fuses sub-frames into full 32×24 frames
- NoiseFilter: Per-pixel 1D Kalman (reduces NETD noise)

**Stage 3 — Background Modeling**: Selective EMA update. Pixels under active tracks are frozen to prevent absorption.

**Stage 4 — Detection**: 
- Peak detection: Local maxima above `BIOLOGICAL_TEMP_MIN` with `BACKGROUND_DELTA_T` contrast
- NMS: Adaptive radius (larger in center where distortion is lower)

**Stage 5 — Tracking & Counting**:
- TrackletTracker: 20-frame position history for velocity estimation
- Composite matching: Distance + temperature similarity
- TrackletFSM: Bidirectional counting with configurable line segments

Algorithm details: [`docs/ALGORITHM.md`](docs/ALGORITHM.md)

## Hardware Connections

| MLX90640 | ESP32-S3 | Note |
|----------|----------|------|
| VCC | 3.3V | Stable LDO required (150mA peak with WiFi) |
| GND | GND | Short ground path |
| SDA | GPIO 8 | 1kΩ-2.2kΩ pull-up for 400kHz |
| SCL | GPIO 9 | 1kΩ-2.2kΩ pull-up for 400kHz |

Full pinout: [`docs/HARDWARE.md`](docs/HARDWARE.md)

## OTA Updates

```bash
# Via Python script (connected to ThermalCounter WiFi)
python scripts/ota_upload.py

# Via Web UI
# Open http://192.168.4.1 → OTA panel → Upload build/DetectorPuerta.bin
```

Operations guide: [`docs/OPERATIONS.md`](docs/OPERATIONS.md)

## Documentation Index

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — System architecture and dual-core design
- [`docs/ALGORITHM.md`](docs/ALGORITHM.md) — TrackletTracker and TrackletFSM algorithms
- [`docs/CONFIGURATION.md`](docs/CONFIGURATION.md) — Calibration parameters and Web UI guide
- [`docs/HARDWARE.md`](docs/HARDWARE.md) — Pinout, connections, and electrical specs
- [`docs/OPERATIONS.md`](docs/OPERATIONS.md) — OTA, deployment, and maintenance

## Project Structure

```
├── components/
│   ├── mlx90640_driver/     # Melexis sensor driver (I2C)
│   ├── thermal_pipeline/      # Vision pipeline (Core 1)
│   ├── telemetry/             # Network stack (Core 0)
│   └── web_server/            # HTTP server + WebSocket
├── docs/
│   ├── assets/                # Screenshots and demo videos
│   ├── ARCHITECTURE.md
│   ├── ALGORITHM.md
│   ├── CONFIGURATION.md
│   ├── HARDWARE.md
│   └── OPERATIONS.md
├── scripts/
│   └── ota_upload.py          # OTA flash utility
├── main/
│   └── main.cpp               # Entry point, task creation
└── README.md / README_EN.md   # This file (ES/EN)
```

## Changelog

- **Stage A3** (Current): TrackletFSM with configurable line segments, debounce logic
- **Stage A2**: TrackletTracker (20-frame history, composite matching, proportional memory)
- **Stage A1**: Kalman filter per pixel, Chess accumulator, staged pipeline
- **Stage A0**: Initial MVP

## License

- **Project**: MIT License
- **MLX90640 Driver**: Apache 2.0 (Melexis N.V.)

---

*Spanish version: [README_ES.md](README_ES.md)*
