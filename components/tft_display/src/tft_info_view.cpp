#include "tft_info_view.hpp"
#include "tft_config.hpp"
#include "esp_log.h"
#include <cstring>
#include <math.h>

const char* TftInfoView::TAG = "TFT_INFO";

static constexpr uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return (c >> 8) | (c << 8);
}

static const uint16_t COL_CYAN     = RGB565(0, 212, 255);
static const uint16_t COL_AMBER    = RGB565(255, 179, 0);
static const uint16_t COL_GRAY     = RGB565(107, 114, 128);
static const uint16_t COL_WHITE    = RGB565(255, 255, 255);
static const uint16_t COL_GREEN    = RGB565(16, 185, 129);
static const uint16_t COL_RED      = RGB565(239, 68, 68);
static const uint16_t COL_DARK     = RGB565(20, 20, 30);
static const uint16_t COL_DIM_GRAY = RGB565(60, 60, 70);

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

static void fillRect(int x, int y, int rw, int rh, uint16_t color, uint16_t* fb, int w) {
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

void TftInfoView::putPixel(int x, int y, uint16_t color, uint16_t* fb, int w) {
    if (x < 0 || x >= w || y < 0 || y >= TftConfig::HEIGHT) return;
    fb[y * w + x] = color;
}

void TftInfoView::drawBigDigit(int x, int y, char digit, uint16_t color, uint16_t* fb, int w) {
    if (digit < 0x20 || digit > 0x7E) digit = ' ';
    const uint8_t* cols = FONT5x7[digit - 0x20];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (cols[col] & (1 << row)) {
                for (int dx = 0; dx < 3; dx++) {
                    for (int dy = 0; dy < 3; dy++) {
                        int px = x + col * 3 + dx;
                        int py = y + row * 3 + dy;
                        if (px >= 0 && px < w && py >= 0 && py < TftConfig::HEIGHT) {
                            fb[py * w + px] = color;
                        }
                    }
                }
            }
        }
    }
}

void TftInfoView::drawSmallChar(int x, int y, char c, uint16_t color, uint16_t* fb, int w) {
    if (c < 0x20 || c > 0x7E) c = ' ';
    const uint8_t* cols = FONT5x7[c - 0x20];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (cols[col] & (1 << row)) {
                putPixel(x + col, y + row, color, fb, w);
            }
        }
    }
}

void TftInfoView::drawSmallString(int x, int y, const char* str, uint16_t color, uint16_t* fb, int w) {
    if (!str) return;
    int cx = x;
    while (*str) {
        drawSmallChar(cx, y, *str, color, fb, w);
        cx += 6;
        str++;
    }
}

void TftInfoView::drawDot(int cx, int cy, int r, uint16_t color, uint16_t* fb, int w) {
    int rr = r * r;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= rr) {
                putPixel(cx + dx, cy + dy, color, fb, w);
            }
        }
    }
}

void TftInfoView::drawClock(uint16_t* fb, int w, const SystemInfoSnapshot& sys) {
    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    bool colon_on = (now_ms % 1000) < 500;

    // HH:MM with big digits
    if (!sys.rtc_ok || sys.time_str[0] == '\0') {
        drawBigDigit(40, 14, '-', COL_CYAN, fb, w);
        drawBigDigit(56, 14, '-', COL_CYAN, fb, w);
        if (colon_on) {
            drawDot(70, 25, 2, COL_WHITE, fb, w);
            drawDot(70, 34, 2, COL_WHITE, fb, w);
        }
        drawBigDigit(88, 14, '-', COL_AMBER, fb, w);
        drawBigDigit(104, 14, '-', COL_AMBER, fb, w);
        drawSmallString(76, 46, "--", COL_GRAY, fb, w);
        return;
    }

    char h1 = sys.time_str[0];
    char h2 = sys.time_str[1];
    char m1 = sys.time_str[3];
    char m2 = sys.time_str[4];
    char s1 = sys.time_str[6];
    char s2 = sys.time_str[7];

    drawBigDigit(40, 14, h1, COL_CYAN, fb, w);
    drawBigDigit(56, 14, h2, COL_CYAN, fb, w);

    if (colon_on) {
        drawDot(70, 25, 2, COL_WHITE, fb, w);
        drawDot(70, 34, 2, COL_WHITE, fb, w);
    } else {
        drawDot(70, 25, 2, COL_DIM_GRAY, fb, w);
        drawDot(70, 34, 2, COL_DIM_GRAY, fb, w);
    }

    drawBigDigit(88, 14, m1, COL_AMBER, fb, w);
    drawBigDigit(104, 14, m2, COL_AMBER, fb, w);

    // Seconds below clock
    char sec_str[4] = {};
    sec_str[0] = s1;
    sec_str[1] = s2;
    drawSmallString(76, 46, sec_str, COL_GRAY, fb, w);

    // Top-left: ambient temperature
    char temp_str[12];
    if (sys.sensor_ok) {
        snprintf(temp_str, sizeof(temp_str), "%.1fC", sys.ambient_temp);
    } else {
        snprintf(temp_str, sizeof(temp_str), "--.-C");
    }
    drawSmallString(2, 2, temp_str, COL_CYAN, fb, w);
}

