# Persistencia de Datos y Registro (SD/NVS)
## ESP32 Thermal Counter v2.0

El sistema garantiza la integridad de los datos de conteo mediante una arquitectura de persistencia dual: volátil (RAM), persistente de sesión (NVS) e histórica (SD).

### 1. Registro en MicroSD (`LOGS.CSV`)

El archivo maestro se encuentra en `/sdcard/logs/counts.csv`. Es un archivo de texto plano en formato CSV accesible vía web.

#### Estructura del Archivo (Schema):
```csv
timestamp_ms,session_id,event_type,code,data_json,count_in,count_out
```
*   **timestamp_ms**: Tiempo Unix en milisegundos (si hay RTC) o tiempo desde el arranque. Es la clave primaria para análisis temporal.
*   **session_id**: ID incremental que cambia en cada reinicio del sistema para separar logs.
*   **event_type**: Tipo de registro (`CROSSING`, `ERROR`, `BOOT`, `CONFIG`).
*   **code**: Código específico del evento (ej. "IN", "OUT" o códigos de error como "E101").
*   **data_json**: Metadatos adicionales en formato JSON (ej. `{"id":5, "temp":36.5}`).
*   **count_in/out**: Estado de los contadores totales tras el evento.

### 2. Persistencia en NVS (Non-Volatile Storage)

La NVS de la Flash interna del ESP32 se utiliza para datos críticos de pequeño tamaño que deben sobrevivir a reinicios:

| Key | Tipo | Función |
| :--- | :--- | :--- |
| `nvs_base_in` | `i32` | El acumulado histórico de entradas. |
| `nvs_base_out` | `i32` | El acumulado histórico de salidas. |
| `session_id` | `u16` | Contador de veces que el sistema ha arrancado. |
| `config` | `blob` | Estructura con todos los parámetros (sensibilidad, líneas, etc). |

#### Estrategia de Guardado:
*   **Respaldo cada 10 min**: Un temporizador automático guarda los contadores actuales en NVS para evitar pérdida de datos por corte de luz si no hay SD.
*   **Escritura Manual**: El usuario puede forzar el guardado de la configuración mediante el botón "Guardar en Flash".

### 3. Sincronización Horaria (RTC DS3231)

El sistema utiliza un módulo RTC externo para garantizar que los timestamps sean correctos incluso sin conexión a Internet.
*   **Falla de RTC**: Si el RTC no está presente, el sistema utiliza el tiempo proporcionado por el navegador del primer cliente que se conecte.
*   **Drift**: El sistema se re-sincroniza automáticamente con el reloj del navegador si detecta una deriva mayor a 2 segundos.

---
*Especificación de almacenamiento y tiempo - v2.1*
