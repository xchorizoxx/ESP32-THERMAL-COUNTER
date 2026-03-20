# Task: Thermal Vision Pipeline

## Objective
Implement the 5 steps of the computer vision algorithm on Core 1, optimizing for the ESP32-S3 (FPU).

## Technical Requirements
1. **Spatial Filter**: Implement interpolation or smoothing to eliminate the "checkerboard" pattern.
2. **EMA Background**: The background should be learned only when no movement is detected (masking).
3. **NMS**: Non-maximum suppression using squared Euclidean distance (to avoid `sqrt`).
4. **Alpha-Beta Tracking**: Maintain track identity (persistent IDs) as they move through the FOV.
5. **Line Counting**: Two virtual lines (e.g., Y=11 and Y=13) that detect IN/OUT direction.

## Performance Notes
- Processing must occur in <40ms.
- Use native `float`. Do not use `double`.
