#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

struct SystemInfoSnapshot {
    bool     rtc_ok;
    char     time_str[16];      // "HH:MM:SS"
    char     date_str[16];      // "DD/MM/YY"
    bool     sd_ok;
    uint32_t sd_free_kb;
    uint32_t sd_total_kb;
    int      wifi_clients;
    uint32_t uptime_sec;
    float    ambient_temp;
    float    sensor_temp;
    bool     sensor_ok;
    uint32_t update_id;
};

extern SystemInfoSnapshot g_sys_snapshot;
extern portMUX_TYPE sys_snapshot_mux;

void writeSystemSnapshot(const SystemInfoSnapshot& info);
SystemInfoSnapshot readSystemSnapshot();
