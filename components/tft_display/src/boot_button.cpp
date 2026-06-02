#include "boot_button.hpp"
#include "esp_log.h"

static const char* TAG = "BTN_BOOT";

BootButton::BootButton()
    : m_initialized(false)
    , m_lastState(1)
    , m_pressStartMs(0) {}

BootButton::~BootButton() {}

bool BootButton::init() {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
    return false;
}

bool BootButton::poll() {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
    return false;
}
