# Architecture and Workflow Document
**Project:** Thermal People Counting System (MLX90640)  
**Base Hardware:** ESP32-S3 (Dual-Core)  
**Execution Environment:** C++ / FreeRTOS

## 1. Architecture Overview
The system uses an Asymmetric Multiprocessing (AMP) approach managed by FreeRTOS. Mathematical processing is completely isolated from network management to ensure a constant 16 Hz vision pipeline.

## 2. Core and Task Assignment (FreeRTOS)
- **Core 1 (APP_CPU):** The Vision Engine. A high-priority task that reads I2C and executes the 5-step pipeline (EMA Background, Peaks, NMS, Tracking, Counting).
- **Core 0 (PRO_CPU):** The Telemetry Node. Manages Wi-Fi, SoftAP, HTTP Server, and WebSockets.

## 3. The Vision Pipeline (Core 1)
1. **Step 0 - Acquisition:** I2C reading from the MLX90640 sensor (32x24 pixels).
2. **Step 1 - Dynamic Background:** Background subtraction via Exponential Moving Average (EMA) with a feedback mask.
3. **Step 2 - Topology:** Local peak detection above biological threshold and background temperature.
4. **Step 3 - Adaptive NMS:** Temperature-ordered Non-Maximum Suppression to consolidate tracks.
5. **Step 4 - Tracking and Logic:** Alpha-Beta filter to smooth trajectories and hysteresis-based line crossing logic.
6. **Step 5 - Feedback:** Generation of a blocking mask for the next frame based on active tracks.

---
*This is the original design document that served as the basis for the V1/V2 implementation.*
