# Architectural Review and Code Analysis (Code Review)

*Review Date: March 2026*

The following details the strengths identified in the project's codebase and areas where clear opportunities for improvement exist for a mass-production-oriented version.

## 🟢 Technical Strengths

1.  **Priority Isolation (Dual Core):** 
    Pinning HTTP server tasks to Core 0 and the intensive vision pipeline to Core 1 is an exceptional design. It prevents WiFi interrupt handling from generating jitter or losing synchronicity in the strict I2C refresh rate (16 Hz).
2.  **Binary Network Emission:**
    The use of fixed numerical representations (e.g., sending temperatures as `int16_t` multiplied by 100) dramatically shortens the WebSocket payload per iteration to about 1.5 KB. Converting an array of 768 floats to a massive JSON string would have brought the ESP32 to its knees. The current approach is fast and efficient.
3.  **Central and Persistent Configuration Manager:**
    The implementation of `SAVE_CONFIG` with `nvs_flash` access keeps operational requirements and state separate, preventing the thermal analyst from being reset or re-compiled every time the microcontroller is unplugged.
4.  **Decoupled UI Pattern:**
    Image interpolation and UI drawing (Scanlines, Bounds, Matrices) were delegated to the connected client's graphics processor (Phone/PC) using HTML5 2D Canvas and GPU-assisted bilinear scaling. This avoided the typical overhead of scaling matrices locally in C++ and saved dozens of milliseconds of machine time.

## 🟡 Critical Areas/Cautionary Review (Opportunities for Improvement)

1.  **Tracking Difficulty in Growing Crowds (Naive NMS):**
    The tracker stage implements Non-Maximum Suppression in the form of strict iterative boxes. If two people walk shoulder-to-shoulder or cross hugged together under the sensor, the system will merge their heat signatures into a single thermal complex, causing a counting failure (counting 1 person where there are 2). This could be improved using more advanced clustering techniques (*modern Blob Component Labeling*, low-iteration *K-Means clustering*).
2.  **Blocking I2C Protocol:**
    Currently, the native ESP-IDF I2C command actively waits for the MLX90640 slave to complete each DMA frame. At strict RTOS levels, the `mlx_i2c_read(...)` driver could be transmuted to a non-blocking interrupt-based function, allowing the Core 1 CPU to sleep a bit more and save battery.
3.  **HTTP Stack / SoftAP Limitations:**
    `http_server.cpp` runs on the native ESP-IDF stack under a worker pool defined with fixed stack sizes (8192 or 4096 bytes). While a single user views the HUD, everything works perfectly; if 3 or 4 smartphones connect to the same IP, the ESP32 will saturate the WebSocket limit (as each connection requires its own buffer and thermal frame transmission) or overrun the heap. A "Hard" connection limit should be defined.
4.  **Fixed Spatial Adjustment (Alpha, Beta):**
    Static filter parameters (alpha = 0.85, beta = 0.05) assume typical "office person" movement dynamics. If the device is installed in logistics corridors where someone passes by quickly or runs, the Kalman/Alpha-Beta control hyperparameters will "lose sight" of that track because the acceleration doesn't match the inertial weight assigned in the current filter.

---
**Review Conclusion:** The base quality is high, modules are clean (no spaghetti), and coupling is appropriate (via FreeRTOS Queues). Ready for field experimentation stages before undergoing algorithmic refactoring iterations.
