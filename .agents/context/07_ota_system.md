# Contexto del Sistema OTA (Over-The-Air)

El ESP32-S3 de este proyecto puede actualizar su propio firmware por Wi-Fi sin usar cables USB.

## Arquitectura de Particiones
En `partitions.csv` el chip está dividido en:
- `factory` (1MB): Firmware de fábrica (solo flasheado por USB originalmente).
- `ota_0` (1400KB): Slot principal OTA.
- `ota_1` (1400KB): Slot secundario OTA (Dual-Bank).

## Mecanismo de Flasheo (Frontend y Backend)
1. **Endpoint HTTP**: El componente `web_server` expone la ruta `POST /update`.
2. **Espacio de Stack**: El Web Server `httpd` tiene un `stack_size` incrementado a **16384 bytes** para manejar los chunks de escritura a flash sin desbordarse.
3. **Web UI**: Dentro de `web_ui_html.h` hay un bloque Javascript (`uploadFirmware`) que empaqueta el `.bin` usando XHR (XMLHttpRequest) puro y dibuja una barra de carga en el DOM.

## Rollback de Seguridad (Anti-Bootloop)
1. Las configuraciones del ESP-IDF (`sdkconfig.defaults`) tienen habilitado `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`.
2. Cuando se sube un nuevo `.bin`, la partición recibe el estado `ESP_OTA_IMG_PENDING_VERIFY`.
3. Al final de `main.cpp`, dentro de `app_main()`, una vez que todo levantó con éxito (WiFi, Sensor, WebServer, Queues), el firmware ejecuta obligatoriamente:  
   `esp_ota_mark_app_valid_cancel_rollback();`
4. Si el nuevo código crashea **antes** de llegar a esa línea, el micro se reinicia por el Watchdog, el bootloader ve que sigue "Pending Verify" (falló), y automáticamente arranca la versión anterior sana.

## Scripts de Terminal
El script `scripts/ota_upload.py` es la alternativa gráfica. Un agente o el usuario lo puede invocar pasándole el `.bin` y la IP del ESP para flashear remotamente.
