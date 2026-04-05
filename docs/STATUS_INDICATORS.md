# Status Indicators & USB Configuration

This document describes the visual feedback and USB connectivity features implemented in the ESP32-S3 Thermal Counter.

## 1. RGB Status LED (GPIO 48)

The system uses a built-in WS2812 RGB LED (Pin 48) to provide real-time status feedback.

| Color  | System State | Description |
| :---   | :---         | :---        |
| **Blue**  | **BOOT**     | Initializing hardware and FreeRTOS tasks. |
| **Green** | **READY**    | All systems (Thermal, WiFi, Telemetry) are operational. |
| **Purple**| **USB MODE** | USB NCM Networking is active (OTG PC access). |

### Safety Constraints
* **Brightness Limit**: The LED is hard-coded to a **30% maximum brightness**. This prevents excessive power draw (crucial when powered via mobile OTG) and protects the LED hardware.
* **Non-Blocking**: The LED logic runs efficiently without using `vTaskDelay` inside main loops.

---

## 2. USB Network (ECM/NCM/RNDIS)

The device supports local network access via USB for maintenance or PC-based UI monitoring.

### How to Activate
To enable USB Networking:
1. Ensure the device is powered on.
2. **Press and hold the BOOT button (GPIO 0)** for **2.0 seconds**.
3. The Status LED will turn **Purple** once the USB stack is initialized.

### Technical Details
* **Mode**: NCM (Network Control Model) - Driver-free on most modern operating systems (Linux/Mac/Windows 10+).
* **MAC Address**: `02:02:84:6A:96:00`
* **Connectivity**: Once activated, the device enumerates as a network interface on the host PC.

---

## 3. Future Indicators (Planned)
* **Red Flashing**: Critical error or Watchdog timeout.
* **Yellow Flash**: High thermal peak detection (crossing door line).
