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
6. **Tracking**: Prediction/Correlation via Alpha-Beta filter.
4. **Alpha-Beta Tracking**: Maintain track identity (persistent IDs) as they move through the FOV.
5. **Line Counting**: Two virtual lines (e.g., Y=11 and Y=13) that detect IN/OUT direction.

## Performance Notes
- Processing must occur in <40ms.
- Use native `float`. Do not use `double`.
