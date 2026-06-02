#include "tft_stats_view.hpp"
#include "tft_config.hpp"
#include "esp_log.h"
#include <cstring>
#include <math.h>

const char* TftStatsView::TAG = "TFT_STATS";

static constexpr uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return (c >> 8) | (c << 8);
}

static const uint16_t COL_BLUE    = RGB565(59, 130, 246);   // #3b82f6 IN
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

void TftStatsView::drawSparkline(uint16_t* fb, int w, int h) {
    const int SPARK_Y0 = 0;
    const int SPARK_H = 40;
    const int MID_Y = SPARK_Y0 + SPARK_H / 2;
    const int AMP = 10;
    const int X_SPACING = 6;
    const int X_OFFSET = 8;

    fillRect(0, SPARK_Y0, w, SPARK_H, COL_DARK, fb, w);

    for (int px = 0; px < w; px += 4) {
        putPixel(px, MID_Y, COL_MIDLINE, fb, w);
    }

    if (spark_count_ == 0) {
        drawSmallText(w / 2 - 20, MID_Y - 4, "sin datos", COL_GRAY, fb, w);
        return;
    }

    int first = spark_count_ > MAX_SPARK ? spark_count_ - MAX_SPARK : 0;
    int count = spark_count_ < MAX_SPARK ? spark_count_ : MAX_SPARK;

    for (int i = 0; i < count; i++) {
        int si = (first + i) % MAX_SPARK;
        if (spark_data_[si] == 0) continue;

        int x0 = X_OFFSET + i * X_SPACING;
        int y0 = MID_Y + (spark_data_[si] > 0 ? -AMP : AMP);
        uint16_t col = spark_data_[si] > 0 ? COL_BLUE : COL_GREEN;

        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                putPixel(x0 + dx, y0 + dy, col, fb, w);
            }
        }

        if (i > 0) {
            int prev_si = (first + i - 1) % MAX_SPARK;
            if (spark_data_[prev_si] != 0) {
                int x1 = X_OFFSET + (i - 1) * X_SPACING;
                int y1 = MID_Y + (spark_data_[prev_si] > 0 ? -AMP : AMP);
                drawLine(x1, y1, x0, y0, col, fb, w);
            }
        }
    }

    char label[16];
    snprintf(label, sizeof(label), "%d cruces", spark_count_);
    drawSmallText(w - 60, 2, label, COL_GRAY, fb, w);
}

void TftStatsView::drawCounters(uint16_t* fb, int w, int h, uint16_t in, uint16_t out) {
    const int y0 = 42;
    const int half_w = w / 2;

    drawSmallText(half_w / 2 - 24, y0, "ENTRADAS", COL_BLUE, fb, w);
    drawSmallText(half_w + half_w / 2 - 20, y0, "SALIDAS", COL_GREEN, fb, w);

    drawBoldNumber(half_w / 2, y0 + 12, in, COL_BLUE, fb, w);
    drawBoldNumber(half_w + half_w / 2, y0 + 12, out, COL_GREEN, fb, w);
}

void TftStatsView::drawMetrics(uint16_t* fb, int w, int h, const TftSnapshot& snap) {
    const int y0 = 76;
    const int col_w = w / 4;
    char buf[16];

    int ocup = (int)snap.count_in - (int)snap.count_out;
    if (ocup < 0) ocup = 0;

    snprintf(buf, sizeof(buf), "OCUP:%d", ocup);
    drawSmallText(2, y0, buf, COL_AMBER, fb, w);

    snprintf(buf, sizeof(buf), "TRK:%d", snap.num_tracks);
    drawSmallText(col_w + 2, y0, buf, COL_WHITE, fb, w);

    if (snap.sensor_ok) {
        snprintf(buf, sizeof(buf), "AMB:%.1f", snap.ambient_temp);
        drawSmallText(col_w * 2 + 2, y0, buf, COL_CYAN, fb, w);

        snprintf(buf, sizeof(buf), "SEN:%.1f", snap.ambient_temp);
        drawSmallText(col_w * 3 + 2, y0, buf, COL_AMBER, fb, w);
    } else {
        drawSmallText(col_w * 2 + 2, y0, "AMB:---", COL_CYAN, fb, w);
        drawSmallText(col_w * 3 + 2, y0, "SEN:---", COL_AMBER, fb, w);
    }
}

void TftStatsView::drawCrossingInfo(uint16_t* fb, int w, int h, uint32_t now_ms) {
    const int y0 = 98;
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

void TftStatsView::onEnter() {
    ESP_LOGI(TAG, "Stats view active");
}

void TftStatsView::onExit() {
}

void TftStatsView::render(const TftSnapshot& snap, uint16_t* fb, int w, int h) {
    fillRect(0, 0, w, h, COL_DARK, fb, w);

    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    if (prev_frame_id_ == 0) {
        prev_count_in_ = snap.count_in;
        prev_count_out_ = snap.count_out;
        prev_frame_id_ = snap.frame_id;
        drawSparkline(fb, w, h);
        drawCounters(fb, w, h, snap.count_in, snap.count_out);
        drawMetrics(fb, w, h, snap);
        drawCrossingInfo(fb, w, h, now_ms);
        return;
    }

    if (snap.count_in < prev_count_in_ || snap.count_out < prev_count_out_) {
        spark_count_ = 0;
        cross_ts_count_ = 0;
        cross_ts_idx_ = 0;
        memset(spark_data_, 0, sizeof(spark_data_));
    }

    if (snap.frame_id != prev_frame_id_) {
        if (snap.count_in > prev_count_in_) {
            int diff = snap.count_in - prev_count_in_;
            for (int i = 0; i < diff && i < 5; i++) {
                spark_data_[spark_count_ % MAX_SPARK] = 1;
                spark_count_++;
                last_cross_was_in_ = true;
                last_cross_ms_ = now_ms;
                cross_timestamps_[cross_ts_idx_] = now_ms;
                cross_ts_idx_ = (cross_ts_idx_ + 1) % 60;
                if (cross_ts_count_ < 60) cross_ts_count_++;
            }
        }
        if (snap.count_out > prev_count_out_) {
            int diff = snap.count_out - prev_count_out_;
            for (int i = 0; i < diff && i < 5; i++) {
                spark_data_[spark_count_ % MAX_SPARK] = -1;
                spark_count_++;
                last_cross_was_in_ = false;
                last_cross_ms_ = now_ms;
                cross_timestamps_[cross_ts_idx_] = now_ms;
                cross_ts_idx_ = (cross_ts_idx_ + 1) % 60;
                if (cross_ts_count_ < 60) cross_ts_count_++;
            }
        }
        prev_count_in_ = snap.count_in;
        prev_count_out_ = snap.count_out;
        prev_frame_id_ = snap.frame_id;
    }

    drawSparkline(fb, w, h);
    drawCounters(fb, w, h, snap.count_in, snap.count_out);
    drawMetrics(fb, w, h, snap);
    drawCrossingInfo(fb, w, h, now_ms);
}
