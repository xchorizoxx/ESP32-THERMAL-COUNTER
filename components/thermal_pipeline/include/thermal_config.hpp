#pragma once
/**
 * @file thermal_config.hpp
 * @brief Neural center of configurable system parameters.
 *
 * ALL values depending on the environment, sensor, or installation
 * are centralized here. To adapt the system to a new door,
 * only this file needs to be edited.
 */

#include <stdint.h>
#include "freertos/FreeRTOS.h"   // portMUX_TYPE (P02-fix)

namespace ThermalConfig {

// =========================================================================
//  I2C HARDWARE
// =========================================================================
constexpr int I2C_SDA_PIN = 8;     // GPIO for SDA of MLX90640
constexpr int I2C_SCL_PIN = 9;     // GPIO for SCL of MLX90640
constexpr uint8_t MLX_ADDR = 0x33; // Default I2C address
// Fast-Mode Plus (FM+): 1 MHz.
// Compatible con MLX90640 (datasheet §7.4) y ESP32-S3 I2C hardware.
// REQUISITO HARDWARE: pull-ups externos de 1kΩ en SDA y SCL.
// Con cables cortos (<10cm) puede operar a 1MHz sin problemas.
// Si hay errores I2C frecuentes, bajar a 800000 (800 kHz).
constexpr int I2C_FREQ_HZ = 1000000; // 1 MHz Fast-Mode Plus

// =========================================================================
//  MLX90640 SENSOR
// =========================================================================
constexpr int MLX_COLS = 32;
constexpr int MLX_ROWS = 24;
// TOTAL_PIXELS is now in thermal_types.hpp
// --- FUTURE / DOCUMENTATION-ONLY CONSTANTS ---
// These are defined for reference but not currently used in code:
// constexpr float SENSOR_FOV_DEG = 110.0f;  // Field of View [degrees] - for
// future auto-calibration

// Currently used emissivity (must match deprecated/alpha_beta_tracker if
// referenced there)
constexpr float EMISSIVITY = 0.95f; // Emissivity for human skin

// =========================================================================
//  STEP 1 — DYNAMIC BACKGROUND (Selective EMA)
// =========================================================================
extern float EMA_ALPHA; // Background adaptation speed
                        // Lower = more stable background
                        // Higher = adapts faster

// =========================================================================
//  STEP 2 — PEAK DETECTION (Topology)
// =========================================================================
extern float BIOLOGICAL_TEMP_MIN;      // Minimum detection threshold [°C]
extern float BACKGROUND_DELTA_T;       // Minimum contrast vs background [°C]
constexpr float NOISE_MARGIN_C = 0.5f; // Sensor noise margin [°C]

// =========================================================================
//  STEP 3 — NMS (Non-Maximum Suppression) Adaptive
// =========================================================================
extern float SENSOR_HEIGHT_M;   // Altura del sensor sobre el suelo [m]
extern float PERSON_DIAMETER_M; // Diametro fisico de la persona [m]

// =========================================================================
//  STEP 4 — TRACKING (TrackletTracker) + COUNTING
// =========================================================================

// --- TRACKLET TRACKER (A2) ---
constexpr int TRACK_CONFIRM_FRAMES =
    3; ///< Min. consecutive detections for a valid track
constexpr int TRACK_MAX_MISSED = 12; ///< Frames without detection before expiry
constexpr float TRACK_MAX_DIST =
    8.0f; ///< Max match distance [px] — 8px handles fast hand movement on 32x24
constexpr float TRACK_TEMP_WEIGHT =
    0.25f; ///< Temperature weight in composite match cost
constexpr float TRACK_DISPLAY_SMOOTH =
    0.65f; ///< EMA alpha for HUD display position. 0.65: reactive but stable.
           ///< (0=frozen, 1=raw)

// --- Counting Zones (Y-Hysteresis) ---
// Initial values as straight horizontal lines.
// TODO: Expand to per-column array after visual calibration to
//       handle non-linear FOV 110° geometry on wide doors.

struct DoorLineConfig;

// P02-fix: Mutex dedicado para door_lines.
// TrackletFSM (Core 1) lee door_lines concurrentemente mientras HTTP Server (Core 0)
// la reescribe. Este spinlock protege todas las lecturas y escrituras de door_lines.
// Uso: portENTER_CRITICAL(&ThermalConfig::door_lines_mux) / portEXIT_CRITICAL(...)
extern portMUX_TYPE door_lines_mux;

extern DoorLineConfig door_lines; // Config global de lineas

extern int DEFAULT_LINE_ENTRY_Y;    // Virtual upper line (entrance)
extern int DEFAULT_LINE_EXIT_Y;     // Virtual lower line (exit)
extern int DEFAULT_DEAD_ZONE_LEFT;  // Exclusion lateral (left limit)
extern int DEFAULT_DEAD_ZONE_RIGHT; // Exclusion lateral (right limit)

// --- UI / HUD ---
extern int VIEW_MODE; // 0 = Normal, 1 = Background Subtraction

// =========================================================================
//  STEP 5 — FEEDBACK MASK
// =========================================================================
constexpr int MASK_HALF_SIZE = 1; // Square radius (1 = 3×3 px)

// =========================================================================
//  SYSTEM AND CAPACITY
// =========================================================================
constexpr int PIPELINE_FREQ_HZ = 32; // Pipeline frequency [Hz]
constexpr int MAX_PEAKS = 20;        // Max. raw peaks per frame
// MAX_TRACKS is now in thermal_types.hpp
constexpr int IPC_QUEUE_DEPTH =
    4; // Bug3-fix: 15→4 saves ~20 KB SRAM (pipeline drops on full, timeout=0)

// =========================================================================
//  NETWORK (SoftAP + UDP)
// =========================================================================
constexpr const char *SOFTAP_SSID = "ThermalCounter";
constexpr const char *SOFTAP_PASS = "counter1234";
constexpr int SOFTAP_CHANNEL = 1;
constexpr int SOFTAP_MAX_CONN = 2;
constexpr int UDP_PORT = 4210;
constexpr const char *UDP_BROADCAST_IP = "192.168.4.255";

// =========================================================================
//  UDP PROTOCOL — Packet Types
// =========================================================================
constexpr uint8_t UDP_PACKET_TELEMETRY = 0x01;
constexpr uint8_t UDP_PACKET_IMAGE = 0x02;

} // namespace ThermalConfig
