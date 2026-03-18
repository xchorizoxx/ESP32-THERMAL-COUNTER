# Guía de Reversión - Fix Boot Loop (Marzo 18, 2026)

Este documento detalla los cambios realizados para solucionar el bucle de reinicio del ESP32 y cómo revertirlos.

## Resumen de Cambios

1. **`components/mlx90640_driver/src/MLX90640_API.c`**:
   - Se agregaron contadores de seguridad (`guard`) a todos los bucles `while` en las funciones de extracción de parámetros (`ExtractAlpha`, `ExtractKta`, `ExtractKv`).
   - Estos bucles podían volverse infinitos si el sensor entregaba datos nulos (0.0), bloqueando la CPU y disparando el Watchdog.
   - **Riesgo:** Bajo. No altera la lógica de negocio, solo previene cuelgues.

2. **`components/mlx90640_driver/src/mlx90640_sensor.cpp`**:
   - Se añadió una verificación de suma (checksum simple) tras leer la EEPROM.
   - Si la EEPROM viene vacía (todos 0 o 0xFF), el inicializador aborta con un error claro en el log (`FATAL: EEPROM corrupta`) en lugar de intentar procesarla y colgarse.
   - Se añadieron logs adicionales para ver el progreso de la extracción.

## Cómo Revertir

### Opción 1: Reversión Manual (Recomendado)

#### En `MLX90640_API.c`:
- Busca las variables `int guard = 0;`.
- Elimina la condición `&& guard < 100` de los bucles `while`.
- Elimina la línea `guard++;`.

#### En `mlx90640_sensor.cpp`:
- Elimina el bloque de código que calcula `uint32_t sum` y el condicional `if (sum == 0 || sum == ...)`.
- Elimina los logs `ESP_LOGI(TAG, "Extrayendo parámetros...");`.

### Opción 2: Usar el historial de Git
Si el proyecto usa Git, simplemente ejecuta:
```bash
git checkout components/mlx90640_driver/src/MLX90640_API.c
git checkout components/mlx90640_driver/src/mlx90640_sensor.cpp
```

## Por qué sucedió el error
El sensor MLX90640 es sensible al ruido en el bus I2C. Si la lectura de la EEPROM falla silenciosamente (retornando todo ceros), la librería oficial de Melexis entra en bucles infinitos al intentar converger parámetros que dependen de multiplicaciones por 2 sucesivas. Esto "congela" el núcleo de la CPU y provoca el reinicio por Watchdog.
