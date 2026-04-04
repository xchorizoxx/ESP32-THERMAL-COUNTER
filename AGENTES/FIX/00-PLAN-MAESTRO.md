# Plan Maestro de Mejoras — ESP32 Thermal Counter

## Contexto del proyecto

- **Hardware:** ESP32-S3 + MLX90640 (32×24 px, FOV 110°×75°)
- **Framework:** ESP-IDF v6.0, std=gnu++26
- **Aplicación:** Contador de personas en **puertas anchas** (4+ m), múltiples personas simultáneas
- **Pipeline:** Core 1 a 16 Hz → 8 Hz efectivos por acumulador de sub-frames
- **Frontend:** WebSocket binary → Canvas 320×240

## Archivos de fases (ejecutar en este orden)

| Fase | Archivo | Descripción | Archivos modificados |
|------|---------|-------------|---------------------|
| 1 | [FASE-1-quick-wins.md](./FASE-1-quick-wins.md) | Correcciones rápidas JS + cleanup C++ | `app.js`, eliminar `alpha_beta_tracker.*` |
| 2 | [FASE-2-hud-redesign.md](./FASE-2-hud-redesign.md) | Rediseño completo del HUD web | `index.html`, `style.css`, `app.js` |
| 3 | [FASE-3-pipeline-firmware.md](./FASE-3-pipeline-firmware.md) | Mejoras del pipeline en C++ | `peak_detector.cpp`, `tracklet_fsm.cpp`, `thermal_config.hpp`, `mlx90640_i2c_esp.cpp` |
| 4 | [FASE-4-hungarian.md](./FASE-4-hungarian.md) | Algoritmo Húngaro de asignación | `tracklet_tracker.hpp`, `tracklet_tracker.cpp` |
| 5 | [FASE-5-fov-correction.md](./FASE-5-fov-correction.md) | Corrección geométrica FOV + slider altura | `thermal_config.hpp`, `thermal_pipeline.cpp`, `tracklet_tracker.cpp`, `app.js` |

## Reglas para el agente ejecutor

1. **Leer el archivo de fase completo** antes de tocar ningún archivo de código.
2. **Localizar el bloque BUSCAR con coincidencia exacta** antes de aplicar cualquier reemplazo.
3. Si el bloque BUSCAR no coincide exactamente → **detener y reportar**.
4. Ejecutar **un cambio a la vez**, verificar antes de pasar al siguiente.
5. **No modificar** archivos no listados explícitamente en el documento de fase.
6. **No compilar** — al finalizar cada fase, notificar al usuario para que compile.
7. Los `.hpp` solo se modifican si el documento lo indica explícitamente.

## Estado de fases

- [ ] Fase 1 — Quick wins
- [x] Fase 2 — HUD redesign
- [ ] Fase 3 — Pipeline firmware
- [ ] Fase 4 — Hungarian algorithm
- [ ] Fase 5 — FOV correction
