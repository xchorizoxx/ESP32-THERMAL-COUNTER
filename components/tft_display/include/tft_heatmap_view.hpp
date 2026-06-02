#pragma once

#include "tft_view.hpp"
#include <stdint.h>

class TftHeatmapView : public ITftView {
public:
    TftHeatmapView();
    ~TftHeatmapView() override;

    void onEnter() override;
    void onExit() override;
    void render(const TftSnapshot& snap,
                uint16_t* fb, int width, int height) override;
    const char* name() const override;

private:
    bool m_initialized;
};
