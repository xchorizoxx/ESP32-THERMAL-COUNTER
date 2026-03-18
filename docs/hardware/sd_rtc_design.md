# Diseño e Implementación: SD Card y RTC (V3 Roadmap)

Este documento consolida la arquitectura lógica y topología de hardware para la futura implementación de grabación de clips térmicos en tarjeta SD con estampa de tiempo real (RTC).

## 1. Concepto del Sistema "Dashcam" Térmico
La meta principal es permitir que el dispositivo guarde en una memoria SD un clip exacto de qué vio la cámara **justo antes y después** de que una persona cruzara la puerta (evento de IN/OUT).

Para esto se requiere **Pre-Grabación** mediante un Ring Buffer estático.

## 2. Flujo Lógico y Memoria en FreeRTOS

1.  **El Buffer Circular (Ring Buffer):**
    Una clase estática pre-aloja ~36 KB de RAM (para 24 frames @ 8 FPS = 3 segundos de pre-grabación). Esto ocurre en el Core 1 y está protegido por Mutex.
2.  **El Evento de Disparo:**
    Cuando el detector confirma un cruce, emite un *Event Group* no-bloqueante a una tarea inactiva en el Core 0.
3.  **Captura de Tiempo:**
    Se consulta al RTC por I2C su objeto `DateTime` actual para nombrar el log (`20241103_091522_OUT.bin`).
4.  **Dumping Asíncrono:**
    La `SDRecorderTask` toma el snapshot del ring buffer (3s en el pasado), espera 2 segundos adicionales (post-grabación) y vuelca el archivo completo (~60 KB) al bus SPI sin frenar a la cámara que sigue en I2C.

## 3. Hardware y Pinout (Propuesta S3)

### Reloj RTC DS3231 (Bus I2C)
Comparte el mismo bus físico de la cámara térmica.
*   **VCC:** 3.3V
*   **SDA / SCL:** Mismo pinout que el MLX90640 (GPIO 8 y 9)
*   **Dirección:** `0x68`

### Módulo MicroSD (Bus SPI)
El volcado se hace mediante `sdspi_host`.
*   **VCC:** 3.3V o 5V (según regulador)
*   **MISO:** GPIO 19
*   **MOSI:** GPIO 23
*   **SCK:** GPIO 18
*   **CS (Chip Select):** GPIO 32 (Exclusivo para la tarjeta)

> [!CAUTION]
> Evitar pines de strapping (0, 2, 5, 12, 15) como Chip Select de la SD para prevenir fallos de arranque. Evitar pines analógicos (34-39) si planean usarse ADCs en el futuro. 
