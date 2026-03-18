# Tarea: Integración y Sistema de Arranque

## Objetivo
Unir todos los componentes en el `app_main()` y gestionar el ciclo de vida del sistema.

## Requisitos Técnicos
1. **Orden de Arranque**: NVS → WiFi → Colas → Pipeline → Servidor Web.
2. **Distribución de Núcleos**: Asegurar que las tareas intensivas estén pinnadas (`xTaskCreatePinnedToCore`) al núcleo correcto.
3. **Watchdogs**: Activar WDT en ambas tareas principales.
4. **Validación OTA**: Al final del arranque exitoso, marcar el firmware como "Válido" para cancelar el rollback.
5. **Estadísticas**: Tarea periódica de monitoreo de RAM y ciclos de CPU.
