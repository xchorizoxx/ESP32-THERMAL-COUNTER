# Thermal Door Counter — ESP32-S3 + MLX90640

Embedded person counting system using thermal vision (32×24 pixels). Zero optical cameras: 100% privacy, works in total darkness.

## Technical Specifications

| Parameter | Value |
|-----------|-------|
| Sensor | Melexis MLX90640 (32×24 thermopile, 110° FOV) |
| Processor | ESP32-S3 dual-core @ 240MHz |
| Acquisition | 32 Hz (sub-frames) |
| Processing | 16 Hz (full composed frames) |
| Architecture | Core 1 (Vision) + Core 0 (Network/Web) |
| Tracking | TrackletTracker with 20-frame circular history (Stage A2) |
| Counting | TrackletFSM with configurable line segments (Stage A3) |
| Interface | Web UI via SoftAP (192.168.4.1) |
| Storage | MicroSD (FATFS on SPI2) |
| Real Time Clock | RTC DS3231 (I2C1) with battery backup |
| Flash | 16MB (4MB App Partitions) |
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
  └── TrackletFSM (bidirectional counting, dead zones)

[Core 0] TelemetryTask + HTTP Server (priority 2-5)
  ├── WiFi SoftAP "ThermalCounter"
  ├── Binary WebSocket (16 FPS)
  ├── RTC Driver (DS3231 vs Soft-clock Sync)
  ├── SD Manager (CSV Event Logging on MicroSD)
  └── Health Monitor (Real-time HW status)

IPC: FreeRTOS Queue (depth 4, static allocation)
```

## Main Features
- **Sub-pixel Detection**: Heat centroids calculated with sub-pixel precision for smooth tracking.
- **Dual Connectivity**: WiFi (SoftAP) and **USB (RNDIS/ECM)** for direct local access.
- **Visual Feedback**: RGB LED (GPIO 48) for system status (Blue: Booting, Green: Operating, Purple: USB Mode).
- **Responsive Web UI**: Adaptive layout for mobile and PC monitors (Desktop-optimized grid).
- **Privacy First**: 100% local processing; no images ever leave the device.
- **Dynamic Config**: Adjustable thresholds via web panel, persisted to NVS flash.

## Quick Start
1. **Hardware**: Connect ESP32-S3 to MLX90640 via I2C (GPIO 8/9).
2. **BOOT Button**: Hold for 2s to activate **USB Network Mode**.
3. **Flash**: VS Code + ESP-IDF extension → "Build, Flash and Monitor".
4. **Connect**: Join WiFi "ThermalCounter" or use USB cable (IP: 192.168.4.1).
5. **Config**: Open http://192.168.4.1 in your browser → adjust thresholds → Save to Flash.

See [`docs/01-architecture.md`](docs/01-architecture.md) for detailed system design.

## Critical Configuration Parameters

| Parameter | Description | Typical Range |
|-----------|-------------|---------------|
| Biological Temp | Human temperature threshold | 25-30°C |
| Background Delta T | Contrast vs learned background | 1.5-2.5°C |
| EMA Alpha | Background adaptation speed | 0.05-0.10 |
| NMS Radius Center | Suppression radius (center zone) | 4-8 pixels |
| NMS Radius Edge | Suppression radius (edge zones) | 2-4 pixels |
| Dead Zone Left/Right | Horizontal exclusion zones | 0-8 pixels |

Calibration guide: [`docs/04-configuration.md`](docs/04-configuration.md)

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

Algorithm details: [`docs/02-algorithm.md`](docs/02-algorithm.md)

## Hardware Connections

| Bus | Signal | GPIO | Note |
|-----|--------|------|------|
| I2C0 (MLX90640) | SDA | GPIO 8 | 1 MHz FM+, external 1kΩ pull-ups |
| I2C0 (MLX90640) | SCL | GPIO 9 | 1 MHz FM+, external 1kΩ pull-ups |
| I2C1 (DS3231) | SDA | GPIO 7 | Standard speed |
| I2C1 (DS3231) | SCL | GPIO 6 | Standard speed |
| I2C1 (DS3231) | VCC | GPIO 15 | RTC power control |
| I2C1 (DS3231) | GND | GPIO 16 | RTC ground switch |
| SPI2 (SD) | MOSI | GPIO 13 | MicroSD |
| SPI2 (SD) | MISO | GPIO 14 | MicroSD |
| SPI2 (SD) | SCK | GPIO 12 | MicroSD |
| SPI2 (SD) | CS | GPIO 11 | MicroSD |

Full pinout: [`docs/03-hardware.md`](docs/03-hardware.md)

## OTA Updates

```bash
# Via Python script (connected to ThermalCounter WiFi)
python scripts/ota_upload.py

