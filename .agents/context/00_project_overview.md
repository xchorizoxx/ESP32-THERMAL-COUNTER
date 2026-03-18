# Contexto General del Proyecto — Detector de Puerta Térmico

## Descripción del Sistema
Contador cenital de personas con sensor térmico **MLX90640** (32×24 px, FOV 110°) montado a ~3.6m sobre una puerta de ~3–4.5m de ancho. Un **ESP32-S3** (dual-core) ejecuta un pipeline de visión artificial térmica y mantiene una interfaz Web HUD táctica, además de soportar flasheo inalámbrico (OTA).

## Arquitectura de Dos Núcleos (FreeRTOS)
1. **Pipeline de Procesamiento:** (Core 1)
   - Extraer temperatura ambiente vía I2C a 400kHz.
   - Filtrar fondo estático dinámicamente usando EMA.
   - Detectar "picos" térmicos espaciales (personas).
   - Seguir su movimiento (Tracking Alpha-Beta) e incrementar contadores (In/Out) si cruzan líneas virtuales.
   - Todo usa memoria estática (cero fragmentación).

2. **UI Web, Red y OTA:** (Core 0)
   - Crea un SoftAP ("ThermalCounter") en `192.168.4.1`.
   - Servidor HTTP embebido que sirve la Web UI (Dashboard HTML Canvas 2D) con `WebSockets` para emitir los tracks y la matriz de temperaturas en vivo a 16 FPS.
   - Escucha en POST `/update` para actualizaciones de firmware inalámbrico (Dual-Bank OTA).

## Entorno de Desarrollo
- **Framework**: ESP-IDF v5.5 (CMake), C++ puro.
- **Build system**: `CMakeLists.txt` en la raíz con binarios en `build/`.
- **Hardware Target**: `esp32s3`.

## Reglas Estrictas para Agentes (TÚ)
1. **Cero `malloc`/`new` en runtime** — Solo allocar objetos en la inicialización (setup).
2. **No usar `delay()`** — Usa SIEMPRE `vTaskDelay()` o `vTaskDelayUntil()`.
3. **Punto flotante**: Usa `float` (FPU de simple precisión nativa), NUNCA uses `double`.
4. **Logs**: Usa `ESP_LOGI`, `ESP_LOGE` etc., con un macro genérico estático `TAG`.
5. **No asumas el control del proceso de Build**: NUNCA ejecutes `idf.py build` o `flash` en la terminal sin pedirlo. Solo escribe el código y avisa al usuario para que él recompile usando su extensión de VS Code.
6. **Programación Orientada a Objetos**: Tareas FreeRTOS como métodos de clase usando `static void TaskWrapper(void*)` y un cast de `this`.
7. Si el usuario pide cambiar velocidad I2C, pines, o reloj, repasa el protocolo de **hardware-safety** primero.
