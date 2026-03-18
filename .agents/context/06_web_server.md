# Tarea: Servidor Web y HUD Táctico

## Objetivo
Servir una interfaz web Cyberpunk/Tactical en alta frecuencia para depuración y calibración.

## Requisitos Técnicos
1. **Embedded Assets**: HTML/CSS/JS comprimidos en archivos de cabecera C.
2. **WebSockets**: Canal binario para enviar la matriz 32x24 (int16_t x 100) y metadatos de tracks.
3. **Canvas 2D**: El navegador debe realizar el escalado a 640x480 usando interpolación bilineal.
4. **Configuración NVS**: Sliders en la web que envíen comandos para ajustar `TEMP_BIOLOGICO`, `NMS_RADIUS`, etc., y botón de guardado persistente.

## OTA
Integrar el panel de selección de archivo para el sistema de actualizaciones inalámbricas.
