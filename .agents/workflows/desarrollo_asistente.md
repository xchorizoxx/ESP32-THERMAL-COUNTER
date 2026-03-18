---
description: Proceso de desarrollo y arquitectura — Detector de Puerta Térmico
---

# 🤖 Flujo de Trabajo del Asistente Antigravity

Este documento describe el "Workflow" interno que he seguido para desarrollar este proyecto de forma autónoma, desde el análisis hasta la implementación final.

## 1. Análisis de Recursos y Requisitos
- **Herramientas**: `list_dir`, `view_file`.
- **Acción**: Identifiqué los recursos en `PROJECT_RESOURCES/`. Leí el `Borrador-Proyecto.md` para entender el pipeline matemático y analicé la librería C de Melexis para convertirla a C++ OOP.

## 2. Propuesta de Arquitectura (Implementation Plan)
- **Herramientas**: `write_to_file` (Artifact).
- **Acción**: Antes de escribir una sola línea de código, generé un `implementation_plan.md` con:
  - Estructura de 3 componentes ESP-IDF.
  - Diagrama de flujo del pipeline.
  - Definición de pines (GPIO 8/9).
  - Estrategia de multitarea (Core 0 para telemetría, Core 1 para pipeline).

## 3. Desarrollo Modular por Capas (Bottom-Up)
Seguí un orden lógico para asegurar que cada pieza tuviera sus dependencias listas:
1. **Tipos y Configuración**: `thermal_types.hpp` y `thermal_config.hpp`.
2. **Driver Hardware**: Wrapper `Mlx90640Sensor` encapsulando I2C de ESP-IDF.
3. **Pipeline Matemático**: Implementación de los 5 pasos (Fondo, Picos, NMS, Tracker, Máscara) como clases independientes para facilitar el testeo unitario.
4. **Telemetría**: Gestión de SoftAP y sockets UDP en el segundo núcleo.
5. **Integración**: `main.cpp` orquestando todo con tareas estáticas.

## 4. Gestión de Memoria y Seguridad
- **Sin Fragmentación**: Usé `xTaskCreateStatic` y `xQueueCreateStatic`.
- **Rendimiento**: Convertí floats a `int16` para reducir el ancho de banda del WiFi a la mitad.
- **Robustez**: Implementé un mecanismo de reset del bus I2C automático si el sensor deja de responder.

## 5. Documentación para el Futuro
- **Agent Instructions**: Creé 7 archivos en `AGENT_INSTRUCTIONS/` para que si otro agente entra mañana, sepa exactamente qué hace cada módulo sin tener que leer todo el código.
- **Walkthrough**: Resumen final de la implementación.
- **Guía de Calibración**: Específicamente para el usuario final.

---

### // turbo-all
Para ejecutar el build una vez terminados los cambios:
1. Asegúrate de tener el target configurado (`idf.py set-target esp32s3`)
2. Compila el proyecto desde el IDE para verificar que todo el flujo de trabajo fue exitoso.
