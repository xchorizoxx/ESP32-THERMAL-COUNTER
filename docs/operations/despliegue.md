# Guía de Operaciones: Despliegue y Primer Setup

Este documento explica cómo poner en marcha un nuevo Detector de Puerta Térmico desde cero.

## 1. Clonación y Requisitos
- Instalar **VS Code** con la extensión **ESP-IDF** (versión v5.5 recomendada).
- Clonar el repositorio.

## 2. Preparación del Entorno
1. Seleccionar el chip: `Ctrl+Shift+P` → `ESP-IDF: Set Espressif Device Target` → `esp32s3`.
2. Limpiar el proyecto: `Ctrl+Shift+P` → `ESP-IDF: Full Clean Project`.
   *Esto es vital la primera vez para que las particiones OTA y el rollback se configuren correctamente.*

## 3. Compilación e Instalación (USB)
1. Conectar el ESP32-S3 por cable USB.
2. Presionar el icono del rayo (**Build, Flash and Monitor**).
3. Esperar que el firmware se suba. El dispositivo creará la red `ThermalCounter`.

## 4. Calibración In-Situ
1. Conectar PC/Celular al Wi-Fi `ThermalCounter`.
2. Abrir `http://192.168.4.1`.
3. Ajustar umbrales según la altura del techo y la temperatura ambiente.
4. Clic en **GUARDAR EN FLASH**.

## 5. Actualizaciones Futuras (OTA)
Ya no necesitas el cable USB. Para actualizar:
1. Compilar el código: `Build` únicamente.
2. Ejecutar `python scripts/ota_upload.py` o subir el archivo `.bin` desde el Dashboard web.
