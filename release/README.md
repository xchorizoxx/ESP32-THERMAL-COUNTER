# Thermal Door Counter v1.0.0

Binary universal para **cualquier ESP32-S3** (4MB / 8MB / 16MB flash).
Incluye bootloader, partition table (OTA dual) y aplicación.

## Requisitos

- Python 3.6+
- `esptool`:
  ```bash
  pip install esptool
  ```
- Cable USB-C
- ESP32-S3 en modo descarga (hold BOOT + tap RESET)

## Flashear

```bash
esptool.py --chip esp32s3 -b 460800 write-flash \
  --flash-mode dio --flash-size detect \
  0x0 DetectorPuerta-v1.0.0.bin
```

> `--flash-size detect` auto-detecta el tamaño real de la flash.
> Funciona en placas con 4MB, 8MB o 16MB.

## Post-flash

1. Conectarse a WiFi **ThermalCounter**
2. Abrir http://192.168.4.1
3. Ajustar líneas de conteo y umbrales
4. Listo

## Notas

- Puerto serie por defecto: `/dev/ttyACM0` (Linux) o `COM3` (Windows)
- Para especificar puerto: `esptool.py -p /dev/ttyACM0 ...`
- Si falla la conexión WiFi, mantener BOOT 2s para activar modo USB Network (192.168.4.1)
