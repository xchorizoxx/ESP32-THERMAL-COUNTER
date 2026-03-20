# Task: Sensor Driver and I2C Acquisition

## Objective
Implement a C++ abstraction layer for the MLX90640 sensor that ensures I2C bus stability and deterministic frame delivery.

## Technical Requirements
1. **OO Layer**: `MLX90640_Sensor` class to hide calls to the Melexis API.
2. **I2C Bus**: Configure at 400kHz (Fast Mode) for greater stability against noise. 
3. **DMA/Interrupts**: If possible, use the advanced ESP-IDF driver to avoid Core 1 stalls.
4. **Resilience**: If the sensor fails at startup, it must NOT block the rest of the system (WiFi/Web). It should attempt to re-initialize or show an error on the HUD.

## References
- `lib/mlx90640-library-master/`
- `sdkconfig.defaults` (I2C configuration)
