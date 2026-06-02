#pragma once

#include "freertos/FreeRTOS.h"
#include "thermal_types.hpp"

namespace TftBridge {

extern TftSnapshot g_snapshot;

extern portMUX_TYPE snapshot_mux;

void writeSnapshot(const ImagePayload& image,
                   const TelemetryPayload& telemetry,
                   bool sensor_ok);

TftSnapshot readSnapshot();

} // namespace TftBridge
