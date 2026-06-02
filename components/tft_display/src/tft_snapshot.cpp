#include "tft_snapshot.hpp"
#include "esp_log.h"

static const char* TAG = "TFT_SNAP";

namespace TftBridge {

TftSnapshot g_snapshot{};
portMUX_TYPE snapshot_mux = portMUX_INITIALIZER_UNLOCKED;

void writeSnapshot(const ImagePayload& image,
                   const TelemetryPayload& telemetry,
                   bool sensor_ok,
                   const ThermalConfig::DoorLineConfig& door_lines) {
    portENTER_CRITICAL(&snapshot_mux);
    memcpy(&g_snapshot.pixels, &image.pixels, sizeof(image.pixels));
    g_snapshot.count_in    = telemetry.count_in;
    g_snapshot.count_out   = telemetry.count_out;
    g_snapshot.num_tracks  = telemetry.num_tracks;
    memcpy(&g_snapshot.tracks, &telemetry.tracks, sizeof(telemetry.tracks));
    g_snapshot.ambient_temp = telemetry.ambient_temp;
    g_snapshot.sensor_temp  = telemetry.sensor_temp;
    g_snapshot.num_events   = telemetry.num_events;
    memcpy(g_snapshot.events, telemetry.events, sizeof(telemetry.events));
    g_snapshot.sensor_ok   = sensor_ok;
    g_snapshot.frame_id    = telemetry.frame_id;
    g_snapshot.door_lines  = door_lines;
    portEXIT_CRITICAL(&snapshot_mux);
}

TftSnapshot readSnapshot() {
    TftSnapshot local;
    portENTER_CRITICAL(&snapshot_mux);
    local = g_snapshot;
    portEXIT_CRITICAL(&snapshot_mux);
    return local;
}

} // namespace TftBridge
