# Contador Térmico de Puertas — ESP32-S3 + MLX90640

Sistema embebido para conteo de personas mediante visión térmica (32×24 píxeles). Sin cámaras ópticas: 100% privacidad, funciona en oscuridad total.

## Especificaciones Técnicas

| Parámetro | Valor |
|-----------|-------|
| Sensor | Melexis MLX90640 (termopila 32×24, 110° FOV) |
| Procesador | ESP32-S3 dual-core @ 240MHz |
| Adquisición | 32 Hz (sub-frames) |
| Procesamiento | 16 Hz (frames completos) |
| Arquitectura | Core 1 (Visión) + Core 0 (Red/Web) |
| Tracking | TrackletTracker con historial 20 frames (Stage A2) |
| Conteo | TrackletFSM con líneas configurables (Stage A3) |
| Interfaz | Web UI vía SoftAP (192.168.4.1) |
| Almacenamiento | MicroSD (FATFS sobre SPI2) |
| Reloj Real | RTC DS3231 (I2C1) con backup |
| Flash | 16MB (Particiones de App de 4MB) |
| Actualizaciones | OTA vía endpoint `/update` |

## Arquitectura de Software

```
[Core 1] ThermalPipeline (prioridad 24, 16 Hz)
  ├── MLX90640 Driver (I2C 400kHz, Fast Mode)
  ├── FrameAccumulator (fusión modo Chess)
  ├── NoiseFilter (Kalman 1D por píxel)
  ├── BackgroundModel (EMA selectivo)
  ├── PeakDetector (detección de máximos locales)
  ├── NmsSuppressor (radio adaptativo: centro vs bordes)
  ├── TrackletTracker (historial 20 frames, matching compuesto)
  └── TrackletFSM (conteo bidireccional, zonas muertas)

[Core 0] TelemetryTask + HTTP Server (prioridad 2-5)
  ├── WiFi SoftAP "ThermalCounter"
  ├── WebSocket binario (1.5 KB/frame, 16 FPS)
  ├── RTC Driver (Sincronización DS3231 vs Soft-clock)
  ├── SD Manager (Logging de eventos CSV en MicroSD)
  ├── Health Monitor (Estado hardware en tiempo real)
  └── Handler OTA (/update)

IPC: FreeRTOS Queue (profundidad 4, asignación estática)
```

## Características Principales

- **Detección Sub-píxel**: Centroides de calor calculados con precisión sub-píxel para trayectorias suaves.
- **Conectividad Dual**: WiFi (SoftAP) para visualización y **USB (RNDIS/ECM)** para acceso local directo.
- **Feedback Visual**: LED RGB (GPIO 48) para estados (Boot: Azul, Operativo: Verde, USB: Morado).
- **Web UI Responsiva**: Layout adaptable para móviles y monitores de PC (Vista Dividida).
- **Control de Privacidad**: Procesamiento local; sin datos en la nube.
- **Configuración Dinámica**: Parámetros ajustables desde la web y persistentes en NVS.

## Inicio Rápido

1. **Hardware**: Conectar ESP32-S3 al MLX90640 vía I2C (GPIO 8/9).
2. **Botón BOOT**: Mantener presionado 2s para activar el **Modo USB Network**.
3. **Flash**: VS Code + extensión ESP-IDF → "Build, Flash and Monitor".
4. **Conectar**: WiFi "ThermalCounter" o cable USB (192.168.4.1).
5. **Configurar**: Abrir navegador → ajustar umbrales → Guardar en Flash.

Ver [`docs/01-architecture.md`](docs/01-architecture.md) para diseño detallado del sistema.

## Parámetros de Configuración Críticos

| Parámetro | Descripción | Rango Típico |
|-----------|-------------|---------------|
| Temp. Biológica | Umbral temperatura humana | 25-30°C |
| Delta T Fondo | Contraste vs fondo aprendido | 1.5-2.5°C |
| EMA Alpha | Velocidad adaptación fondo | 0.05-0.10 |
| Radio NMS Centro | Radio supresión (zona central) | 4-8 píxeles |
| Radio NMS Borde | Radio supresión (zonas bordes) | 2-4 píxeles |
| Zona Muerta Izq/Der | Zonas exclusión horizontal | 0-8 píxeles |

Guía de calibración: [`docs/04-configuration.md`](docs/04-configuration.md)

## Pipeline de Visión (5 Etapas)

**Etapa 1 — Adquisición**: MLX90640 en modo Chess, 16 Hz sub-frames (píxeles pares/impares alternados).

**Etapa 2 — Pre-procesamiento**:
- FrameAccumulator: Fusiona sub-frames en frames 32×24 completos
- NoiseFilter: Kalman 1D por píxel (reduce ruido NETD)

**Etapa 3 — Modelado de Fondo**: EMA selectivo. Píxeles bajo tracks activos se congelan para evitar absorción.

**Etapa 4 — Detección**:
- Detección picos: Máximos locales sobre `BIOLOGICAL_TEMP_MIN` con contraste `BACKGROUND_DELTA_T`
- NMS: Radio adaptativo (mayor en centro donde la distorsión es menor)

**Etapa 5 — Tracking y Conteo**:
- TrackletTracker: Historial 20 frames para estimación de velocidad
- Matching compuesto: Distancia + similitud de temperatura
- TrackletFSM: Conteo bidireccional con líneas configurables por segmentos

