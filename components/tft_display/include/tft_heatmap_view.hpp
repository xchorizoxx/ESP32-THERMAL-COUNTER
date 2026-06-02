#pragma once
/**
 * @file tft_heatmap_view.hpp
 * @brief Heatmap view: renders live thermal data via bilinear interpolation.
 */

#include "tft_view.hpp"
#include "tft_renderer.hpp"

class TftHeatmapView : public ITftView {
public:
    void onEnter() override;
    void onExit() override;
    void render(const TftSnapshot& snap,
                uint16_t* fb, int width, int height) override;
    const char* name() const override { return "Heatmap"; }

private:
    TftRenderer renderer_;
    bool initialized_ = false;
    static const char* TAG;
};
