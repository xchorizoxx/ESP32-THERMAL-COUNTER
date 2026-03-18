# Propuestas de Implementación Futura (V3 & V4 Roadmap)

Pensando en la escalabilidad a largo plazo, seguridad e inteligencia operacional del Contador de Puerta ESP32; la arquitectura en C++ diseñada abre la puerta a las siguientes evoluciones orgánicas:

## 1. Integración con Machine Learning (Edge AI real)
El algoritmo *Alpha-Beta* y *NMS* basado en física térmica cruda es excepcionalmente rápido, pero puede engañarse fácilmente si se pasa un "Reno Mapeado" o si entra una mascota (perro grande) u objetos calientes como un carrito de alimentos industriales.
**Propuesta:** 
Reemplazar el Bloque Matemático del Core 1 por un motor inferencial **TensorFlow Lite for Microcontrollers**. Se puede entrenar una red neuronal pequeña convolucional (CNN) alimentada de miles de capturas 32x24 del comportamiento térmico humano desde el techo. Esto añadiría "Segmentación Semántica" capaz de distinguir entre "Cuerpo Superior Humano" y "Mascota/Fuente de Calor anónima".

## 2. Telemetría a Nivel Entorno Empresarial (MQTT / InfluxDB)
Hoy en día, el valor del sistema radica en mirar la HUD manualmente. Para ser un sistema IoT corporativo:
**Propuesta:** 
Cambiar la arquitectura del `wifi_init` de modo `WIFI_AP` excluyente, a modo `WIFI_STA` para que el ESP32 se conecte a la red del edificio como un cliente mudo. Desde ahí, publicar un paquete JSON mínimo `{ counts_in: 19, counts_out: 4, tz: "UTC"}` cada 60 segundos hacia un broker **MQTT** interno. Esto permitiría a gerentes integrar los datos directamente en Grafana/HomeAssistant o un servidor en la Nube corporativo de métricas de ocupación sin modificar lo ya programado.

## 3. Auto-Calibración Biométrica (Auto Tunning)
Es probable que instalar un sensor en 50 puertas requiera mover manualmente el *Slider* de `Radio NMS` por cada puerta, ya que todas difieren en grosor o distancia del techo (entre 2.0 y 5.0 metros).
**Propuesta:**
Programar una rutina de inicialización de "24 horas estáticas". El sensor usará la Inteligencia Artificial o detección probabilística para evaluar el tamaño en píxeles promedio de los bultos calientes detectados. Tras el análisis, el propio sistema autoseleccionará los valores de `NMS_CENTER` y `NMS_EDGE` y redefinirá de forma automática dónde caen las líneas divisorias `line_entry` y `line_exit` garantizando cero margen de error durante la instalación por un no-técnico.

## 4. Actualizaciones Inalámbricas (OTA Subsystem)
Actualmente hay que usar el arnés físico USB serial del ESP32-S3 para flashear. 
**Propuesta:**
Añadir una partición `app0/app1` en el archivo de tabla de particiones `.csv` del ESP-IDF. Luego habilitar el componente `esp_https_ota`. Añadir un pequeño botón gris en la HUD Web que diga `Actualizar Firmware`. Al presionarlo, le abre un `<input type="file">` al usuario, permitiendo pasar un archivo `.bin` y escribiendo bit-a-bit en flash on-the-fly. Esto facilita infinitamente la depuración y parches urgentes de sistemas instalados en cielo-raso profundo.

## 5. Implementación de Visión Estereoscópica Térmica
Para entradas excesivamente anchas o tiendas donde varias personas crucen, un MLX90640 (campo de 110x75°) empieza a deformar los extremos y sufre deficiencias de ángulo ciego.
**Propuesta:** 
Hacer uso del mismo firmware pero un *I2C Multiplexor* o utilizar los dos buses I2C nativos (I2C0, I2C1) del ESP32S3, para montar internamente **2 sensores cruzados superpuestos**. Esto entregaría no solo una resolución superior de `64x24` térmica, sino auténtica paralelisis geométrica; proveyendo valores reales de profundidad en un espacio tridimensional.

## 6. Grabación de Clips Térmicos en Tarjeta SD (con RTC)
El sistema actual procesa los frames térmicos al vuelo para contarlos. Para auditorías, depuración de algoritmos o verificación humana posterior, es extremadamente útil poder guardar "clips de video térmico" al estilo *Dashcam* solo cuando ocurre un evento de cruce.
**Propuesta:**
Integrar un módulo MicroSD por bus SPI y un Reloj en Tiempo Real (RTC, ej: DS3231) por el mismo bus I2C de la cámara. Cargar el entorno `esp_vfs_fat` para montar el sistema de archivos de la SD. Esto permitirá guardar pequeños archivos `.bin` con la matriz térmica de los últimos N frames previos y posteriores al cruce, usando el RTC para nombrar el archivo con un *timestamp* absoluto (ej: `20241026_143005_IN.bin`).
*Para consultar la viabilidad técnica y los detalles de esta implementación, ver los documentos dedicados en `docs/sd_rtc_implementation/`.*
