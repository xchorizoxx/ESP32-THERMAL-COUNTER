# Future Implementation Proposals (V3 & V4 Roadmap)

Thinking about long-term scalability, security, and operational intelligence of the ESP32 Door Counter, the designed C++ architecture opens the door to the following organic evolutions:

## 1. Machine Learning Integration (Real Edge AI)
The *Alpha-Beta* and *NMS* algorithm based on raw thermal physics is exceptionally fast but can be easily fooled by a "mapped reindeer," large pets (like big dogs), or hot objects like industrial food carts.
**Proposal:** 
Replace the Core 1 mathematical block with a **TensorFlow Lite for Microcontrollers** inference engine. A small Convolutional Neural Network (CNN) can be trained on thousands of 32x24 thermal captures of human behavior from a ceiling perspective. This would add "Semantic Segmentation" capable of distinguishing between a "Upper Human Body" and an "Anonymous Pet/Heat Source."

## 2. Enterprise-Level Telemetry (MQTT / InfluxDB)
Currently, the system's value lies in manually viewing the HUD. To be a corporate IoT system:
**Proposal:** 
Change the `wifi_init` architecture from exclusive `WIFI_AP` mode to `WIFI_STA` mode so the ESP32 connects to the building's network as a silent client. From there, publish a minimum JSON packet `{ counts_in: 19, counts_out: 4, tz: "UTC"}` every 60 seconds to an internal **MQTT** broker. This would allow managers to integrate data directly into Grafana/HomeAssistant or a corporate cloud metrics server without modifying existing logic.

## 3. Biometric Auto-Calibration (Auto-Tuning)
Installing sensors in 50 different doors likely requires manually adjusting the `NMS Radius` slider for each, as door widths and ceiling heights (between 2.0 and 5.0 meters) differ.
**Proposal:**
Program a "24-hour static" initialization routine. The sensor will use Artificial Intelligence or probabilistic detection to evaluate the average pixel size of detected hot bulges. After analysis, the system will self-select appropriate `NMS_CENTER` and `NMS_EDGE` values and automatically redefine the `line_entry` and `line_exit` counting lines, ensuring zero error margin during installation by non-technical staff.

## 4. Wireless Updates (OTA Subsystem)
Currently, the physical ESP32-S3 USB serial harness is required for flashing. 
**Proposal:**
Already implemented in V2.6! The system now supports both a Web Dashboard uploader and a terminal script (`ota_upload.py`), facilitating remote debugging and urgent patches for ceiling-mounted systems.

## 5. Thermal Stereoscopic Vision Implementation
For excessively wide entrances or stores where multiple people cross simultaneously, a single MLX90640 (110x75° field) starts to deform at the edges and suffers from blind spots.
**Proposal:** 
Utilize the same firmware but with an *I2C Multiplexer* or use both native I2C buses (I2C0, I2C1) of the ESP32-S3 to mount **2 overlapping crossed sensors**. This would provide not only a superior `64x24` thermal resolution but authentic geometric parallelism, providing real depth values in a 3D space.

## 6. Thermal Clip Recording on SD Card (with RTC)
The current system processes thermal frames on-the-fly for counting. For audits, algorithm debugging, or post-human verification, it is extremely useful to save "thermal video clips" Dashcam-style only when a crossing event occurs.
**Proposal:**
Integrate a MicroSD module via SPI bus and a Real-Time Clock (RTC, e.g., DS3231) on the same I2C bus as the camera. Load the `esp_vfs_fat` environment to mount the SD file system. This will allow saving small `.bin` files with the thermal matrix of the last N frames before and after the crossing, using the RTC to name the file with an absolute timestamp (e.g., `20241026_143005_IN.bin`).
*For technical feasibility and implementation details, see the dedicated documents in `docs/hardware/sd_rtc_design.md`.*