void TftInfoView::drawDate(uint16_t* fb, int w, const SystemInfoSnapshot& sys) {
    if (!sys.rtc_ok || sys.date_str[0] == '\0') {
        drawSmallString(112, 2, "--/--/--", COL_GRAY, fb, w);
        return;
    }
    int len = (int)strlen(sys.date_str) * 6;
    drawSmallString(w - len - 2, 2, sys.date_str, COL_GRAY, fb, w);
}

void TftInfoView::drawSensorDots(uint16_t* fb, int w, int y, const SystemInfoSnapshot& sys) {
    const int col_w = w / 4;
    const int dot_centers[4] = { col_w / 2, col_w + col_w / 2, col_w * 2 + col_w / 2, col_w * 3 + col_w / 2 };
    const int label_x[4] = { 4, col_w + 4, col_w * 2 + 4, col_w * 3 + 4 };
    char buf[16];

    // MLX
    if (sys.sensor_ok) {
        drawDot(dot_centers[0], y + 4, 3, COL_GREEN, fb, w);
        drawSmallString(label_x[0], y + 10, "MLX OK", COL_GREEN, fb, w);
    } else {
        drawDot(dot_centers[0], y + 4, 3, COL_RED, fb, w);
        drawSmallString(label_x[0], y + 10, "MLX NO", COL_RED, fb, w);
    }

    // RTC
    if (sys.rtc_ok) {
        drawDot(dot_centers[1], y + 4, 3, COL_GREEN, fb, w);
        drawSmallString(label_x[1], y + 10, "RTC OK", COL_GREEN, fb, w);
    } else {
        drawDot(dot_centers[1], y + 4, 3, COL_RED, fb, w);
        drawSmallString(label_x[1], y + 10, "RTC NO", COL_RED, fb, w);
    }

    // SD
    if (sys.sd_ok) {
        drawDot(dot_centers[2], y + 4, 3, COL_GREEN, fb, w);
        drawSmallString(label_x[2], y + 10, "SD OK", COL_GREEN, fb, w);
    } else {
        drawDot(dot_centers[2], y + 4, 3, COL_RED, fb, w);
        drawSmallString(label_x[2], y + 10, "SD NO", COL_RED, fb, w);
    }

    // WiFi
    {
        uint16_t wifi_col = sys.wifi_clients > 0 ? COL_GREEN : COL_RED;
        drawDot(dot_centers[3], y + 4, 3, wifi_col, fb, w);
        snprintf(buf, sizeof(buf), "WiFi %d", sys.wifi_clients);
        drawSmallString(label_x[3], y + 10, buf, wifi_col, fb, w);
    }

    // Sensor temperature centered below
    if (sys.sensor_ok) {
        snprintf(buf, sizeof(buf), "%.1f C", sys.ambient_temp);
    } else {
        snprintf(buf, sizeof(buf), "--.- C");
    }
    int tlen = (int)strlen(buf) * 6;
    drawSmallString((w - tlen) / 2, y + 20, buf, COL_AMBER, fb, w);
}

void TftInfoView::drawTemps(uint16_t* fb, int w, const SystemInfoSnapshot& sys) {
    (void)fb; (void)w; (void)sys;
    // Temps are drawn inside drawSensorDots
}

void TftInfoView::drawUptime(uint16_t* fb, int w, int h, const SystemInfoSnapshot& sys) {
    uint32_t sec = sys.uptime_sec;
    char buf[24];

    if (sec < 60) {
        snprintf(buf, sizeof(buf), "%lus", (unsigned long)sec);
    } else if (sec < 3600) {
        snprintf(buf, sizeof(buf), "%lum", (unsigned long)(sec / 60));
    } else {
        uint32_t hrs = sec / 3600;
        uint32_t mins = (sec % 3600) / 60;
        snprintf(buf, sizeof(buf), "%luh %lum", (unsigned long)hrs, (unsigned long)mins);
    }

    int len = (int)strlen(buf) * 6;
    drawSmallString(w - len - 2, h - 12, buf, COL_GRAY, fb, w);
}

void TftInfoView::onEnter() {
    ESP_LOGI(TAG, "Info view active");
}

void TftInfoView::onExit() {
}

void TftInfoView::render(const TftSnapshot& snap, uint16_t* fb, int w, int h) {
    (void)snap;
    SystemInfoSnapshot sys = readSystemSnapshot();

    fillRect(0, 0, w, h, COL_DARK, fb, w);

    drawDate(fb, w, sys);
    drawClock(fb, w, sys);
    drawSensorDots(fb, w, 64, sys);
    drawUptime(fb, w, h, sys);
}
