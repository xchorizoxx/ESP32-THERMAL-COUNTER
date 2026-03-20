# 📡 OTA Flashing Guide — Thermal Door Detector

## ⚠️ Prerequisite (ONLY for the first time, or after changes to `sdkconfig.defaults`)

Changing `sdkconfig.defaults` (e.g., activating rollback) requires a **fullclean** before compiling.
Do this from the VS Code ESP-IDF extension:

1. `Ctrl+Shift+P` → **ESP-IDF: Full Clean Project**
2. `Ctrl+Shift+P` → **ESP-IDF: Build, Flash and Start a Monitor on your Device** (USB)

After this first flash via cable, the ESP32 is ready for wireless updates.

---

## 🚀 Normal OTA Flow (without USB)

```
Modify code  →  Build (VS Code)  →  python scripts/ota_upload.py
```

### Method 1 — Script from Terminal

```bash
# Connected to 'ThermalCounter' Wi-Fi (SoftAP IP: 192.168.4.1)
python scripts/ota_upload.py

# If the ESP32 is on another network:
python scripts/ota_upload.py 192.168.1.5
```

The script automatically looks for `build/DetectorPuerta.bin` and shows real-time progress.

### Method 2 — Dashboard Web Panel

1. Connect to `ThermalCounter` Wi-Fi.
2. Open `http://192.168.4.1` in your browser.
3. Drag (or select) the `build/DetectorPuerta.bin` file.
4. Click **⚡ FLASH FIRMWARE**.

---

## 🛡️ Anti-Bootloop Protection

The system uses the ESP-IDF rollback mechanism:

| State           | Description |
|-----------------|-------------|
| `PENDING_VERIFY` | New firmware has just been flashed. If it crashes, the bootloader reverts. |
| `VALID`          | `esp_ota_mark_app_valid_cancel_rollback()` called → firmware is definitively accepted. |

Marking as `VALID` occurs **inside `app_main()`** once WiFi + Sensor + HTTP Server + Tasks are all active. If the new firmware crashes before that point, the **bootloader automatically reverts** to the previous firmware.

---

## 📁 Key Files

| File | Role |
|---------|-----|
| `scripts/ota_upload.py` | PC upload script |
| `components/web_server/src/http_server.cpp` | POST `/update` handler |
| `components/web_server/src/web_ui_html.h` | OTA panel in the browser |
| `main/main.cpp` | `esp_ota_mark_app_valid_cancel_rollback()` call |
| `sdkconfig.defaults` | `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` |
| `partitions.csv` | `factory` + `ota_0` + `ota_1` |
