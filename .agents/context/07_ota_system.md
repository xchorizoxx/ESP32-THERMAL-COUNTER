# OTA (Over-The-Air) System Context

The ESP32-S3 in this project can update its own firmware over Wi-Fi without using USB cables.

## Partition Architecture
In `partitions.csv`, the chip is divided into:
- `factory` (1MB): Factory firmware (originally flashed via USB).
- `ota_0` (1400KB): Primary OTA slot.
- `ota_1` (1400KB): Secondary OTA slot (Dual-Bank).

## Flashing Mechanism (Frontend and Backend)
1. **HTTP Endpoint**: The `web_server` component exposes the `POST /update` route.
2. **Stack Space**: The `httpd` Web Server has an increased `stack_size` of **16384 bytes** to handle flash writing chunks without overflowing.
3. **Web UI**: Within `web_ui_html.h`, there is a Javascript block (`uploadFirmware`) that packages the `.bin` using pure XHR (XMLHttpRequest) and renders a progress bar in the DOM.

## Safety Rollback (Anti-Bootloop)
1. ESP-IDF configurations (`sdkconfig.defaults`) have `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` enabled.
2. When a new `.bin` is uploaded, the partition receives the `ESP_OTA_IMG_PENDING_VERIFY` state.
3. At the end of `main.cpp`, inside `app_main()`, once everything has successfully started (WiFi, Sensor, WebServer, Queues), the firmware must execute:  
   `esp_ota_mark_app_valid_cancel_rollback();`
4. If the new code crashes **before** reaching that line, the micro restarts via the Watchdog, the bootloader sees it is still "Pending Verify" (it failed), and automatically boots the previous stable version.

## Terminal Scripts
The `scripts/ota_upload.py` script is the automated alternative. An agent or the user can invoke it by passing the `.bin` and the ESP's IP for remote flashing.
