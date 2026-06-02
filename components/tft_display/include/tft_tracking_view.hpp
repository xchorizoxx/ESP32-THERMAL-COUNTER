#pragma once

#include "tft_view.hpp"
#include "tft_renderer.hpp"
#include "thermal_config.hpp"

class TftTrackingView : public ITftView {
public:
    void onEnter() override;
    void onExit() override;
    void render(const TftSnapshot& snap, uint16_t* fb, int w, int h) override;
    const char* name() const override { return "Tracking"; }
private:
    TftRenderer renderer_;
    bool initialized_ = false;

    void drawHeatmap(const int16_t* pixels, uint16_t* fb, int w, int h);
    void drawTracks(const TrackInfo* tracks, int num_tracks, uint16_t* fb, int w, int h);
    void drawLines(const ThermalConfig::DoorLineConfig& lines, uint16_t* fb, int w, int h);
    void clampSensorCoords(float& x, float& y);
    void putPixel(int px, int py, uint16_t color, uint16_t* fb, int w, int h);
    void fillCircle(int cx, int cy, int r, uint16_t color, uint16_t* fb, int w, int h);
    void drawLineSeg(int x0, int y0, int x1, int y1, uint16_t color, uint16_t* fb, int w, int h);

    static const char* TAG;
};
