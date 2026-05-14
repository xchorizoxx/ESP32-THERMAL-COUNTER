# CHANGELOG - Thermal Door Detector

## [v1.0.0-estable] - 2026-05-13 (Current)
### Added
- **SD Session-based organization**: Logs and clips organized in `session_XXX/` subfolders.
- **CSV Restructuring**: Separate `crossings.csv` (crossing events) and `clips.csv` (clip lifecycle) per session.
- **Config Export**: Door lines, thresholds, zones, and sensor geometry exported to `config/session_XXX.json`.
- **Client-side ZIP Downloads**: Pure JS ZIP generation (CRC32 + uncompressed) for session data, full backup, and all clips.
- **Delete All Clips**: Endpoint `POST /api/clips/delete_all` to erase all `.thv` files across sessions.
- **Enhanced Clip List**: API returns enriched metadata (session, duration, timestamp) recursing into session subdirectories.
- **Per-session Clip Storage**: Thermal clips stored in `clips/session_XXX/` directories.

### Changed
- **Docs restructured**: `docs/` files reorganized with numeric prefix (`01-architecture.md`, `02-algorithm.md`, etc.).
- **Agent instructions consolidated**: `AGENTES/` merged into `.agents/plans/`, `CLAUDE.md` removed (outdated).
- **Branch cleanup**: Old feature branches removed, `main` promoted to primary, tagged `v1.0.0-estable`.

### Fixed
- **LogWriter stack overflow**: Task stack increased from 2048 to 4096 bytes.
- **SD format specifiers**: Corrected `uint32_t`/`uint64_t` casts for picolibc compatibility.
- **I2C path buffers**: Increased buffer sizes (`fpath[96]`, `sfpath[128]`) to prevent truncation with session paths.
- **Unused variable warnings**: Suppressed with `(void)` casts to pass `-Werror`.

## [Alpha 0.8] - 2026-04-04
### [Stage A3-B1] - Metric Refinement & USB Implementation
#### Added
- **USB Network Support**: TinyUSB (ECM/RNDIS) implementation for local access via cable, activated by long-pressing the BOOT button (2.0s).
- **RGB Status LED**: Visual indicator on GPIO 48 for system states (Blue: Booting, Green: OK, Purple: USB Mode).
- **Geometric FOV Correction**: Physical mapping of pixels to real meters based on sensor height (`SENSOR_HEIGHT_M`).
- **Sub-pixel Extraction**: Centroid calculation using 1st-order thermal moments for smoother tracking.
- **Physical Tracking**: Target association based on Euclidean distance in meters instead of pixels.
- **I2C Fast-Mode Plus**: 1 MHz bus support for 32 Hz acquisition from the MLX90640.
- **System Hardening**: Integrated Watchdog (WDT) in TelemetryTask and Spinlocks in ThermalConfig for thread safety.
- **Responsive Web UI**: New Desktop-optimized Grid with bilinear interpolation in the thermal viewer.

### Changed
- **Unified Limits**: Adjusted `MAX_PEAKS` and `MAX_TRACKS` to 20 for memory consistency.
- **Memory Optimization**: Removal of redundant parameters in `thermal_config.hpp`.
- **Frontend Refactor**: Migrated to a modular JS component for HUD rendering.

### Removed
- **Obsolete Parameters**: Cleaned up `DOOR_HEIGHT_M` (legacy), `ALPHA_TRK`, and redundant temperature ranges.


## [Alpha 0.7] - 2026-03-31
# Changelog

All notable changes to this project will be documented in this file.

## [Stage A3] - FSM Infinite Door & Unified Bitmap
### Added
- **TrackletFSM**: New Finite State Machine using a 768-byte Unified Bitmap (ROI Map) for O(1) zone lookups.
- **Autonomous ROI Tracking**: Supports free-form counting lines (curves, diagonals) and exclusion zones directly on the ESP32.
- **Ghost Suppression**: Automatic rejection of tracklets born in "Dead Zones" (ZONE_DEAD) or neutral areas.
- **Bi-Directional Counting**: Unified logic for North-South, East-West, or diagonal crossings based on zone transitions.
- **Mock Calibration**: Initial test map with 18x11 central dead zone and 7px side corridors for wall-hugging support.

## [Stage A2] - Stable Tracking
### Added
- **TrackletTracker**: Replaces legacy AlphaBetaTracker. Implements a 20-frame circular history buffer for linear velocity prediction.
- **Proportional Coastal Memory**: Fixes "ghost IDs" by linking a track's survival time during occlusion directly to its confirmed lifespan (up to 750ms).
- **EMA Display Smoothing**: Decouples HUD rendering positions from the physics engine to eliminate visual stuttering.
- **Composite Matching**: Matches targets using both Euclidean distance and thermal signature similarity.

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
*To see the roadmap of future versions, see [ROADMAP.md](ROADMAP.md).*
