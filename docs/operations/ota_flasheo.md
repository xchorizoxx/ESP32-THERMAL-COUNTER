# 📡 Guía de Flasheo OTA — Detector de Puerta Térmica

## ⚠️ Requisito Previo (SOLO la primera vez, o tras cambios en `sdkconfig.defaults`)

Cambiar `sdkconfig.defaults` (p. ej. activar rollback) requiere un **fullclean** antes de compilar.
Hazlo desde la extensión ESP-IDF de VS Code:

1. `Ctrl+Shift+P` → **ESP-IDF: Full Clean Project**
2. `Ctrl+Shift+P` → **ESP-IDF: Build, Flash and Start a Monitor on your Device** (USB)

Después de este primer flash por cable, el ESP32 queda listo para actualizarse inalámbricamente.

---

## 🚀 Flujo Normal OTA (sin USB)

```
Modificas código  →  Build (VS Code)  →  python scripts/ota_upload.py
```

### Método 1 — Script desde Antigravity / terminal

```bash
# Conectado al Wi-Fi 'ThermalCounter' (IP del SoftAP: 192.168.4.1)
python scripts/ota_upload.py

# Si el ESP32 está en otra red:
python scripts/ota_upload.py 192.168.1.5
```

El script busca automáticamente `build/DetectorPuerta.bin` y muestra progreso en tiempo real.

### Método 2 — Panel Web del Dashboard

1. Conectar al Wi-Fi `ThermalCounter`
2. Abrir `http://192.168.4.1` en el navegador
3. Arrastrar (o seleccionar) el archivo `build/DetectorPuerta.bin`
4. Clic en **⚡ FLASH FIRMWARE**

---

## 🛡️ Protección Anti-Bootloop

El sistema usa el mecanismo de rollback de ESP-IDF:

| Estado           | Descripción |
|-----------------|-------------|
| `PENDING_VERIFY` | Nuevo firmware acaba de ser flasheado. Si hace crash, el bootloader revierte. |
| `VALID`          | `esp_ota_mark_app_valid_cancel_rollback()` llamado → firmware definitivamente aceptado. |

El marcado como `VALID` ocurre **dentro de `app_main()`** una vez que WiFi + Sensor + HTTP Server + Tareas están todos activos. Si el firmware nuevo hace crash antes de ese punto, el **bootloader revierte automáticamente** al firmware anterior.

---

## 📁 Archivos Clave

| Archivo | Rol |
|---------|-----|
| `scripts/ota_upload.py` | Script de upload desde PC |
| `components/web_server/src/http_server.cpp` | Handler POST `/update` |
| `components/web_server/src/web_ui_html.h` | Panel OTA en el navegador |
| `main/main.cpp` | Llamada a `esp_ota_mark_app_valid_cancel_rollback()` |
| `sdkconfig.defaults` | `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` |
| `partitions.csv` | `factory` + `ota_0` + `ota_1` |
