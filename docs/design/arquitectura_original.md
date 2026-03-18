Documento de Arquitectura y Flujo de Trabajo
Proyecto: Sistema de Conteo Térmico de Personas (MLX90640) 
Hardware Base: ESP32-S3 (Dual-Core) 
Entorno de Ejecución: C++ / FreeRTOS

## 1. Visión General de la Arquitectura
El sistema utiliza un enfoque de multiprocesamiento asimétrico (AMP) administrado por FreeRTOS. El procesamiento matemático se aísla por completo de la gestión de red para garantizar un pipeline de visión de 16 Hz constantes.

## 2. Asignación de Núcleos y Tareas (FreeRTOS)
- **Core 1 (APP_CPU):** El Motor de Visión. Tarea de alta prioridad que lee el I2C y ejecuta el pipeline de 5 pasos (Fondo EMA, Picos, NMS, Tracking, Conteo).
- **Core 0 (PRO_CPU):** El Nodo de Telemetría. Gestiona Wi-Fi, SoftAP, HTTP Server y WebSockets.

## 3. El Pipeline de Visión (Core 1)
1. **Paso 0 - Adquisición:** Lectura I2C del sensor MLX90640 (32x24 píxeles).
2. **Paso 1 - Fondo Dinámico:** Sustracción de fondo mediante Media Móvil Exponencial (EMA) con máscara de bloqueo.
3. **Paso 2 - Topología:** Detección de picos locales por encima del umbral biológico y temperatura del fondo.
4. **Paso 3 - NMS Adaptativo:** Supresión de no-máximos ordenados por temperatura para consolidar tracks.
5. **Paso 4 - Tracking y Lógica:** Filtro Alpha-Beta para suavizar trayectorias y lógica de cruce de líneas por histéresis.
6. **Paso 5 - Retroalimentación:** Generación de máscara de bloqueo para el siguiente frame basada en tracks activos.

---
*Este es el documento de diseño original que sirvió de base para la implementación V1/V2.*
