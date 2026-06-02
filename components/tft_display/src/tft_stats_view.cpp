#include "tft_stats_view.hpp"
#include "tft_config.hpp"
#include "esp_log.h"
#include <cstring>
#include <math.h>
#include <ctime>
#include "tft_system_snapshot.hpp"

const char* TftStatsView::TAG = "TFT_STATS";

static constexpr uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return (c >> 8) | (c << 8);
}

static const uint16_t COL_BLUE    = RGB565(15, 75, 200);   // Darker blue IN
static const uint16_t COL_GREEN   = RGB565(16, 185, 129);   // #10b981 OUT
static const uint16_t COL_AMBER   = RGB565(255, 179, 0);    // #ffb300 metrics
static const uint16_t COL_CYAN    = RGB565(0, 212, 255);    // #00d4ff AMB
static const uint16_t COL_WHITE   = RGB565(255, 255, 255);  // white text
static const uint16_t COL_GRAY    = RGB565(107, 114, 128);  // #6b7280
static const uint16_t COL_DARK    = RGB565(20, 20, 30);     // background
static const uint16_t COL_MIDLINE = RGB565(60, 60, 70);     // spark midline

// 5x7 font (ASCII 0x20-0x7E): each entry = 5 column bytes, bit 0=top row
static const uint8_t FONT5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},{0x08,0x2A,0x1C,0x2A,0x08},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},{0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},
    {0x41,0x22,0x14,0x08,0x00},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x01,0x01},
    {0x3E,0x41,0x41,0x51,0x32},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x04,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x7F,0x20,0x18,0x20,0x7F},{0x63,0x14,0x08,0x14,0x63},
    {0x03,0x04,0x78,0x04,0x03},{0x61,0x51,0x49,0x45,0x43},{0x00,0x00,0x7F,0x41,0x41},
    {0x02,0x04,0x08,0x10,0x20},{0x41,0x41,0x7F,0x00,0x00},{0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x08,0x14,0x54,0x54,0x3C},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},
    {0x00,0x7F,0x10,0x28,0x44},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0x7C,0x14,0x14,0x14,0x08},
    {0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00},{0x08,0x08,0x2A,0x1C,0x08}
};

void TftStatsView::putPixel(int x, int y, uint16_t color, uint16_t* fb, int w) {
    if (x < 0 || x >= w || y < 0 || y >= TftConfig::HEIGHT) return;
    fb[y * w + x] = color;
}

void TftStatsView::fillRect(int x, int y, int rw, int rh, uint16_t color, uint16_t* fb, int w) {
    int x1 = x + rw;
    int y1 = y + rh;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > w) x1 = w;
    if (y1 > TftConfig::HEIGHT) y1 = TftConfig::HEIGHT;
    for (int py = y; py < y1; py++) {
        for (int px = x; px < x1; px++) {
            fb[py * w + px] = color;
        }
    }
}

void TftStatsView::drawLine(int x0, int y0, int x1, int y1, uint16_t color, uint16_t* fb, int w) {
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        putPixel(x0, y0, color, fb, w);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void TftStatsView::drawSmallText(int x, int y, const char* text, uint16_t color, uint16_t* fb, int w) {
    if (!text) return;
    int cx = x;
    while (*text) {
        char c = *text;
        if (c < 0x20 || c > 0x7E) { c = ' '; }
        const uint8_t* cols = FONT5x7[c - 0x20];
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                if (cols[col] & (1 << row)) {
                    putPixel(cx + col, y + row, color, fb, w);
                }
            }
        }
        cx += 6;
        text++;
    }
}

void TftStatsView::drawBoldNumber(int x, int y, int num, uint16_t color, uint16_t* fb, int w) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", num);
    int len = (int)strlen(buf);
    int total_w = len * 16; // 15px digit + 1px spacing
    int sx = x - total_w / 2;
    if (sx < 0) sx = 0;
    for (int i = 0; i < len; i++) {
        char c = buf[i];
        if (c < '0' || c > '9') continue;
        const uint8_t* cols = FONT5x7[c - 0x20];
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                if (cols[col] & (1 << row)) {
                    for (int dx = 0; dx < 3; dx++) {
                        for (int dy = 0; dy < 3; dy++) {
                            int px = sx + col * 3 + dx;
                            int py = y + row * 3 + dy;
                            if (px >= 0 && px < w && py >= 0 && py < TftConfig::HEIGHT) {
                                fb[py * w + px] = color;
                            }
                        }
                    }
                }
            }
        }
        sx += 16;
    }
}

