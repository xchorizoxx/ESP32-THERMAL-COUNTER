# Arquitectura de Sistema y Red

Este documento detalla la infraestructura técnica que permite al ESP32-S3 manejar el pesado pipeline de visión térmica y la comunicación web simultáneamente.

## 1. Diseño Multi-Núcleo (Asimétrico)

El ESP32-S3 tiene dos núcleos (Core 0 y Core 1). Hemos asignado las tareas para maximizar el determinismo:

*   **Core 1 (APP_CPU):** Ejecuta la tarea `ThermalPipe`. Es una tarea de tiempo real crítico. Su prioridad es máxima (24) para asegurar que el bus I2C nunca espere.
*   **Core 0 (PRO_CPU):** Ejecuta la tarea `TelemetryTask` y el `HTTP_SERVER`. Se encarga de la pila de red WiFi y TCP/IP.

### Comunicación Inter-Core (IPC)
Para pasar datos del Core 1 al Core 0 de forma segura sin bloqueos (Race Conditions), utilizamos **FreeRTOS Queues**.
1. El Core 1 genera un `IpcPacket`.
2. Lo envía a la `ipcQueue` con un timeout de 0 (si la cola está llena, el frame se descarta para no retrasar la visión).
3. El Core 0 recibe el paquete, lo empaqueta en binario y lo envía por el WebSocket.

## 2. Protocolo Binario WebSockets

Para ahorrar ancho de banda y CPU, no usamos JSON para enviar la imagen térmica (que son 768 números flotantes). En su lugar, enviamos un **Buffer Binario**.

### Estructura del Frame (Little-Endian):
| Posición | Tipo | Descripción |
|----------|------|-------------|
| 0 | `uint8` | Header (`0xAA`) |
| 1 | `uint8` | Sensor OK (0/1) |
| 2-5 | `float32` | Temperatura Ambiente (`Ta`) |
| 6-7 | `uint16` | Contador Entradas |
| 8-9 | `uint16` | Contador Salidas |
| 10 | `uint8` | Número de Tracks activos (`N`) |
| 11+ | `TrackInfo[N]` | Arreglo de picos (ID, X, Y, VX, VY) |
| final | `int16[768]` | Intensidad térmica (Temp * 100) |

## 3. Persistencia de Configuración (NVS)

Utilizamos el sistema **Non-Volatile Storage (NVS)** del ESP32 para guardar los parámetros de calibración.
- Los valores se guardan en el namespace `"thcfg"`.
- Los flotantes (como `EMA_ALPHA`) se guardan como `int32_t` escalados por 1000, ya que NVS no soporta floats nativos.
- **Ciclo de Vida:** Al arrancar (`vTaskStartScheduler`), el Core 0 carga los valores de NVS y los envía al Core 1. Cuando el usuario pulsa "Guardar" en la web, el Core 0 escribe en la flash y notifica al núcleo de visión.

## 4. Servidor Web Integrado

El servidor web corre sobre la pila `esp_http_server`.
- **Memoria:** El código HTML/JS/CSS está incrustado en el binario (`web_ui_html.h`) como constantes de texto comprimido. No depende de una tarjeta SD.
- **Concurrencia:** Soporta hasta 4 clientes simultáneos, aunque se recomienda conectar solo 1 o 2 para no saturar la RAM del ESP32-S3.
