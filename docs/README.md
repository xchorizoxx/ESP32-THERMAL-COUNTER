# Project Documentation

This index organizes the technical documentation for the Thermal Door Counter.

## Main Documents

| Document | Content | Audience |
|----------|---------|----------|
| [`01-architecture.md`](01-architecture.md) | Dual-core architecture, vision pipeline, IPC, memory management | Developers, system architects |
| [`02-algorithm.md`](02-algorithm.md) | TrackletTracker and TrackletFSM algorithms (Stage A2/A3) | CV developers, researchers |
| [`03-hardware.md`](03-hardware.md) | Electrical connections, sensor specifications, mounting | Hardware engineers, installers |
| [`04-configuration.md`](04-configuration.md) | Calibration guide, Web UI parameters, troubleshooting | Installers, field technicians |
| [`05-webserver.md`](05-webserver.md) | HTTP API, WebSocket protocol, OTA update | Developers |
| [`06-operations.md`](06-operations.md) | Deployment, OTA, maintenance, monitoring | DevOps, maintenance technicians |
| [`07-peripherals.md`](07-peripherals.md) | Hardware drivers, pinout, status LED | Hardware engineers |
| [`08-data-persistence.md`](08-data-persistence.md) | Storage logic, NVS, SD CSV format | Developers |
| [`09-status-indicators.md`](09-status-indicators.md) | RGB LED codes, USB activation guide, power safety | Users, developers |

## Repository Structure

```
├── .agents/                   # AI agent instructions and plans
│   ├── context/               # Optimized AI context summaries
│   ├── plans/                 # Development plans (historical)
│   └── workflows/             # Agent workflows
├── components/                # ESP-IDF components
│   ├── mlx90640_driver/       # Melexis sensor I2C driver
│   ├── rtc_driver/            # DS3231 RTC driver (I2C1)
│   ├── sd_manager/            # SD card FATFS manager
│   ├── status_led/            # RGB status LED driver
│   ├── telemetry/             # Network stack, WebSocket, LogWriter
│   ├── thermal_pipeline/      # Vision pipeline (Core 1)
│   └── web_server/            # HTTP server + WebSocket + OTA
├── docs/
│   ├── 01-architecture.md     # System design overview
│   ├── 02-algorithm.md        # Tracking algorithms
│   ├── 03-hardware.md         # Pinout and connections
│   ├── 04-configuration.md    # Calibration guide
│   ├── 05-webserver.md        # HTTP API, WebSocket, OTA
│   ├── 06-operations.md       # Deployment and maintenance
│   ├── 07-peripherals.md      # Peripheral drivers
│   ├── 08-data-persistence.md # Storage, NVS, CSV
│   ├── 09-status-indicators.md# LED codes, USB guide
│   ├── assets/                # Screenshots, demo videos
│   │   ├── images/
│   │   └── videos/
│   ├── CHANGELOG.md           # Version history
│   ├── README.md              # This index
│   ├── ROADMAP.md             # Future plans
│   └── reference/             # Hardware reference docs
├── main/
│   └── main.cpp               # Entry point, task creation
├── scripts/
│   └── ota_upload.py          # OTA flash utility
├── managed_components/        # ESP-IDF managed dependencies
├── README.md                  # User documentation (EN)
└── README_ES.md               # User documentation (ES)
```

## Documentation Flow

**New user →** [`README.md`](../README.md) → [`04-configuration.md`](04-configuration.md) → [`06-operations.md`](06-operations.md)

**Developer →** [`README.md`](../README.md) →
- [01-architecture.md](01-architecture.md): High-level system design.
- [02-algorithm.md](02-algorithm.md): Vision pipeline and tracking logic.
- [05-webserver.md](05-webserver.md): HTTP API, WebSocket protocol, OTA.
- [07-peripherals.md](07-peripherals.md): Hardware drivers, pinout, and status LED.
- [08-data-persistence.md](08-data-persistence.md): Storage logic, NVS, and SD CSV format.

**Field installer →** [`README.md`](../README.md) → [`03-hardware.md`](03-hardware.md) → [`04-configuration.md`](04-configuration.md)

## AI Agent Context

Files in `.agents/context/` are optimized summaries for AI assistants.
These documents may be outdated; the source of truth are the files in this directory (`docs/`).

## Version

**v1.0.0-estable** — See [`CHANGELOG.md`](CHANGELOG.md) for full history.

## Legal Notes

- Project: MIT License
- MLX90640 Driver: Apache 2.0 (Copyright Melexis N.V.)
