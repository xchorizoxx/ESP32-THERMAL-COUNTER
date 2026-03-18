# CHANGELOG - Detector de Puerta Térmico

## [v2.6.0] - 2026-03-18 (Actual)
### Añadido
- **Sistema OTA Web**: Panel de actualización inalámbrica integrado en el Dashboard.
- **Rollback de Seguridad**: Prevención de bootloops mediante el marcado de app válida en `app_main`.
- **Script OTA**: Nueva herramienta `scripts/ota_upload.py` con barra de progreso y soporte para ESP32-S3.
- **Reestructuración de Docs**: Nuevo sistema de documentación para humanos (`docs/`) y agentes (`.agents/`).

### Cambios
- Incrementado el stack del HTTP Server a 16KB para soportar flasheo inalámbrico.
- Refactorización de la estructura de carpetas: `PROJECT_RESOURCES` eliminado en favor de `lib/` y `.agents/`.

## [v2.5.0] - Versión Estable Anterior
### Añadido
- **Filtro De-Chess**: Eliminación de artefactos de ajedrez en la imagen térmica.
- **HUD Táctico**: Interfaz web Cyberpunk con WebSockets de alta frecuencia.
- **Dual-Core**: Aislamiento estricto de Pipeline (Core 1) y Red (Core 0).
- **Control NVS**: Persistencia de calibración.

---
*Para ver el roadmap de futuras versiones, consultar [docs/roadmap.md](docs/roadmap.md).*
