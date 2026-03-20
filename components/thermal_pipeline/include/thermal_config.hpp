#pragma once
/**
 * @file thermal_config.hpp
 * @brief Neural center of configurable system parameters.
 *
 * ALL values depending on the environment, sensor, or installation
 * are centralized here. To adapt the system to a new door,
 * only this file needs to be edited.
 */

#include <cstdint>

namespace ThermalConfig {

// =========================================================================
//  I2C HARDWARE
// =========================================================================
constexpr int I2C_SDA_PIN = 8;      // GPIO for SDA of MLX90640
constexpr int I2C_SCL_PIN = 9;      // GPIO for SCL of MLX90640
constexpr uint8_t MLX_ADDR = 0x33;  // Default I2C address
constexpr int I2C_FREQ_HZ = 400000; // 400 kHz Fast Mode

// =========================================================================
//  PHYSICAL ENVIRONMENT (edit according to installation)
// =========================================================================
constexpr float DOOR_HEIGHT_M = 3.6f;    // Sensor height above floor [m]
constexpr float DOOR_WIDTH_M = 4.0f;     // Door width [m]
constexpr float FLOOR_TEMP_MIN_C = 7.0f; // Expected minimum floor temp [°C]
constexpr float FLOOR_TEMP_MAX_C = 26.0f; // Expected maximum floor temp [°C]

// =========================================================================
//  MLX90640 SENSOR
// =========================================================================
constexpr int MLX_COLS = 32;
constexpr int MLX_ROWS = 24;
constexpr int TOTAL_PIXELS = MLX_COLS * MLX_ROWS; // 768
constexpr float SENSOR_FOV_DEG = 110.0f;          // Field of View [degrees]
constexpr float EMISSIVITY = 0.95f;               // Emissivity for human skin

// =========================================================================
//  STEP 1 — DYNAMIC BACKGROUND (Selective EMA)
// =========================================================================
extern float EMA_ALPHA; // Background adaptation speed
                        // Lower = more stable background
                        // Higher = adapts faster

// =========================================================================
//  STEP 2 — PEAK DETECTION (Topology)
// =========================================================================
extern float TEMP_BIOLOGICO_MIN;       // Minimum detection threshold [°C]
extern float DELTA_T_FONDO;            // Minimum contrast vs background [°C]
constexpr float NOISE_MARGIN_C = 0.5f; // Sensor noise margin [°C]

// =========================================================================
//  STEP 3 — NMS (Non-Maximum Suppression) Adaptive
// =========================================================================
extern int NMS_RADIUS_CENTER_SQ;     // Squared radius in central zone (=radius 4)
extern int NMS_RADIUS_EDGE_SQ;       // Squared radius at edges (=radius 2)
constexpr int NMS_CENTER_X_MIN = 8;  // Left limit of lens central zone
constexpr int NMS_CENTER_X_MAX = 23; // Right limit of lens central zone

// =========================================================================
//  STEP 4 — TRACKING (Alpha-Beta Filter) + COUNTING
// =========================================================================
constexpr float ALPHA_TRK = 0.85f;    // Weight of measured position
constexpr float BETA_TRK = 0.05f;     // Weight of estimated velocity
constexpr int MAX_MATCH_DIST_SQ = 25; // Max. squared distance to match (=5px)
constexpr int TRACK_MAX_AGE = 5;      // Frames without update → remove

// --- Counting Zones (Y-Hysteresis) ---
// Initial values as straight horizontal lines.
// TODO: Expand to per-column array after visual calibration to
//       handle non-linear FOV 110° geometry on wide doors.
extern int DEFAULT_LINE_ENTRY_Y; // Virtual upper line (entrance)
extern int DEFAULT_LINE_EXIT_Y;  // Virtual lower line (exit)

// --- UI Commands ---
extern int VIEW_MODE;         // 0 = Normal, 1 = Background Subtraction
extern bool APP_RESET_COUNTS; // Flag to reset counters from Web
extern bool APP_RETRY_SENSOR; // Flag to retry sensor initialization

// =========================================================================
//  STEP 5 — FEEDBACK MASK
// =========================================================================
constexpr int MASK_HALF_SIZE = 1; // Square radius (1 = 3×3 px)

// =========================================================================
//  SYSTEM AND CAPACITY
// =========================================================================
constexpr int PIPELINE_FREQ_HZ = 16; // Pipeline frequency [Hz]
constexpr int MAX_OBJETIVOS = 15;    // Max. raw peaks per frame
constexpr int MAX_TRACKS = 15;       // Max. simultaneous tracked persons
constexpr int IPC_QUEUE_DEPTH = 15; // FreeRTOS queue depth (increased for stability)

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
