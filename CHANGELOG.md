# CHANGELOG - Thermal Door Detector

## [Alpha 0.6] - 2026-03-20 (Current)
# Changelog

All notable changes to this project will be documented in this file.

## [Stage A1] - Bugfixes & Thermal Pre-processing
### Added
- **FrameAccumulator**: Fuses alternating sub-frames to eliminate visual artifacts and matrix sync issues.
- **NoiseFilter**: Per-pixel 1D Kalman filter to reduce NETD noise at high frame rates (8Hz filtered vs 16Hz raw).
- **Staged Pipeline**: New `composed_frame_` and `filtered_frame_` buffers for cleaner detection and display.

### Fixed
- **Bug 1 (Double Init)**: Removed redundant sensor initialization in the pipeline to preserve MLX90640 EEPROM lifespan.
- **Bug 2 (Initial Zone)**: Track starting zone now correctly derived from `lineEntryY` / `lineExitY` instead of hardcoded `y < 12`.
- **Bug 3 (Memory)**: Reduced `IPC_QUEUE_DEPTH` from 15 to 4, freeing ~20 KB of SRAM.
- **Bug 4 (Constants)**: Replaced all magic numbers (`15`) with `ThermalConfig::MAX_TRACKS` and `MAX_PEAKS`.
- **Bug 5 (Concurrency)**: Implemented atomic snapshot in `GET_CONFIG` using `portMUX_TYPE` critical sections to prevent torn reads.

## [v0.1.0] - Initial Release
### Added
- HTTP Server stack to 16KB to support wireless flashing.
- Folder structure refactoring: `PROJECT_RESOURCES` removed in favor of `lib/` and `.agents/`.

## [Alpha 0.5] - Previous Stable Version
### Added
- **De-Chess Filter**: Removal of checkerboard artifacts in the thermal image.
- **Tactical HUD**: Cyberpunk web interface with high-frequency WebSockets.
- **Dual-Core**: Strict isolation of Pipeline (Core 1) and Network (Core 0).
- **NVS Control**: Calibration persistence.

---
*To see the roadmap of future versions, see [docs/roadmap.md](docs/roadmap.md).*
