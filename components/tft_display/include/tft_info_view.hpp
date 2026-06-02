#pragma once

#include "tft_view.hpp"
#include "tft_system_snapshot.hpp"
#include "thermal_config.hpp"
#include <stdint.h>

class TftInfoView : public ITftView {
public:
    void onEnter() override;
    void onExit() override;
    void render(const TftSnapshot& snap, uint16_t* fb, int w, int h) override;
    const char* name() const override { return "Info"; }

private:
    void drawClock(uint16_t* fb, int w, const SystemInfoSnapshot& sys);
    void drawDate(uint16_t* fb, int w, const SystemInfoSnapshot& sys);
    void drawSensorDots(uint16_t* fb, int w, int y, const SystemInfoSnapshot& sys);
    void drawTemps(uint16_t* fb, int w, const SystemInfoSnapshot& sys);
    void drawUptime(uint16_t* fb, int w, int h, const SystemInfoSnapshot& sys);

    void drawBigDigit(int x, int y, char digit, uint16_t color, uint16_t* fb, int w);
    void drawSmallChar(int x, int y, char c, uint16_t color, uint16_t* fb, int w);
    void drawSmallString(int x, int y, const char* str, uint16_t color, uint16_t* fb, int w);
    void drawDot(int cx, int cy, int r, uint16_t color, uint16_t* fb, int w);
    void putPixel(int x, int y, uint16_t color, uint16_t* fb, int w);

    static const char* TAG;
};
