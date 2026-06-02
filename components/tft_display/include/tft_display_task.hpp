#pragma once

#include "tft_driver.hpp"
#include "tft_heatmap_view.hpp"
#include "tft_tracking_view.hpp"
#include "tft_stats_view.hpp"
#include "tft_info_view.hpp"
#include "boot_button.hpp"
#include "tft_view_manager.hpp"
#include "thermal_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

class TftDisplayTask {
public:
    esp_err_t init(TftDriver* driver);
    static void TaskWrapper(void* pvParameters);
private:
    void run();

    TftDriver*        driver_ = nullptr;
    TftHeatmapView    heatmap_view_;
    TftTrackingView   tracking_view_;
    TftStatsView      stats_view_;
    TftInfoView       info_view_;
    BootButton        button_;
    TftViewManager    view_manager_;

    static const char* TAG;
};