Detalles algoritmicos: [`docs/02-algorithm.md`](docs/02-algorithm.md)

## Conexiones de Hardware

| Bus | Señal | GPIO | Nota |
|-----|-------|------|------|
| I2C0 (MLX90640) | SDA | GPIO 8 | 1 MHz FM+, pull-ups 1kΩ externos |
| I2C0 (MLX90640) | SCL | GPIO 9 | 1 MHz FM+, pull-ups 1kΩ externos |
| I2C1 (DS3231) | SDA | GPIO 7 | Velocidad estándar |
| I2C1 (DS3231) | SCL | GPIO 6 | Velocidad estándar |
| I2C1 (DS3231) | VCC | GPIO 15 | Control alimentación RTC |
| I2C1 (DS3231) | GND | GPIO 16 | Interruptor masa RTC |
| SPI2 (SD) | MOSI | GPIO 13 | MicroSD |
| SPI2 (SD) | MISO | GPIO 14 | MicroSD |
| SPI2 (SD) | SCK | GPIO 12 | MicroSD |
| SPI2 (SD) | CS | GPIO 11 | MicroSD |

Pinout completo: [`docs/03-hardware.md`](docs/03-hardware.md)

## Actualizaciones OTA

```bash
# Vía script Python (conectado a WiFi ThermalCounter)
python scripts/ota_upload.py

# Vía Web UI
# Abrir http://192.168.4.1 → panel OTA → Subir build/DetectorPuerta.bin
```

Guía de operaciones: [`docs/06-operations.md`](docs/06-operations.md)

## Índice de Documentación

- [`docs/01-architecture.md`](docs/01-architecture.md) — Arquitectura del sistema y diseño dual-core
- [`docs/02-algorithm.md`](docs/02-algorithm.md) — Algoritmos TrackletTracker y TrackletFSM
- [`docs/03-hardware.md`](docs/03-hardware.md) — Pinout, conexiones y especificaciones eléctricas
- [`docs/04-configuration.md`](docs/04-configuration.md) — Parámetros de calibración y guía Web UI
- [`docs/05-webserver.md`](docs/05-webserver.md) — API HTTP, WebSocket, OTA
- [`docs/06-operations.md`](docs/06-operations.md) — OTA, despliegue y mantenimiento
- [`docs/07-peripherals.md`](docs/07-peripherals.md) — Drivers de periféricos
- [`docs/08-data-persistence.md`](docs/08-data-persistence.md) — Almacenamiento, NVS, CSV
- [`docs/09-status-indicators.md`](docs/09-status-indicators.md) — Códigos LED y USB

## Estructura del Proyecto

```
├── .agents/                   # Instrucciones IA y planes
├── components/
│   ├── mlx90640_driver/       # Driver sensor Melexis (I2C0)
│   ├── rtc_driver/            # Driver RTC DS3231 (I2C1)
│   ├── sd_manager/            # Gestor SD FATFS
│   ├── status_led/            # Driver LED RGB
│   ├── telemetry/             # Stack red, LogWriter (Core 0)
│   ├── thermal_pipeline/      # Pipeline visión (Core 1)
│   ├── thermal_recorder/      # Grabador de clips térmicos
│   └── web_server/            # HTTP + WebSocket + OTA
├── docs/
│   ├── assets/
│   │   ├── images/
│   │   └── videos/
│   ├── 01-architecture.md
│   ├── 02-algorithm.md
│   ├── 03-hardware.md
│   ├── 04-configuration.md
│   ├── 05-webserver.md
│   ├── 06-operations.md
│   ├── 07-peripherals.md
│   ├── 08-data-persistence.md
│   ├── 09-status-indicators.md
│   ├── CHANGELOG.md
│   ├── README.md
│   └── ROADMAP.md
├── managed_components/
├── scripts/
│   └── ota_upload.py
├── main/
│   └── main.cpp
└── README.md / README_ES.md
```

## Changelog

- **v1.0.0-estable** (Actual): Organización por sesión, CSV rediseñado, exportación de configuración, descargas ZIP, gestión de clips, documentación reestructurada.
- **v0.9.5-alpha**: Arquitectura dual-core, logging SD, OTA, modo USB network.
- **v0.8.1-alpha**: TrackletFSM, líneas de conteo configurables, persistencia NVS.

## Licencia

- **Proyecto**: MIT License
- **Driver MLX90640**: Apache 2.0 (Melexis N.V.)

### 📂 Gestión de Datos y Persistencia
- **Registro Histórico en SD**: Guarda cada cruce en `/sdcard/logs/counts.csv` de forma persistente.
- **Respaldo en NVS**: Los contadores totales se guardan cada 10 minutos en la Flash interna.
- **Descarga vía Web**: Botón dedicado para bajar el historial completo en formato CSV.
- **Sesión Trazable**: ID de sesión incremental para separar datos tras reinicios.

Para más detalles, consulta la [Documentación de Persistencia](docs/08-data-persistence.md).

### 🌐 Interfaz Web Avanzada
- **Streaming en Tiempo Real**: Visualización térmica a 16 FPS mediante WebSockets.
- **Protocolo Binario**: Optimizado para baja latencia en redes WiFi congestionadas.
- **Panel de Control**: Configuración dinámica de líneas, sensibilidad y altura.

Consulta la [Especificación del Servidor Web](docs/05-webserver.md).

---

*English version: [README_EN.md](README_EN.md)*
