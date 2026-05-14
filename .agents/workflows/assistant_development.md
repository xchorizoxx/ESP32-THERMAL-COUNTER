---
description: Assistant development process and architecture — Thermal Door Detector
---

# 🤖 Antigravity Assistant Workflow

This document describes the internal "Workflow" I followed to develop this project autonomously, from analysis to final implementation.

## 1. Resource and Requirements Analysis
- **Tools**: `list_dir`, `view_file`.
- **Action**: Identified resources in `PROJECT_RESOURCES/`. Read `Project-Draft.md` to understand the mathematical pipeline and analyzed the Melexis C library to convert it to C++ OOP.

## 2. Architecture Proposal (Implementation Plan)
- **Tools**: `write_to_file` (Artifact).
- **Action**: Before writing a single line of code, I generated an `implementation_plan.md` with:
  - 3-component ESP-IDF structure.
  - Pipeline flowchart.
  - Pin definitions (GPIO 8/9).
  - Multitasking strategy (Core 0 for telemetry, Core 1 for pipeline).

## 3. Layered Modular Development (Bottom-Up)
I followed a logical order to ensure each part had its dependencies ready:
1. **Types and Configuration**: `thermal_types.hpp` and `thermal_config.hpp`.
2. **Hardware Driver**: `Mlx90640Sensor` wrapper encapsulating ESP-IDF I2C.
3. **Mathematical Pipeline**: Implementation of the 5 steps (Background, Peaks, NMS, Tracker, Mask) as independent classes to facilitate unit testing.
4. **Telemetry**: Management of SoftAP and UDP sockets on the second core.
5. **Integration**: `main.cpp` orchestrating everything with static tasks.

## 4. Memory Management and Safety
- **No Fragmentation**: Used `xTaskCreateStatic` and `xQueueCreateStatic`.
- **Performance**: Converted floats to `int16` to halve the WiFi bandwidth.
- **Robustness**: Implemented an automatic I2C bus reset mechanism if the sensor stops responding.

## 5. Documentation for the Future
- **Agent Instructions**: Created 7 files in `AGENT_INSTRUCTIONS/` (now translated to `.agents/context/`) so that if another agent enters tomorrow, they know exactly what each module does without reading all the code.
- **Walkthrough**: Final implementation summary.
- **Calibration Guide**: Specifically for the end user.

---

### // turbo-all
To run the build after finishing changes:
1. Ensure the target is configured (`idf.py set-target esp32s3`)
2. Compile the project from the IDE to verify that the entire workflow was successful.
