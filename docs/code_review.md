# Revisión Arquitectónica y Análisis de Código (Code Review)

*Fecha de revisión: Marzo 2026*

A continuación se detallan los puntos fuertes constatados en la base de código del proyecto y las áreas donde existe una clara oportunidad de mejora para una versión orientada a producción masiva.

## 🟢 Puntos Fuertes (Fortalezas Técnicas)

1.  **Aislamiento de Prioridades (Dual Core):** 
    El haber pinnado las tareas del servidor HTTP al Core 0 y el pipeline intensivo de visión al Core 1 es un diseño excepcional. Evita que la atención de interrupciones de WiFi genere "jitter" o pérdida de sincronicidad en la tasa de refresco (16 Hz) del sensor de ritmo estricto I2C.
2.  **Emisión Binaria en Red:**
    El uso de representaciones numéricas fijas (ej. enviar las temperaturas en `int16_t` multiplicadas por 100) acorta dramáticamente el Payload del WebSocket por iteración a unos 1.5 KB. Convertir un array de 768 floats a un string JSON masivo hubiese puesto de rodillas al ESP32. El enfoque actual es rápido y eficiente.
3.  **Gestor de Configuración Central y Persistente:**
    La implementación de `SAVE_CONFIG` con acceso a `nvs_flash` mantiene separados los requerimientos operativos y el estado; aislando al analista térmico de ser reseteado o re-compilado cada vez que el microcontrolador se desenchufa.
4.  **Patrón UI Desacoplado:**
    La interpolación de imagen y dibujado de UI (Scanlines, Bounds, Matrices) se delegó al procesador gráfico del cliente conectado (Celular/PC) mediante el Canvas 2D HTML5 y escalado bi-linear asistido por GPU. Así se eludió la sobrecarga típica de escalar matrices localmente sobre C++ y se ganaron decenas de milisegundos de tiempo de máquina.

## 🟡 Áreas Críticas/Revisión Cautelar (Oportunidades de Mejora)

1.  **Dificultad de Tracking en Multitudes Crecientes (NMS ingenuo):**
    El paso del rastreador implementa supresión de No-Máximos en forma de cajas estrictas iterativas. Si dos personas caminan pegadas hombro a hombro, o se cruzan abrazados bajo el sensor, el sistema terminará uniendo sus focos de calor en un solo radiocomplejo térmico provocando fallo negativo en conteo (cuenta a 1 persona donde hay 2). Se podría mejorar usando técnicas de aglutinamiento más avanzadas *(Blob Component Labeling* moderno, *K-Means clustering* de bajas iteraciones).
2.  **Protocolo I2C Bloqueante:**
    Actualmente el comando nativo hacia el dispositivo ESP-IDF de I2C espera activamente que el esclavo MLX90640 termine cada frame DMA. A niveles estrictos de RTOS, podría transmutarse el driver de `mlx_i2c_read(...)` a una función no bloqueante basada en interrupciones, permitiendo dormir un poco más la CPU de Core 1 ahorrando batería.
3.  **Limitaciones de Pila HTTP / SoftAP:**
    El `http_server.cpp` corre sobre el stack nativo de ESP-IDF bajo un pool de workers definido con tamaño de stack fijo (8192 o 4096 bytes). Mientras un solo usuario visualice la HUD todo trabaja perfecto, si 3 o 4 smartphones entran a la misma IP, el ESP32 saturará la limitante de anclaje de WebSocket (ya que cada conexión exigirá su buf+send del frame térmico) o sobregirará el heap. Hay que definir un límite "Hard" de conexiones.
4.  **Ajuste Fijo Espacial (Alpha, Beta):**
    Los parámetros estáticos del filtro (alpha = 0.85, beta = 0.05) asumen dinámicas de velocidades muy de "personas de oficina normales". Si el dispositivo es instalado en pasillos logísticos donde pasa alguien rotando de prisa, o corriendo; los hiperparámetros de control de Kalman/Alpha-Beta van a "perder de vista" el track de esa persona porque la aceleración no coincide con el peso inercial asignado en el filtro actual.

---
**Conclusión de Revisión:** La calidad base es alta, los módulos no están espagueti y el acoplamiento es adecuado (via Queues de FreeRTOS). Listo para etapa de experimentación de campo antes de someterlo a iteraciones de refabricación algorítmica.
