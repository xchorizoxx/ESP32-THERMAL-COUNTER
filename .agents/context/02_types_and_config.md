# Task: Global Types and Configuration

## Objective
Centralize all parameters and data structures to ensure system consistency.

## Technical Requirements
1. **Types**: `thermal_types.hpp` with packed structs (`__attribute__((packed))`) to facilitate binary transmission.
2. **Configuration**: `thermal_config.hpp` with `constexpr` constants for hardware (pins) and software (thresholds).
3. **NVS**: Implement a `ConfigManager` class that reads from NVS at boot and updates pipeline values at runtime.

## Rules
- Do not use magic values (hardcoded) in `.cpp` files.
- Every adjustable parameter must have a `default` and persistence.
