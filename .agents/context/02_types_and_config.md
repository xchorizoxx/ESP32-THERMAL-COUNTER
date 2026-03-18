# Tarea: Tipos Globales y Configuración

## Objetivo
Centralizar todos los parámetros y estructuras de datos para garantizar coherencia en el sistema.

## Requisitos Técnicos
1. **Tipos**: `thermal_types.hpp` con structs empaquetados (`__attribute__((packed))`) para facilitar la transmisión binaria.
2. **Configuración**: `thermal_config.hpp` con constantes `constexpr` para hardware (pines) y software (umbrales).
3. **NVS**: Implementar una clase `ConfigManager` que lea de NVS en el booteo y actualice los valores del pipeline en runtime.

## Reglas
- No usar valores mágicos (hardcoded) en los archivos `.cpp`.
- Todo parámetro que sea ajustable debe tener un `default` y persistencia.
