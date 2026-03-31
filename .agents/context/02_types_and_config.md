# Task: Global Types and Configuration

## Objective
Centralize all parameters and data structures to ensure system consistency.

## Technical Requirements
1. **Types**: `thermal_types.hpp` with packed structs (`__attribute__((packed))`) to facilitate binary transmission.
2. **Configuration**: `thermal_config.hpp` with `constexpr` constants for hardware (pins) and software (thresholds).
3. **NVS**: Implement a- `IPC_QUEUE_DEPTH`: 4 (Optimized in A1 from 15 to save ~20KB SRAM).
- `MAX_TRACKS`: 15 (Universal constant for track array sizing).
- `MAX_PEAKS`: 15 (Universal constant for peak detection array sizing).
- `PIPELINE_FREQ_HZ`: 16 (Raw acquisition rate; result in 8Hz processed frames).
at boot and updates pipeline values at runtime.

## Rules
- Do not use magic values (hardcoded) in `.cpp` files.
- Every adjustable parameter must have a `default` and persistence.
