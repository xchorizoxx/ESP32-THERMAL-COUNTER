# Contador Térmico de Puertas — ESP32-S3 + MLX90640

Sistema embebido para conteo de personas mediante visión térmica (32×24 píxeles). Sin cámaras ópticas: 100% privacidad, funciona en oscuridad total.

## Especificaciones Técnicas

| Parámetro | Valor |
|-----------|-------|
| Sensor | Melexis MLX90640 (termopila 32×24, 110° FOV) |
| Procesador | ESP32-S3 dual-core @ 240MHz |
| Adquisición | 16 Hz (sub-frames) |
| Procesamiento | 8 Hz (frames completos) |
| Arquitectura | Core 1 (Visión) + Core 0 (Red/Web) |
| Tracking | TrackletTracker con historial 20 frames (Stage A2) |
| Conteo | TrackletFSM con líneas configurables (Stage A3) |
| Interfaz | Web UI vía SoftAP (192.168.4.1) |
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
  ├── Broadcast UDP (opcional, puerto 4210)
  └── Handler OTA (/update)

IPC: FreeRTOS Queue (profundidad 4, asignación estática)
```

## Inicio Rápido

1. **Hardware**: Conectar ESP32-S3 al MLX90640 vía I2C (GPIO 8/9, 400kHz)
2. **Flash**: VS Code + extensión ESP-IDF → "Build, Flash and Monitor"
3. **Conectar**: Unirse a WiFi "ThermalCounter" / contraseña: `counter1234`
4. **Configurar**: Abrir http://192.168.4.1 → ajustar umbrales → Guardar en Flash

Ver [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) para diseño detallado del sistema.

## Parámetros de Configuración Críticos

| Parámetro | Descripción | Rango Típico |
|-----------|-------------|---------------|
| Temp. Biológica | Umbral temperatura humana | 25-30°C |
| Delta T Fondo | Contraste vs fondo aprendido | 1.5-2.5°C |
| EMA Alpha | Velocidad adaptación fondo | 0.05-0.10 |
| Radio NMS Centro | Radio supresión (zona central) | 4-8 píxeles |
| Radio NMS Borde | Radio supresión (zonas bordes) | 2-4 píxeles |
| Zona Muerta Izq/Der | Zonas exclusión horizontal | 0-8 píxeles |

Guía de calibración: [`docs/CONFIGURATION.md`](docs/CONFIGURATION.md)

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

Detalles algoritmicos: [`docs/ALGORITHM.md`](docs/ALGORITHM.md)

## Conexiones de Hardware

| MLX90640 | ESP32-S3 | Nota |
|----------|----------|------|
| VCC | 3.3V | LDO estable requerido (150mA pico con WiFi) |
| GND | GND | Camino de tierra corto |
| SDA | GPIO 8 | Pull-up 1kΩ-2.2kΩ para 400kHz |
| SCL | GPIO 9 | Pull-up 1kΩ-2.2kΩ para 400kHz |

Pinout completo: [`docs/HARDWARE.md`](docs/HARDWARE.md)

## Actualizaciones OTA

```bash
# Vía script Python (conectado a WiFi ThermalCounter)
python scripts/ota_upload.py

# Vía Web UI
# Abrir http://192.168.4.1 → panel OTA → Subir build/DetectorPuerta.bin
```

Guía de operaciones: [`docs/OPERATIONS.md`](docs/OPERATIONS.md)

## Índice de Documentación

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — Arquitectura del sistema y diseño dual-core
- [`docs/ALGORITHM.md`](docs/ALGORITHM.md) — Algoritmos TrackletTracker y TrackletFSM
- [`docs/CONFIGURATION.md`](docs/CONFIGURATION.md) — Parámetros de calibración y guía Web UI
- [`docs/HARDWARE.md`](docs/HARDWARE.md) — Pinout, conexiones y especificaciones eléctricas
- [`docs/OPERATIONS.md`](docs/OPERATIONS.md) — OTA, despliegue y mantenimiento

## Estructura del Proyecto

```
├── components/
│   ├── mlx90640_driver/       # Driver sensor Melexis (I2C)
│   ├── thermal_pipeline/      # Pipeline visión (Core 1)
│   ├── telemetry/             # Stack red (Core 0)
│   └── web_server/            # Servidor HTTP + WebSocket
├── docs/
│   ├── assets/                # Capturas y videos demo
│   ├── ARCHITECTURE.md
│   ├── ALGORITHM.md
│   ├── CONFIGURATION.md
│   ├── HARDWARE.md
│   └── OPERATIONS.md
├── scripts/
│   └── ota_upload.py          # Utilidad flash OTA
├── main/
│   └── main.cpp               # Punto entrada, creación tareas
└── README.md / README_EN.md   # Este archivo (ES/EN)
```

## Changelog

- **Stage A3** (Actual): TrackletFSM con líneas configurables por segmentos, lógica debounce
- **Stage A2**: TrackletTracker (historial 20 frames, matching compuesto, memoria proporcional)
- **Stage A1**: Filtro Kalman por píxel, acumulador Chess, pipeline por etapas
- **Stage A0**: MVP inicial

## Licencia

- **Proyecto**: MIT License
- **Driver MLX90640**: Apache 2.0 (Melexis N.V.)

---

*English version: [README_EN.md](README_EN.md)*
