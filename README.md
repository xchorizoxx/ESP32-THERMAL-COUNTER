# 🛰️ Detector de Puerta Térmico · Edge AI HUD

![Version](https://img.shields.io/badge/Versi%C3%B3n-V2.5--Advanced--Debug-blue)
![Platform](https://img.shields.io/badge/Platform-ESP32--S3-orange)
![Framework](https://img.shields.io/badge/Framework-ESP--IDF--v5.5-red)

Sistema de visión artificial térmica de alto rendimiento para el conteo de personas en accesos, diseñado para **análisis en el borde (Edge Computing)**. Utiliza un sensor Melexis MLX90640 y un microcontrolador ESP32-S3 para procesar imágenes térmicas, rastrear objetivos y gestionar estadísticas sin comprometer la privacidad.

---

## 📖 Índice
1. [Visión General](#visión-general)
2. [Sustento Arquitectónico](#sustento-arquitectónico)
3. [El Pipeline de Visión (Algoritmo)](#el-pipeline-de-visión-algoritmo)
4. [Interfaz HUD Táctica](#interfaz-hud-táctica)
5. [Guía de Calibración](#guía-de-calibración)
6. [Especificaciones Técnicas](#especificaciones-técnicas)
7. [Instalación y Despliegue](#instalación-y-despliegue)

---

## 🛡️ Visión General

A diferencia de las cámaras convencionales, este sistema utiliza una matriz de **32x24 termopilas**. Cada píxel es una medición de temperatura real. Esta naturaleza del dato garantiza:
- **Privacidad Total:** No se capturan rostros ni rasgos identificables.
- **Inmunidad Lumínica:** Funciona en oscuridad total o bajo luz solar directa.
- **Detección Biométrica:** Se basa en la firma térmica humana (~30-36°C) diferenciándola de objetos inanimados.

---

## 🏗️ Sustento Arquitectónico

El sistema explota la arquitectura **Dual-Core** del ESP32-S3 mediante una división asimétrica de tareas utilizando **FreeRTOS**:

### Core 1: El Motor de Visión (`ThermalPipe`)
Es el núcleo de mayor prioridad. Ejecuta el bucle de procesamiento matemático a **16 Hz**.
- **Determinismo:** Utiliza `vTaskDelayUntil` para garantizar un muestreo exacto del sensor.
- **Seguridad I2C (400kHz):** Configurado a Fast Mode para garantizar integridad de datos y evitar corrupción de EEPROM ante ruidos eléctricos.
- **Aislamiento:** No realiza tareas de red pesadas para evitar interferencias en el bus de datos.
- **Static Memory:** Todos los buffers de imagen están pre-asignados estáticamente para evitar fragmentación del Heap.

### Core 0: Comunicaciones y Telemetría (`TelemetryTask`)
Gestiona la capa externa del sistema.
- **SoftAP & Servidor Web:** Levanta un punto de acceso WiFi y sirve el Dashboard táctico.
- **WebSockets Binarios:** Empaqueta los datos procesados en estructuras `packed` de C para su transmisión eficiente.
- **NVS Flash:** Gestiona la persistencia de la calibración para que los ajustes sobrevivan a cortes de energía.

---

## 🧠 El Pipeline de Visión (Algoritmo)

El procesamiento se divide en 5 estadios secuenciales que transforman ruido térmico en eventos de conteo:

### 1. Pre-procesado y Filtro "De-Chess"
Se aplica un algoritmo de interpolación espacial ortogonal que funde cada píxel con sus vecinos para eliminar la malla de ajedrez nativa del MLX90640. 
- Elimina el ruido "Salt & Pepper" (píxeles muertos).
- Suaviza la imagen sin perder la firma térmica humana.

### 2. Sustracción de Fondo Dinámico (EMA)
Utiliza un modelo de **Media Móvil Exponencial (EMA)** para aprender la temperatura ambiente.
- Si un objeto caliente permanece estático (ej: una cafetera), el sistema lo "absorbe" en el fondo tras unos minutos.
- **Máscara de Retroalimentación:** Los tracks activos "congelan" el aprendizaje del fondo bajo ellos para evitar que el sistema borre a una persona que se queda quieta.

### 3. Detección de Picos y NMS
- **Peak Detection:** Encuentra máximos locales que superen el `TEMP_BIOLOGICO_MIN` y el `DELTA_T_FONDO`.
- **NMS (Non-Maximum Suppression):** Une múltiples picos cercanos en un solo centro de masa. Es vital para evitar que una sola cabeza caliente se cuente como 5 personas distintas.

### 4. Tracking Alpha-Beta con Verificación de Identidad
Implementa un filtro predictivo para seguir a las personas entre cuadros.
- **Anti-Stealing (V2.5):** Garantiza que cada "pico" de calor sea asignado a un único track, evitando que personas cercanas borren la identidad de su vecino.
- Calcula el vector de velocidad `(vx, vy)`.
- Gestiona la "vida" del track: si una persona desaparece por 5 cuadros, el sistema la elimina para no generar fantasmas.

### 5. Lógica de Conteo (Histéresis + Inferencia de Intención)
Define dos líneas virtuales `Y`. El conteo se dispara cuando un ID de track cruza ambas líneas.
- **Inferencia de Intención (V2.5):** Si una persona es detectada por primera vez en la zona media (neutral), el sistema utiliza su **vector de velocidad vertical** para decidir si el cruce de línea cuenta como entrada o salida, eliminando el fallo por "aparición súbita".

---

## 🖥️ Interfaz HUD Táctica

El sistema incluye una interfaz web de estilo **Cyberpunk/Tactical HUD** diseñada para ingeniería de campo:

- **Interpolación Bilineal:** El navegador reescala la matriz de 32x24 a 640x480 usando la GPU, creando una imagen suave ("blur") en lugar de bloques pixelados.
- **Vectores de Velocidad:** Cada persona rastreada muestra una flecha amarilla indicando hacia dónde y qué tan rápido se mueve.
- **Modo Radar:** Permite ver el "residuo térmico" (imagen restada). Es ideal para depurar si el fondo se está aprendiendo correctamente.
- **Telemetría Ta:** Muestra la temperatura interna del silicio para monitorear el estrés térmico del sensor.

---

## ⚙️ Guía de Calibración

El sistema es altamente flexible gracias a los parámetros accesibles vía Web:

| Parámetro | Función | Cuándo ajustar |
|-----------|---------|----------------|
| **Temp. Biológica** | Umbral mínimo (°C) | Si el ambiente es muy caluroso (>30°C), subir este valor. |
| **Delta Fondo** | Contraste vs Pared | Si hay mucha sombra térmica, subir para evitar falsos positivos. |
| **Adaptación EMA** | Velocidad de aprendizaje | Aumentar si la temperatura del local cambia bruscamente (Aire Acondicionado). |
| **Radio NMS** | Tamaño de la "persona" | **Dato Crave:** Ajustar según la altura del techo. Techos altos requieren radios más pequeños. |
| **Líneas Y** | Zonas de activación | Mover para que queden justo sobre el dintel de la puerta en la vista. |

**Flujo recomendado:**
1. Ajustar parámetros para una detección limpia.
2. Hacer clic en **APLICAR AJUSTES** para probar en vivo.
3. Hacer clic en **GUARDAR EN FLASH** para que la configuración sea permanente.

---

## 📊 Especificaciones Técnicas

- **Sensor:** Melexis MLX90640 (Matriz de Termopilas).
- **Resolución:** 32 x 24 píxeles (768 puntos de medición).
- **Campo de Visión (FOV):** 110° x 75° (Gran angular).
- **Frecuencia de Procesamiento:** 16 FPS constantes.
- **Consumo:** ~120mA (WiFi activo + Procesamiento).
- **Precisión:** ±1.5°C típica.

---

---

## 🛠️ Actualizaciones OTA (Over-The-Air)
El sistema soporta actualizaciones inalámbricas de firmware. No necesitas conectar el cable USB una vez instalado el sensor.
- **Dashboard Web**: Sube el archivo `.bin` directamente desde el panel "Actualización OTA".
- **Script Directo**: Usa `python scripts/ota_upload.py` para flashear remotamente desde tu terminal.

Para más detalles, consulta la [Guía de Flasheo OTA](docs/operations/ota_flasheo.md).

---

## 📂 Estructura del Proyecto

- **`docs/`**: Documentación técnica detallada.
  - `algorithm/`: Explicación matemática del pipeline de visión.
  - `hardware/`: Esquemas de conexión y diseño de extensiones (SD/RTC).
  - `operations/`: Guías de calibración, despliegue y flasheo OTA.
- **`.agents/`**: Contexto optimizado para asistentes de IA (Antigravity).
- **`components/`**: Módulos C++ (Driver, Pipeline, Telemetría, Servidor Web).
- **`scripts/`**: Herramientas de utilidad para desarrollo.
- **`lib/`**: Librerías externas de referencia.

---

## 🚀 Instalación y Despliegue
Consulta la [Guía de Despliegue](docs/operations/despliegue.md) para el primer setup.

---

> [!IMPORTANT]
> **Seguridad Industrial:** Este dispositivo es un sistema de conteo y análisis de flujo de personas. No debe utilizarse para diagnósticos médicos o seguridad crítica.

---
*Desarrollado como una solución de visión térmica embebida de alta eficiencia.*

