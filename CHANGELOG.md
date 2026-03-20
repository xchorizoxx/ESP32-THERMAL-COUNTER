# CHANGELOG - Thermal Door Detector

## [Alpha 0.6] - 2026-03-20 (Current)
### Added
- **I2C CMA Optimization**: Migrated to the new `driver/i2c_master.h` with DMA support and non-blocking transactions (yielding CPU during transfers).
- **Static IpcPacket**: Moved heavy telemetry structures to static memory to prevent stack overflows and increase system stability.

### Changed
- **HUD Synchronization**: Fixed a parity mismatch bug where the web dashboard would skip updated frames.
- **Project Standardization**: Normalized all source code, comments, and internal documentation to Technical English.
- Increased HTTP Server stack to 16KB to support wireless flashing.
- Folder structure refactoring: `PROJECT_RESOURCES` removed in favor of `lib/` and `.agents/`.

## [Alpha 0.5] - Previous Stable Version
### Added
- **De-Chess Filter**: Removal of checkerboard artifacts in the thermal image.
- **Tactical HUD**: Cyberpunk web interface with high-frequency WebSockets.
- **Dual-Core**: Strict isolation of Pipeline (Core 1) and Network (Core 0).
- **NVS Control**: Calibration persistence.

---
*To see the roadmap of future versions, see [docs/roadmap.md](docs/roadmap.md).*
