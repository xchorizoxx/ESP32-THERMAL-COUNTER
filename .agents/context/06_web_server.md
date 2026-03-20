# Task: Web Server and Tactical HUD

## Objective
Serve a high-frequency Cyberpunk/Tactical web interface for debugging and calibration.

## Technical Requirements
1. **Embedded Assets**: HTML/CSS/JS compressed into C header files.
2. **WebSockets**: Binary channel for sending the 32x24 matrix (int16_t x 100) and track metadata.
3. **2D Canvas**: The browser must perform scaling to 640x480 using bilinear interpolation.
4. **NVS Configuration**: Web sliders to send commands for adjusting `TEMP_BIOLOGICO`, `NMS_RADIUS`, etc., with a persistent save button.

## OTA
Integrate an OTA file selection panel for the wireless update system.
