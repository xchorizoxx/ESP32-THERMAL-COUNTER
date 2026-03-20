# Operations Guide: Deployment and First Setup

This document explains how to set up a new Thermal Door Detector from scratch.

## 1. Cloning and Requirements
- Install **VS Code** with the **ESP-IDF** extension (v5.5 recommended).
- Clone the repository.

## 2. Environment Preparation
1. Select the chip: `Ctrl+Shift+P` → `ESP-IDF: Set Espressif Device Target` → `esp32s3`.
2. Clean the project: `Ctrl+Shift+P` → `ESP-IDF: Full Clean Project`.
    *This is vital the first time so that OTA partitions and rollback are correctly configured.*

## 3. Compilation and Installation (USB)
1. Connect the ESP32-S3 via USB cable.
2. Click the lightning icon (**Build, Flash and Monitor**).
3. Wait for the firmware to upload. The device will create the `ThermalCounter` network.

## 4. In-Situ Calibration
1. Connect PC/Phone to the `ThermalCounter` Wi-Fi.
2. Open `http://192.168.4.1`.
3. Adjust thresholds based on ceiling height and ambient temperature.
4. Click **SAVE TO FLASH**.

## 5. Future Updates (OTA)
You no longer need the USB cable. To update:
1. Compile the code: `Build` only.
2. Run `python scripts/ota_upload.py` or upload the `.bin` file from the web Dashboard.
