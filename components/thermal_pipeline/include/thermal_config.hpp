#pragma once
/**
 * @file thermal_config.hpp
 * @brief Centro neurálgico de parámetros configurables del sistema.
 *
 * TODOS los valores que dependen del entorno, sensor, o instalación
 * se centralizan aquí. Para adaptar el sistema a una nueva puerta,
 * solo hay que editar este archivo.
 */

#include <cstdint>

namespace ThermalConfig {

// =========================================================================
//  HARDWARE I2C
// =========================================================================
constexpr int I2C_SDA_PIN = 8;      // GPIO para SDA del MLX90640
constexpr int I2C_SCL_PIN = 9;      // GPIO para SCL del MLX90640
constexpr uint8_t MLX_ADDR = 0x33;  // Dirección I2C por defecto
constexpr int I2C_FREQ_HZ = 400000; // 400 kHz Fast Mode

// =========================================================================
//  ENTORNO FÍSICO (editar según instalación)
// =========================================================================
constexpr float DOOR_HEIGHT_M = 3.6f;    // Altura del sensor sobre el suelo [m]
constexpr float DOOR_WIDTH_M = 4.0f;     // Anchura de la puerta [m]
constexpr float FLOOR_TEMP_MIN_C = 7.0f; // Temp. mínima esperada del suelo [°C]
constexpr float FLOOR_TEMP_MAX_C =
    26.0f; // Temp. máxima esperada del suelo [°C]

// =========================================================================
//  SENSOR MLX90640
// =========================================================================
constexpr int MLX_COLS = 32;
constexpr int MLX_ROWS = 24;
constexpr int TOTAL_PIXELS = MLX_COLS * MLX_ROWS; // 768
constexpr float SENSOR_FOV_DEG = 110.0f;          // Campo de visión [grados]
constexpr float EMISSIVITY = 0.95f;               // Emisividad para piel humana

// =========================================================================
//  PASO 1 — FONDO DINÁMICO (EMA Selectiva)
// =========================================================================
extern float EMA_ALPHA; // Velocidad de adaptación del fondo
                        // Menor = fondo más estable
                        // Mayor = se adapta más rápido

// =========================================================================
//  PASO 2 — DETECCIÓN DE PICOS (Topología)
// =========================================================================
extern float TEMP_BIOLOGICO_MIN;       // Umbral mínimo de detección [°C]
extern float DELTA_T_FONDO;            // Contraste mínimo vs fondo [°C]
constexpr float NOISE_MARGIN_C = 0.5f; // Margen de ruido del sensor [°C]

// =========================================================================
//  PASO 3 — NMS (Supresión de No-Máximos) Adaptativa
// =========================================================================
extern int NMS_RADIUS_CENTER_SQ;     // Radio² en zona central (=radio 4)
extern int NMS_RADIUS_EDGE_SQ;       // Radio² en bordes (=radio 2)
constexpr int NMS_CENTER_X_MIN = 8;  // Límite izq. zona central del lente
constexpr int NMS_CENTER_X_MAX = 23; // Límite der. zona central del lente

// =========================================================================
//  PASO 4 — TRACKING (Filtro Alpha-Beta) + CONTEO
// =========================================================================
constexpr float ALPHA_TRK = 0.85f;    // Peso de la posición medida
constexpr float BETA_TRK = 0.05f;     // Peso de la velocidad estimada
constexpr int MAX_MATCH_DIST_SQ = 25; // Distancia² máx. para emparejar (=5px)
constexpr int TRACK_MAX_AGE = 5;      // Frames sin actualización → eliminar

// --- Zonas de Conteo (Histéresis Y) ---
// Valores iniciales como líneas rectas horizontales.
// TODO: Expandir a array por columna tras calibración visual para
//       manejar la geometría no-lineal del FOV 110° sobre puerta ancha.
extern int DEFAULT_LINE_ENTRY_Y; // Línea virtual superior (entrada)
extern int DEFAULT_LINE_EXIT_Y;  // Línea virtual inferior (salida)

// --- Comandos UI ---
extern int VIEW_MODE;         // 0 = Normal, 1 = Sustractor de Fondo
extern bool APP_RESET_COUNTS; // Flag para resetear contadores desde Web
extern bool APP_RETRY_SENSOR; // Flag para reintentar inicializar sensor

// =========================================================================
//  PASO 5 — MÁSCARA DE RETROALIMENTACIÓN
// =========================================================================
constexpr int MASK_HALF_SIZE = 1; // Radio del cuadrado (1 = 3×3 px)

// =========================================================================
//  SISTEMA Y CAPACIDAD
// =========================================================================
constexpr int PIPELINE_FREQ_HZ = 16; // Frecuencia del pipeline [Hz]
constexpr int MAX_OBJETIVOS = 15;    // Máx. picos crudos por frame
constexpr int MAX_TRACKS = 15;       // Máx. personas rastreadas simultáneas
constexpr int IPC_QUEUE_DEPTH =
    15; // Profundidad de la queue FreeRTOS (aumentada para estabilidad)

// =========================================================================
//  RED (SoftAP + UDP)
// =========================================================================
constexpr const char *SOFTAP_SSID = "ThermalCounter";
constexpr const char *SOFTAP_PASS = "counter1234";
constexpr int SOFTAP_CHANNEL = 1;
constexpr int SOFTAP_MAX_CONN = 2;
constexpr int UDP_PORT = 4210;
constexpr const char *UDP_BROADCAST_IP = "192.168.4.255";

// =========================================================================
//  PROTOCOLO UDP — Tipos de Paquete
// =========================================================================
constexpr uint8_t UDP_PACKET_TELEMETRY = 0x01;
constexpr uint8_t UDP_PACKET_IMAGE = 0x02;

} // namespace ThermalConfig
