#pragma once

#include "tft_view.hpp"
#include "thermal_config.hpp"
#include <stdint.h>

class TftStatsView : public ITftView {
public:
    void onEnter() override;
    void onExit() override;
    void render(const TftSnapshot& snap, uint16_t* fb, int w, int h) override;
    const char* name() const override { return "Stats"; }

private:
    static const int MAX_SPARK = 24;
    int8_t spark_data_[MAX_SPARK] = {};  // +1 IN, -1 OUT, 0 empty
    int spark_count_ = 0;

    uint16_t prev_count_in_ = 0;
    uint16_t prev_count_out_ = 0;
    uint32_t prev_frame_id_ = 0;

    uint32_t cross_timestamps_[60] = {};
    int cross_ts_idx_ = 0;
    int cross_ts_count_ = 0;

    bool last_cross_was_in_ = false;
    uint32_t last_cross_ms_ = 0;

    void drawSparkline(uint16_t* fb, int w, int h);
    void drawCounters(uint16_t* fb, int w, int h, uint16_t in, uint16_t out);
    void drawMetrics(uint16_t* fb, int w, int h, const TftSnapshot& snap);
    void drawCrossingInfo(uint16_t* fb, int w, int h, uint32_t now_ms);
    void drawBoldNumber(int x, int y, int num, uint16_t color, uint16_t* fb, int w);
    void drawSmallText(int x, int y, const char* text, uint16_t color, uint16_t* fb, int w);
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color, uint16_t* fb, int w);
    void fillRect(int x, int y, int rw, int rh, uint16_t color, uint16_t* fb, int w);
    void putPixel(int x, int y, uint16_t color, uint16_t* fb, int w);

    static const char* TAG;
};
