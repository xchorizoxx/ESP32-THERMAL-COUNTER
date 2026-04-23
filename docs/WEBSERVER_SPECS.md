# Especificación del Servidor Web (HTTP/WS)
## ESP32 Thermal Counter v2.0

El sistema utiliza un servidor web asíncrono basado en `esp_http_server` para la configuración en tiempo real y la visualización de datos.

### 1. Endpoints HTTP

| Path | Método | Función |
| :--- | :--- | :--- |
| `/` | `GET` | Sirve el `index.html` (interfaz principal). |
| `/app.js` | `GET` | Sirve la lógica de cliente. |
| `/ws` | `GET` | Upgrade a WebSocket para streaming de datos. |
| `/download_log` | `GET` | Descarga el historial CSV completo de la SD. |
| `/update` | `POST` | Interfaz para actualizaciones OTA de firmware. |
| `/reboot` | `POST` | Reinicia el ESP32 de forma controlada. |

### 2. Protocolo WebSocket (Streaming)

El servidor envía dos tipos de datos por el canal WebSocket:

#### A. Frames Binarios (Tipo 0x12)
Enviados a ~16 Hz. Maximizan la eficiencia de ancho de banda.
*   **Header (14 bytes)**: Magic (0x12), Sensor OK (u8), Temp Ambiente (f32), Count In/Out (u16), Num Tracks (u8), Session ID (u16), Time Quality (u8).
*   **Tracks (N * 11 bytes)**: ID, Posición (X,Y), Velocidad, Peak Temp.
*   **Pixeles (768 * 2 bytes)**: Datos crudos del MLX90640 (int16_t * 100).

#### B. Eventos JSON
Enviados solo cuando ocurre una acción específica (ej. un cruce de línea).
*   `type: "crossing"`: Informa dirección, temperatura del track y contadores actualizados.
*   `type: "config"`: Respuesta al comando `GET_CONFIG`.
*   `type: "status"`: Estado de salud del hardware (SD, RTC, NVS).

### 3. Comandos de Control (JSON de Cliente a ESP32)
El cliente puede enviar comandos como:
*   `{"cmd": "SET_PARAM", "param": "...", "val": ...}`
*   `{"cmd": "SAVE_CONFIG"}`: Persiste la configuración actual en NVS.
*   `{"cmd": "RETRY_SENSOR"}`: Forza el reinicio y recalibración del MLX90640.

---
*Documentación técnica del motor de red - v2.1*
