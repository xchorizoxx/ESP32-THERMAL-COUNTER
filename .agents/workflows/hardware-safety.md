---
description: Safety Protocol for Hardware and Logical Layer Modifications
---

# ⚠️ HARDWARE SAFETY PROTOCOL (MANDATORY)

This document establishes critical rules for any AI agent interacting with the "Door Detector" project. Violation of these rules may compromise physical system stability.

## 1. Hardware and Bus Modifications
- **FORBIDDEN** to modify clock frequencies (I2C/SPI), pin numbers (GPIO), or voltage/pull-up configurations without prior user consultation.
- Any proposed bus changes (e.g., dropping from 1MHz to 400kHz) must be accompanied by a technical justification based on the datasheet and observed behavior.
- The user must provide explicit approval (OK) in the chat before the agent applies the change to `.cpp` or `.h` files.

## 2. Structural Pipeline Changes
- Sensor reading methods (e.g., changing from blocking to asynchronous) must not be altered if it affects the established frame rate, unless a critical failure is diagnosed.
- If visual artifacts are detected (e.g., MLX90640 checkerboard pattern), the agent must propose the filter or technical change first, explain its CPU impact, and wait for approval.

## 3. Stability Priority
- In embedded systems, sensor stability and EEPROM integrity take priority over "clean" aesthetic code. 
- Any change that could cause a "Panic" (Crash) on Core 1 must be communicated as a "High Risk Change."

---
// turbo-all
