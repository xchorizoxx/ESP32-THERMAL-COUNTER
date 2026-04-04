# Design and Implementation: SD Card and RTC (V3 Roadmap)

This document consolidates the logical architecture and hardware topology for the future implementation of thermal clip recording on an SD card with Real-Time Clock (RTC) timestamps.

## 1. Thermal "Dashcam" System Concept
The primary goal is to allow the device to save an exact clip of what the camera saw **just before and after** a person crossed the door (IN/OUT event) to an SD memory card.

This requires **Pre-Recording** via a static Ring Buffer.

## 2. Logical Flow and Memory in FreeRTOS

1.  **The Ring Buffer:**
    A static class pre-allocates ~36 KB of RAM (for 24 frames @ 8 FPS = 3 seconds of pre-recording). This occurs in Core 1 and is protected by a Mutex.
2.  **The Trigger Event:**
    When the detector confirms a crossing, it emits a non-blocking *Event Group* to an inactive task on Core 0.
3.  **Timestamp Capture:**
    The RTC is queried via I2C for its current `DateTime` object to name the log (`20241103_091522_OUT.bin`).
4.  **Asynchronous Dumping:**
    The `SDRecorderTask` takes a snapshot of the ring buffer (3s in the past), waits an additional 2 seconds (post-recording), and dumps the complete file (~60 KB) to the SPI bus without slowing down the camera, which remains on I2C.

## 3. Hardware and Pinout (S3 Proposal)

### DS3231 RTC Clock (I2C Bus)
Shares the same physical bus as the thermal camera.
*   **VCC:** 3.3V
*   **SDA / SCL:** Same pinout as the MLX90640 (GPIO 8 and 9)
*   **Address:** `0x68`

### MicroSD Module (SPI Bus)
Dumping is done via `sdspi_host`.
*   **VCC:** 3.3V or 5V (depending on regulator)
*   **MISO:** GPIO 19
*   **MOSI:** GPIO 23
*   **SCK:** GPIO 18
*   **CS (Chip Select):** GPIO 32 (Exclusive for the card)

> [!CAUTION]
> Avoid strapping pins (0, 2, 5, 12, 15) as SD Chip Select to prevent boot failures. Avoid analog pins (34-39) if ADCs are planned for future use. 
