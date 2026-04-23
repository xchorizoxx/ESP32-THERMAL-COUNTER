#pragma once
/**
 * @file thermal_types.hpp
 * @brief Data structures for the thermal counting system.
 *
 * Defines types for the vision pipeline (ThermalPeak, Track)
 * and communication payloads (TelemetryPayload, ImagePayload, IpcPacket).
 */

#include <cstring>
#include <stdint.h>
#include <stdio.h>
#include "esp_log.h"

// =========================================================================
//  [NEW] Full-Line Colorized Logging Macros
// =========================================================================
#define LOG_COLOR_CYAN    "\033[0;36m"
#define LOG_COLOR_MAGENTA "\033[0;35m"
#define LOG_COLOR_WHITE   "\033[0;37m"
#define LOG_COLOR_RESET   "\033[0m"

/**
 * @brief Logs an entire line with a specific ANSI color.
 * Mimics ESP_LOGI format: I (timestamp) TAG: Message
 */
#define ESP_LOG_COLOR(color, tag, format, ...) \
    printf(color "I (%lu) %s: " format LOG_COLOR_RESET "\n", (unsigned long)esp_log_timestamp(), tag, ##__VA_ARGS__)

/**
 * @brief Segmento de linea de conteo en coordenadas del sensor (0..31 x 0..23).
 *
 * Define un segmento que, cuando un track lo cruza, dispara un conteo.
 * La direccion del cruce determina si es IN o OUT.
 */
struct __attribute__((packed)) CountingSegment {
  float x1;      // Punto inicio X [0..31]
  float y1;      // Punto inicio Y [0..23]
  float x2;      // Punto fin X [0..31]
  float y2;      // Punto fin Y [0..23]
  uint8_t id;    // ID de la linea (para multiples lineas)
  char name[16]; // Nombre descriptivo
  bool enabled;  // Activa/inactiva
};

constexpr int MAX_COUNTING_LINES = 4; // Maximo de lineas por puerta

namespace ThermalConfig {
constexpr int MAX_TRACKS = 20;    // Max. simultaneous tracked persons
constexpr int TOTAL_PIXELS = 768; // 32x24
} // namespace ThermalConfig

#include "thermal_config.hpp"

namespace ThermalConfig {
struct DoorLineConfig {
  CountingSegment lines[MAX_COUNTING_LINES];
  uint8_t num_lines;
  bool use_segments; // false = usar Y horizontal legacy
};
} // namespace ThermalConfig

// =========================================================================
//  Vision Pipeline Structures (Core 1)
// =========================================================================

/**
 * @brief Thermal peak detected in a frame.
 * Produced by PeakDetector (Step 2), consumed by NmsSuppressor (Step 3).
 */
struct ThermalPeak {
  float x;           ///< Column [0.0..31.0] — sub-pixel centroid
  float y;           ///< Row [0.0..23.0]    — sub-pixel centroid
  float temperature; ///< Temperature in °C (peak temperature)
  bool suppressed;   ///< NMS flag: true = suppressed by a hotter peak
};

/**
 * @brief Dense track snapshot for mask generation + telemetry (from
 * TrackletTracker::fillTrackArray).
 */
struct Track {
  uint8_t id;      ///< Unique track identifier
  float x;         ///< Smoothed X position (sub-pixel)
  float y;         ///< Smoothed Y position (sub-pixel)
  float v_x;       ///< Estimated X velocity [px/frame]
  float v_y;       ///< Estimated Y velocity [px/frame]
  uint8_t age;     ///< Frames since last real update
  bool active;     ///< false = expired track (age > TRACK_MAX_MISSED)
  uint8_t state_y; ///< HUD tint / zone from TrackletFSM (0=unborn, 1=in,
                   ///< 2=neutral, 3=out)
  float peak_temp; ///< Peak temperature of this track in °C (W4)
};

// =========================================================================
//  Communication Payloads (Core 1 → Core 0 → UDP)
// =========================================================================

/**
 * @brief Compact track info for transmission.
 * Uses fixed-point ×100 to avoid floats on the network.
 */
struct __attribute__((packed)) TrackInfo {
  uint8_t id;
  int16_t x_100;         ///< X position × 100 (e.g., 1520 = 15.20)
  int16_t y_100;         ///< Y position × 100 (e.g., 1200 = 12.00)
  int16_t v_x_100;       ///< X velocity × 100
  int16_t v_y_100;       ///< Y velocity × 100
  int16_t peak_temp_100; ///< Peak temperature × 100 (W4)
};

/**
 * @brief Represents a single person crossing the line.
 * Captured by Core 1 (Vision) and processed by Core 0 (Web/Telemetry).
 */
struct CrossingEvent {
  uint8_t id;             ///< Track ID
  bool is_in;             ///< true=IN, false=OUT
  int16_t count_in;       ///< Snapshot of total IN count (session)
  int16_t count_out;      ///< Snapshot of total OUT count (session)
  float temperature;      ///< EMA temperature at moment of crossing
  uint32_t timestamp_ms;  ///< Local system time
};

/**
 * @brief Telemetry packet: counters + active tracks + new events.
 * Sent via IPC queue between cores.
 */
struct __attribute__((packed)) TelemetryPayload {
  uint32_t frame_id;
  float ambient_temp; ///< Ta read from MLX90640 sensor
  int16_t count_in;
  int16_t count_out;
  uint8_t num_tracks;
  TrackInfo tracks[ThermalConfig::MAX_TRACKS];

  // W4-CSV: Async events detected in this frame
  static constexpr int MAX_EVENTS_PER_FRAME = 5;
  uint8_t num_events;
  CrossingEvent events[MAX_EVENTS_PER_FRAME];
};

/**
 * @brief Thermal image packet: 768 temperature values.
 * Sent via UDP with header 0x02.
 * Each pixel = temp × 100 as int16 (e.g., 2350 = 23.50°C).
 * Size: ~1540 bytes.
 */
struct __attribute__((packed)) ImagePayload {
  uint32_t frame_id;
  int16_t pixels[ThermalConfig::TOTAL_PIXELS];
};

/**
 * @brief Full IPC packet traveling through the FreeRTOS Queue.
 * Contains both telemetry and image so Core 0
 * can send both UDP packets per frame.
 */
struct IpcPacket {
  TelemetryPayload telemetry;
  ImagePayload image;
  bool sensor_ok;
};

// =========================================================================
//  UI Commands (Core 0 -> Core 1)
// =========================================================================
enum class ConfigCmdType {
  SET_TEMP_BIO,
  SET_DELTA_T,
  SET_EMA_ALPHA,
  SET_LINE_ENTRY,
  SET_LINE_EXIT,
  SET_DEAD_LEFT,
  SET_DEAD_RIGHT,
  SET_SENSOR_HEIGHT,
  SET_PERSON_DIAMETER,
  SET_VIEW_MODE,
  RESET_COUNTS,
  RETRY_SENSOR,
  SAVE_CONFIG, ///< Persist current config to NVS flash
  APPLY_CONFIG ///< Batch-apply all parameters (handled in Core 0)
};

struct AppConfigCmd {
  ConfigCmdType type;
  float value;
};