void TftStatsView::drawActivityHistogram(uint16_t* fb, int w, int h) {
    const int GR_Y0 = 0;
    const int GR_H = 46;
    const int MID_Y = GR_Y0 + GR_H / 2;
    const int X_OFFSET = 10;

    fillRect(0, GR_Y0, w, GR_H, COL_DARK, fb, w);

    for (int px = 0; px < w; px += 4) {
        putPixel(px, MID_Y, COL_MIDLINE, fb, w);
    }

    int max_val = 3;
    for (int i = 0; i < NUM_BINS; i++) {
        if (bins_[i].count_in > max_val) max_val = bins_[i].count_in;
        if (bins_[i].count_out > max_val) max_val = bins_[i].count_out;
    }

    for (int i = 0; i < NUM_BINS; i++) {
        int idx = (current_bin_idx_ + 1 + i) % NUM_BINS;
        int x = X_OFFSET + i * 5;

        int h_in = (bins_[idx].count_in * 19) / max_val;
        int h_out = (bins_[idx].count_out * 19) / max_val;

        if (h_in > 0) {
            fillRect(x, MID_Y - h_in, 4, h_in, COL_BLUE, fb, w);
        }
        if (h_out > 0) {
            fillRect(x, MID_Y + 1, 4, h_out, COL_GREEN, fb, w);
        }
    }

    char label[20];
    int total_session_crosses = (int)prev_count_in_ + (int)prev_count_out_;
    snprintf(label, sizeof(label), "%d cruces", total_session_crosses);
    drawSmallText(w - 66, 2, label, COL_GRAY, fb, w);
}

void TftStatsView::drawCounters(uint16_t* fb, int w, int h, uint16_t in, uint16_t out) {
    // Draw modern dashboard flat cards for counters (y = 52 to 88)
    const uint16_t COL_CARD_BG = RGB565(30, 30, 42); // Elegant dark gray-blue card bg
    
    fillRect(4, 52, 72, 36, COL_CARD_BG, fb, w);
    fillRect(84, 52, 72, 36, COL_CARD_BG, fb, w);

    // Draw card borders
    for (int x = 4; x < 76; x++) { putPixel(x, 52, COL_MIDLINE, fb, w); putPixel(x, 87, COL_MIDLINE, fb, w); }
    for (int y = 52; y < 88; y++) { putPixel(4, y, COL_MIDLINE, fb, w); putPixel(75, y, COL_MIDLINE, fb, w); }

    for (int x = 84; x < 156; x++) { putPixel(x, 52, COL_MIDLINE, fb, w); putPixel(x, 87, COL_MIDLINE, fb, w); }
    for (int y = 52; y < 88; y++) { putPixel(84, y, COL_MIDLINE, fb, w); putPixel(155, y, COL_MIDLINE, fb, w); }

    // Draw ENTRADAS / SALIDAS labels centered inside cards
    drawSmallText(16, 56, "ENTRADAS", COL_BLUE, fb, w);
    drawSmallText(99, 56, "SALIDAS", COL_GREEN, fb, w);

    // Draw giant bold numbers
    drawBoldNumber(40, 64, in, COL_BLUE, fb, w);
    drawBoldNumber(120, 64, out, COL_GREEN, fb, w);
}

void TftStatsView::drawMetrics(uint16_t* fb, int w, int h, const TftSnapshot& snap) {
    // Draw horizontal divider line at y = 96
    for (int px = 0; px < w; px += 2) {
        putPixel(px, 96, COL_MIDLINE, fb, w);
    }

    const int y0 = 104;
    char buf[16];

    int ocup = (int)snap.count_in - (int)snap.count_out;
    if (ocup < 0) ocup = 0;

    snprintf(buf, sizeof(buf), "OC:%d", ocup);
    drawSmallText(4, y0, buf, COL_AMBER, fb, w);

    snprintf(buf, sizeof(buf), "TR:%d", snap.num_tracks);
    drawSmallText(52, y0, buf, COL_WHITE, fb, w);

    if (snap.sensor_ok) {
        snprintf(buf, sizeof(buf), "%.1fC", snap.ambient_temp);
        drawSmallText(92, y0, buf, COL_CYAN, fb, w);

        drawSmallText(122, y0, "/", COL_GRAY, fb, w);

        snprintf(buf, sizeof(buf), "%.1fC", snap.sensor_temp);
        drawSmallText(130, y0, buf, COL_AMBER, fb, w);
    } else {
        drawSmallText(92, y0, "--.-C", COL_CYAN, fb, w);
        drawSmallText(122, y0, "/", COL_GRAY, fb, w);
        drawSmallText(130, y0, "--.-C", COL_AMBER, fb, w);
    }
}