# Via Web UI
# Open http://192.168.4.1 → OTA panel → Upload build/DetectorPuerta.bin
```

Operations guide: [`docs/06-operations.md`](docs/06-operations.md)

## Documentation Index

- [`docs/01-architecture.md`](docs/01-architecture.md) — System architecture and dual-core design
- [`docs/02-algorithm.md`](docs/02-algorithm.md) — TrackletTracker and TrackletFSM algorithms
- [`docs/03-hardware.md`](docs/03-hardware.md) — Pinout, connections, and electrical specs
- [`docs/04-configuration.md`](docs/04-configuration.md) — Calibration parameters and Web UI guide
- [`docs/05-webserver.md`](docs/05-webserver.md) — HTTP API, WebSocket protocol, OTA
- [`docs/06-operations.md`](docs/06-operations.md) — OTA, deployment, and maintenance
- [`docs/07-peripherals.md`](docs/07-peripherals.md) — Peripheral drivers detail
- [`docs/08-data-persistence.md`](docs/08-data-persistence.md) — Storage, NVS, SD CSV format
- [`docs/09-status-indicators.md`](docs/09-status-indicators.md) — RGB LED codes and USB guide

## Project Structure

```
├── .agents/                   # AI instructions and plans
├── components/
│   ├── mlx90640_driver/       # Melexis sensor driver (I2C0)
│   ├── rtc_driver/            # DS3231 RTC driver (I2C1)
│   ├── sd_manager/            # SD card FATFS manager
│   ├── status_led/            # RGB LED driver
│   ├── telemetry/             # Network stack, LogWriter (Core 0)
│   ├── thermal_pipeline/      # Vision pipeline (Core 1)
│   ├── thermal_recorder/      # Thermal clip recorder
│   └── web_server/            # HTTP + WebSocket + OTA
├── docs/
│   ├── assets/                # Screenshots and demo videos
│   │   ├── images/
│   │   └── videos/
│   ├── 01-architecture.md
│   ├── 02-algorithm.md
│   ├── 03-hardware.md
│   ├── 04-configuration.md
│   ├── 05-webserver.md
│   ├── 06-operations.md
│   ├── 07-peripherals.md
│   ├── 08-data-persistence.md
│   ├── 09-status-indicators.md
│   ├── CHANGELOG.md
│   ├── README.md              # Docs index
│   └── ROADMAP.md
├── managed_components/        # ESP-IDF dependencies
├── scripts/
│   └── ota_upload.py          # OTA flash utility
├── main/
│   └── main.cpp               # Entry point, task creation
└── README.md / README_ES.md   # This file (ES/EN)
```

## Changelog

- **v1.0.0-estable** (Current): Session-based file organization, CSV restructure, config export, client-side ZIP downloads, enhanced clip management, docs restructured.
- **v0.9.5-alpha**: Dual-core architecture, SD logging, OTA, USB network mode.
- **v0.8.1-alpha**: TrackletFSM, configurable counting lines, NVS persistence.

## License

- **Project**: MIT License
- **MLX90640 Driver**: Apache 2.0 (Melexis N.V.)

---

*Spanish version: [README_ES.md](README_ES.md)*
