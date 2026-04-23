# Detalle de Periféricos de Hardware
## ESP32 Thermal Counter v2.0

### 1. Sensor Térmico (MLX90640)
*   **Bus**: I2C0 (400kHz - 1MHz).
*   **Resolución**: 32x24 (768 píxeles).
*   **Refresh Rate**: Configurado a 16Hz para balancear fluidez y ruido térmico.
*   **Lógica**: El driver realiza lecturas directas a la RAM del sensor y aplica la compensación de temperatura (VDD, Ta, CP) según la hoja de datos de Melexis.

### 2. Reloj de Tiempo Real (DS3231)
*   **Bus**: I2C1 (Canal independiente para evitar colisiones con el sensor).
*   **Precisión**: Compensación de temperatura integrada (TCXO).
*   **Resiliencia**: El sistema detecta automáticamente si el módulo está presente. Si no, conmuta a modo "Relative Ticks".

### 3. Almacenamiento MicroSD
*   **Interfaz**: SPI (HSPI/SPI2).
*   **Punto de Montaje**: `/sdcard`.
*   **Directorio de Trabajo**: 
    *   `/logs/`: Archivos CSV de conteo.
    *   `/clips/`: Reservado para capturas térmicas futuras.
*   **Seguridad**: El sistema cierra el archivo inmediatamente después de cada escritura para prevenir corrupción en caso de pérdida de energía.

### 4. LED de Estado (WS2812B / NeoPixel)
El LED integrado (GPIO 48) proporciona feedback visual inmediato sin necesidad de la web:

| Color | Comportamiento | Significado |
| :--- | :--- | :--- |
| **Azul** | Fijo | Iniciando sistema / Booting. |
| **Verde** | Fijo | Operación normal (Todo OK). |
| **Amarillo** | Parpadeo | Falta RTC o MicroSD (Operación degradada). |
| **Rojo** | Fijo | Error crítico (Sensor MLX no detectado). |
| **Blanco** | Destello | Se ha detectado y registrado un cruce de persona. |

---
*Referencia de hardware y señales - v2.1*
