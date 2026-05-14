# Task: Thermal Vision Pipeline

## Objective
Implement the 5 steps of the computer vision algorithm on Core 1, optimizing for the ESP32-S3 (FPU).

## Pipeline Lifecycle
1. **Acquisition**: Reads raw data from the MLX90640 (16Hz raw sub-frames).
2. **Thermal Pre-processing** (A1):
   - **Chess Fusion**: Merges sub-pages into a `composed_frame_` to sync pixels.
   - **Noise Filtering**: High-frequency NETD reduction via per-pixel 1D Kalman.
3. **Background Modeling**: EMA-based dynamic map (8Hz sync).
4. **Detection**: Identifying thermal blobs exceeding Delta-T.
5. **NMS**: Filtering redundant peaks via Non-Maximum Suppression.
6. **Tracking** (A2): State estimation via `TrackletTracker` with a 20-frame position history and proportional coastal memory. **(Completed)**
7. **Unified Counting** (A3): A Finite State Machine (TrackletFSM) uses a 768-byte Unified Bitmap (ROI Map) for O(1) zone evaluation (IN/OUT/DEAD). Supports curves, diagonals, and exclusion zones. **(Completed)**

## Performance Notes
- Processing must occur in <40ms.
- Use native `float`. Do not use `double`.