void TftStatsView::drawCrossingInfo(uint16_t* fb, int w, int h, uint32_t now_ms) {
    const int y0 = 107;
    char buf[32];

    if (cross_ts_count_ > 0) {
        int first_ts_idx = (cross_ts_idx_ - cross_ts_count_ + 60) % 60;
        uint32_t oldest = cross_timestamps_[first_ts_idx];
        uint32_t window = now_ms - oldest;
        if (window > 0 && window <= 60000) {
            int rate = (cross_ts_count_ * 60000) / (int)window;
            snprintf(buf, sizeof(buf), "+%d/min", rate);
            drawSmallText(2, y0, buf, COL_WHITE, fb, w);
        }
    }

    if (last_cross_ms_ > 0) {
        uint32_t ago_ms = now_ms - last_cross_ms_;
        const char* arrow = last_cross_was_in_ ? "^" : "v";
        if (ago_ms < 60000) {
            snprintf(buf, sizeof(buf), "%s hace %lus", arrow, (unsigned long)(ago_ms / 1000));
        } else {
            snprintf(buf, sizeof(buf), "%s hace %lum", arrow, (unsigned long)(ago_ms / 60000));
        }
        int len = (int)strlen(buf) * 6;
        drawSmallText(w - len - 2, y0, buf, COL_GRAY, fb, w);
    }
}

void TftStatsView::drawCrossingLog(uint16_t* fb, int w, int h) {
    int y = 77;
    int count = num_records_ < 2 ? num_records_ : 2;
    for (int i = 0; i < count; i++) {
        const auto& rec = last_records_[i];
        
        // Time
        drawSmallText(4, y, rec.time_str, COL_GRAY, fb, w);
        
        // Direction
        const char* dir_str = rec.is_in ? "IN " : "OUT";
        uint16_t dir_col = rec.is_in ? COL_BLUE : COL_GREEN;
        drawSmallText(60, y, dir_str, dir_col, fb, w);
        
        // Temperature
        char temp_buf[12];
        snprintf(temp_buf, sizeof(temp_buf), "%.1fC", rec.temp);
        drawSmallText(90, y, temp_buf, COL_AMBER, fb, w);
        
        // Counters
        char cnt_buf[16];
        snprintf(cnt_buf, sizeof(cnt_buf), "%u/%u", rec.count_in, rec.count_out);
        drawSmallText(130, y, cnt_buf, COL_WHITE, fb, w);
        
        y += 9;
    }
}

void TftStatsView::onEnter() {
    ESP_LOGI(TAG, "Stats view active");
}

void TftStatsView::onExit() {
}

void TftStatsView::render(const TftSnapshot& snap, uint16_t* fb, int w, int h) {
    fillRect(0, 0, w, h, COL_DARK, fb, w);

    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    // Advance time bins (10 seconds each)
    if (last_bin_update_ms_ == 0) {
        last_bin_update_ms_ = now_ms;
    }
    uint32_t elapsed = now_ms - last_bin_update_ms_;
    if (elapsed >= 10000) {
        int steps = elapsed / 10000;
        for (int s = 0; s < steps; s++) {
            current_bin_idx_ = (current_bin_idx_ + 1) % NUM_BINS;
            bins_[current_bin_idx_] = {};
        }
        last_bin_update_ms_ = now_ms - (elapsed % 10000);
    }

    if (prev_frame_id_ == 0) {
        prev_count_in_ = snap.count_in;
        prev_count_out_ = snap.count_out;
        prev_frame_id_ = snap.frame_id;
        drawActivityHistogram(fb, w, h);
        drawCounters(fb, w, h, snap.count_in, snap.count_out);
        drawMetrics(fb, w, h, snap);
        return;
    }

    if (snap.count_in < prev_count_in_ || snap.count_out < prev_count_out_) {
        current_bin_idx_ = 0;
        last_bin_update_ms_ = now_ms;
        memset(bins_, 0, sizeof(bins_));
    }

    if (snap.frame_id != prev_frame_id_) {
        if (snap.count_in > prev_count_in_) {
            int diff = snap.count_in - prev_count_in_;
            bins_[current_bin_idx_].count_in += diff;
        }
        if (snap.count_out > prev_count_out_) {
            int diff = snap.count_out - prev_count_out_;
            bins_[current_bin_idx_].count_out += diff;
        }
        prev_count_in_ = snap.count_in;
        prev_count_out_ = snap.count_out;
        prev_frame_id_ = snap.frame_id;
    }

    drawActivityHistogram(fb, w, h);
    drawCounters(fb, w, h, snap.count_in, snap.count_out);
    drawMetrics(fb, w, h, snap);
}
