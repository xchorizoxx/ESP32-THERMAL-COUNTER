#pragma once
/**
 * @file thermal_types.hpp
 * @brief Estructuras de datos del sistema de conteo térmico.
 *
 * Define los tipos para el pipeline de visión (PicoTermico, Track)
 * y los payloads de comunicación (PayloadTelemetria, PayloadImagen, IpcPacket).
 */

#include <stdint.h>
#include <cstring>
#include "thermal_config.hpp"

// =========================================================================
//  Estructuras del Pipeline de Visión (Core 1)
// =========================================================================

/**
 * @brief Pico térmico detectado en un frame.
 * Producido por PeakDetector (Paso 2), consumido por NmsSuppressor (Paso 3).
 */
struct PicoTermico {
    uint8_t x;              ///< Columna [0..31]
    uint8_t y;              ///< Fila [0..23]
    float   temperatura;    ///< Temperatura en °C
    bool    suprimido;      ///< Bandera NMS: true = suprimido por un pico más caliente
};

/**
 * @brief Track de una persona rastreada entre frames.
 * Gestionado por AlphaBetaTracker (Paso 4).
 */
struct Track {
    uint8_t id;             ///< Identificador único del track
    float   x;              ///< Posición X suavizada (sub-pixel)
    float   y;              ///< Posición Y suavizada (sub-pixel)
    float   v_x;            ///< Velocidad estimada en X [px/frame]
    float   v_y;            ///< Velocidad estimada en Y [px/frame]
    uint8_t age;            ///< Frames desde la última actualización real
    bool    activo;         ///< false = track expirado (age > TRACK_MAX_AGE)
    uint8_t estado_y;       ///< Máquina de estados: 0=Superior, 1=Neutro, 2=Inferior
};

// =========================================================================
//  Payloads de Comunicación (Core 1 → Core 0 → UDP)
// =========================================================================

/**
 * @brief Info compacta de un track para transmisión.
 * Usa fixed-point ×100 para evitar floats en la red.
 */
struct __attribute__((packed)) TrackInfo {
    uint8_t id;
    int16_t x_100;          ///< Posición X × 100 (ej. 1520 = 15.20)
    int16_t y_100;          ///< Posición Y × 100 (ej. 1200 = 12.00)
    int16_t v_x_100;        ///< Velocidad X × 100
    int16_t v_y_100;        ///< Velocidad Y × 100
};

/**
 * @brief Paquete de telemetría: contadores + tracks activos.
 * Se envía por UDP con header 0x01.
 * Tamaño: ~85 bytes (varía con num_tracks).
 */
struct __attribute__((packed)) PayloadTelemetria {
    uint32_t frame_id;
    float    ambient_temp;  ///< Ta leída del sensor MLX90640
    int16_t  count_in;
    int16_t  count_out;
    uint8_t  num_tracks;
    TrackInfo tracks[ThermalConfig::MAX_TRACKS];
};

/**
 * @brief Paquete de imagen térmica: 768 valores de temperatura.
 * Se envía por UDP con header 0x02.
 * Cada pixel = temp × 100 como int16 (ej. 2350 = 23.50°C).
 * Tamaño: ~1540 bytes.
 */
struct __attribute__((packed)) PayloadImagen {
    uint32_t frame_id;
    int16_t  pixels[ThermalConfig::TOTAL_PIXELS];
};

/**
 * @brief Paquete IPC completo que viaja por la FreeRTOS Queue.
 * Contiene tanto la telemetría como la imagen para que Core 0
 * pueda enviar ambos paquetes UDP por cada frame.
 */
struct IpcPacket {
    PayloadTelemetria telemetria;
    PayloadImagen     imagen;
    bool              sensor_ok;
};

// =========================================================================
//  Comandos UI (Core 0 -> Core 1)
// =========================================================================
enum class ConfigCmdType {
    SET_TEMP_BIO,
    SET_DELTA_T,
    SET_EMA_ALPHA,
    SET_LINE_ENTRY,
    SET_LINE_EXIT,
    SET_NMS_CENTER,
    SET_NMS_EDGE,
    SET_VIEW_MODE,
    RESET_COUNTS,
    RETRY_SENSOR,
    SAVE_CONFIG,    ///< Persist current config to NVS flash
    APPLY_CONFIG    ///< Batch-apply all parameters (no-op for pipeline, handled in Core 0)
};

struct AppConfigCmd {
    ConfigCmdType type;
    float value;
};

