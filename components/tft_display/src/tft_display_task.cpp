#include "tft_display_task.hpp"
#include "esp_log.h"

static const char* TAG = "TFT_TASK";

TftDisplayTask::TftDisplayTask()
    : m_initialized(false)
    , m_taskStack(nullptr) {}

TftDisplayTask::~TftDisplayTask() {}

bool TftDisplayTask::init() {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
    return false;
}

void TftDisplayTask::TaskWrapper(void* pvParameters) {
    auto* self = static_cast<TftDisplayTask*>(pvParameters);
    self->run();
    vTaskDelete(NULL);
}

void TftDisplayTask::run() {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
