# CHANGELOG - Thermal Door Detector

## [v2.6.0] - 2026-03-18 (Current)
### Added
- **Web OTA System**: Wireless update panel integrated into the Dashboard.
- **Safety Rollback**: Bootloop prevention by marking the app as valid in `app_main`.
- **OTA Script**: New `scripts/ota_upload.py` tool with progress bar and ESP32-S3 support.
- **Docs Restructuring**: New documentation system for humans (`docs/`) and agents (`.agents/`).

### Changed
- Increased HTTP Server stack to 16KB to support wireless flashing.
- Folder structure refactoring: `PROJECT_RESOURCES` removed in favor of `lib/` and `.agents/`.

## [v2.5.0] - Previous Stable Version
### Added
- **De-Chess Filter**: Removal of checkerboard artifacts in the thermal image.
- **Tactical HUD**: Cyberpunk web interface with high-frequency WebSockets.
- **Dual-Core**: Strict isolation of Pipeline (Core 1) and Network (Core 0).
- **NVS Control**: Calibration persistence.

---
*To see the roadmap of future versions, see [docs/roadmap.md](docs/roadmap.md).*
