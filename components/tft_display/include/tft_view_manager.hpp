#pragma once
/**
 * @file tft_view_manager.hpp
 * @brief Manages registered views and display sleep state.
 *
 * Views are registered at init time via registerView().
 * nextView() cycles through them. toggleSleep() enters/exits sleep.
 */

#include "tft_view.hpp"
#include <stdint.h>

class TftViewManager {
public:
    static constexpr int MAX_VIEWS = 4;

    /**
     * @brief Register a view. Call at init time only.
     * @return true if registered, false if MAX_VIEWS reached
     */
    bool registerView(ITftView* view);

    /**
     * @brief Advance to the next registered view.
     * Calls onExit() on current, onEnter() on next.
     */
    void nextView();

    /**
     * @brief Toggle display sleep state.
     * @return true if now sleeping, false if now awake
     */
    bool toggleSleep();

    /// @return true if display is in sleep mode
    bool isSleeping() const { return sleeping_; }

    /**
     * @brief Get the current active view for rendering.
     * @return Pointer to current view, or nullptr if no views registered
     */
    ITftView* currentView();

    /// Number of registered views
    int viewCount() const { return num_views_; }

private:
    ITftView* views_[MAX_VIEWS] = {};
    int num_views_ = 0;
    int current_index_ = 0;
    bool sleeping_ = false;

    static const char* TAG;
};
