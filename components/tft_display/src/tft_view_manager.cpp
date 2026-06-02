#include "tft_view_manager.hpp"
#include "esp_log.h"

static const char* TAG = "TFT_VIEW";

TftViewManager::TftViewManager()
    : m_views(nullptr)
    , m_viewCount(0)
    , m_currentIndex(0)
    , m_asleep(false) {}

TftViewManager::~TftViewManager() {}

bool TftViewManager::init() {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
    return false;
}

void TftViewManager::registerView(ITftView* view) {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
    (void)view;
}

void TftViewManager::nextView() {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
}

void TftViewManager::toggleSleep() {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
}

const char* TftViewManager::currentView() {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
    return "skeleton";
}

void TftViewManager::render(const TftSnapshot& snap,
                            uint16_t* fb, int width, int height) {
    ESP_LOGW(TAG, "%s: not implemented (P0 skeleton)", __func__);
    (void)snap;
    (void)fb;
    (void)width;
    (void)height;
}
