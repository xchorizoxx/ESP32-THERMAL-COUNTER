#include "tft_system_snapshot.hpp"
#include <cstring>

static const char* TAG = "SYS_SNAP";

SystemInfoSnapshot g_sys_snapshot = {};
portMUX_TYPE sys_snapshot_mux = portMUX_INITIALIZER_UNLOCKED;

void writeSystemSnapshot(const SystemInfoSnapshot& info) {
    portENTER_CRITICAL(&sys_snapshot_mux);
    g_sys_snapshot = info;
    portEXIT_CRITICAL(&sys_snapshot_mux);
}

SystemInfoSnapshot readSystemSnapshot() {
    SystemInfoSnapshot ret;
    portENTER_CRITICAL(&sys_snapshot_mux);
    ret = g_sys_snapshot;
    portEXIT_CRITICAL(&sys_snapshot_mux);
    return ret;
}
