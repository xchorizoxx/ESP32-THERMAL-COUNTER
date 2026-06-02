/**
 * @file tft_view_manager.cpp
 * @brief View cycling + sleep toggle for TFT display.
 */

#include "tft_view_manager.hpp"
#include "esp_log.h"

const char* TftViewManager::TAG = "TFT_VIEWMGR";

bool TftViewManager::registerView(ITftView* view) {
    if (num_views_ >= MAX_VIEWS || !view) {
        ESP_LOGW(TAG, "Cannot register view (max=%d, count=%d)", MAX_VIEWS, num_views_);
        return false;
    }
    views_[num_views_++] = view;
    ESP_LOGI(TAG, "Registered view [%d]: %s", num_views_ - 1, view->name());

    // Auto-enter first registered view
    if (num_views_ == 1) {
        view->onEnter();
    }
    return true;
}

void TftViewManager::nextView() {
    if (num_views_ <= 1) return;
    if (sleeping_) return;

    ITftView* old_view = views_[current_index_];
    if (old_view) old_view->onExit();

    current_index_ = (current_index_ + 1) % num_views_;

    ITftView* new_view = views_[current_index_];
    if (new_view) new_view->onEnter();

    ESP_LOGI(TAG, "Switched to view [%d]: %s", current_index_,
             new_view ? new_view->name() : "null");
}

bool TftViewManager::toggleSleep() {
    sleeping_ = !sleeping_;
    ESP_LOGI(TAG, "Display %s", sleeping_ ? "SLEEP" : "WAKE");
    return sleeping_;
}

ITftView* TftViewManager::currentView() {
    if (num_views_ == 0) return nullptr;
    return views_[current_index_];
}
