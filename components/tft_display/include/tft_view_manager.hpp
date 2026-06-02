#pragma once

#include "tft_view.hpp"
#include <stdint.h>

class TftViewManager {
public:
    TftViewManager();
    ~TftViewManager();

    bool init();
    void registerView(ITftView* view);
    void nextView();
    void toggleSleep();
    const char* currentView();
    void render(const TftSnapshot& snap, uint16_t* fb, int width, int height);

private:
    ITftView** m_views;
    int m_viewCount;
    int m_currentIndex;
    bool m_asleep;
};
