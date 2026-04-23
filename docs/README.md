# Project Documentation

This index organizes the technical documentation for the Thermal Door Counter.

## Main Documents

| Document | Content | Audience |
|----------|---------|----------|
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Dual-core architecture, vision pipeline, IPC, memory management | Developers, system architects |
| [`ALGORITHM.md`](ALGORITHM.md) | TrackletTracker and TrackletFSM algorithms (Stage A2/A3) | CV developers, researchers |
| [`CONFIGURATION.md`](CONFIGURATION.md) | Calibration guide, Web UI parameters, troubleshooting | Installers, field technicians |
| [`HARDWARE.md`](HARDWARE.md) | Electrical connections, sensor specifications, mounting | Hardware engineers, installers |
| [`STATUS_INDICATORS.md`](STATUS_INDICATORS.md) | RGB LED codes, USB activation guide, power safety | Users, developers |
| [`OPERATIONS.md`](OPERATIONS.md) | Deployment, OTA, maintenance, monitoring | DevOps, maintenance technicians |

## Repository Structure

```
├── components/
│   ├── mlx90640_driver/       # Melexis sensor I2C driver
│   ├── thermal_pipeline/      # Vision pipeline (Core 1)
│   │   ├── include/thermal_config.hpp     # Configurable parameters
│   │   ├── src/tracklet_tracker.cpp       # Tracker Stage A2
│   │   ├── src/tracklet_fsm.cpp           # FSM Stage A3
│   │   └── deprecated/                    # Legacy code (Alpha-Beta)
│   ├── telemetry/             # Network stack + WiFi (Core 0)
│   └── web_server/            # HTTP server + WebSocket + OTA
├── docs/
│   ├── ARCHITECTURE.md        # This index
│   ├── ALGORITHM.md
│   ├── CONFIGURATION.md
│   ├── HARDWARE.md
│   ├── OPERATIONS.md
│   └── assets/                # Screenshots, demo videos
├── scripts/
│   └── ota_upload.py          # OTA utility
├── main/
│   └── main.cpp               # Entry point, task creation
├── README.md                  # User documentation (EN)
└── README_ES.md               # User documentation (ES)
```

## Documentation Flow

**New user →** [`README.md`](../README.md) → [`CONFIGURATION.md`](CONFIGURATION.md) → [`OPERATIONS.md`](OPERATIONS.md)

**Developer →** [`README.md`](../README.md) →
- [ARCHITECTURE.md](ARCHITECTURE.md): High-level system design.
- [WEBSERVER_SPECS.md](WEBSERVER_SPECS.md): Detailed Web Server, API, and WebSocket protocol.
- [DATA_PERSISTENCE.md](DATA_PERSISTENCE.md): Storage logic, NVS, and SD CSV format.
- [PERIPHERALS_DETAIL.md](PERIPHERALS_DETAIL.md): Hardware drivers, pinout, and status LED.
- [ALGORITHM.md](ALGORITHM.md): Vision pipeline and tracking logic.

**Field installer →** [`README.md`](../README.md) → [`HARDWARE.md`](HARDWARE.md) → [`CONFIGURATION.md`](CONFIGURATION.md)

## AI Agent Context

Files in `.agents/context/` are optimized summaries for AI assistants:

- `00_project_overview.md` — Overview and milestones
- `01_sensor_driver.md` — MLX90640 driver
- `02_types_and_config.md` — Structures and configuration
- `03_vision_pipeline.md` — Vision pipeline
- `04_telemetry.md` — Communications
- `05_integration.md` — System integration
- `06_web_server.md` — Web server
- `07_ota_system.md` — OTA system

These documents may be outdated; the source of truth are the files in this directory (`docs/`).

## Documentation Versioning (Alpha 0.8)

Documentation is versioned along with the code milestones:

- **Stage A0**: Initial MVP.
- **Stage A1**: Pipeline with Kalman, Chess accumulator.
- **Stage A2**: TrackletTracker (documented in [`ALGORITHM.md`](ALGORITHM.md)).
- **Stage A3**: TrackletFSM with configurable lines.
- **Stage A3-B1 (Current)**: Refinement & Hardening (FIX). Includes USB Network, RGB LED, Sub-pixel moments, and Physical FOV Correction.

See [`CHANGELOG.md`](../CHANGELOG.md) for detailed history and the **FIX** folder for the implementation phases.

## Legal Notes

- Project: MIT License
- MLX90640 Driver: Apache 2.0 (Copyright Melexis N.V.)
- MLX90640 Datasheet: `docs/reference/MLX90640_Datasheet.md`
