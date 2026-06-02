#include "tft_info_view.hpp"
#include "tft_config.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
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

    char h1, h2, m1, m2, s1, s2;

    if (!sys.rtc_ok || sys.time_str[0] == '\0') {
        // Fallback to uptime formatted as HH:MM:SS
        uint32_t sec = sys.uptime_sec;
        uint32_t hrs = sec / 3600;
        uint32_t mins = (sec % 3600) / 60;
        uint32_t secs = sec % 60;
        
        char up_buf[9];
        snprintf(up_buf, sizeof(up_buf), "%02lu:%02lu:%02lu", (unsigned long)(hrs % 100), (unsigned long)mins, (unsigned long)secs);
        h1 = up_buf[0]; h2 = up_buf[1];
        m1 = up_buf[3]; m2 = up_buf[4];
        s1 = up_buf[6]; s2 = up_buf[7];
    } else {
        h1 = sys.time_str[0];
        h2 = sys.time_str[1];
        m1 = sys.time_str[3];
        m2 = sys.time_str[4];
        s1 = sys.time_str[6];
        s2 = sys.time_str[7];
    }

    uint16_t clock_col_h = sys.rtc_ok ? COL_CYAN : COL_GRAY;
    uint16_t clock_col_m = sys.rtc_ok ? COL_AMBER : COL_GRAY;

    drawBigDigit(40, 42, h1, clock_col_h, fb, w);
    drawBigDigit(57, 42, h2, clock_col_h, fb, w);

    if (colon_on) {
        drawDot(80, 53, 2, COL_WHITE, fb, w);
        drawDot(80, 62, 2, COL_WHITE, fb, w);
    } else {
        drawDot(80, 53, 2, COL_DIM_GRAY, fb, w);
        drawDot(80, 62, 2, COL_DIM_GRAY, fb, w);
    }

    drawBigDigit(88, 42, m1, clock_col_m, fb, w);
    drawBigDigit(105, 42, m2, clock_col_m, fb, w);

    // Seconds below clock
    char sec_str[4] = {};
    sec_str[0] = s1;
    sec_str[1] = s2;
    drawSmallString(74, 74, sec_str, COL_GRAY, fb, w);
}

void TftInfoView::drawDate(uint16_t* fb, int w, const SystemInfoSnapshot& sys) {
    if (!sys.rtc_ok || sys.date_str[0] == '\0') {
        drawSmallString(112, 2, "--/--/--", COL_GRAY, fb, w);
        return;
    }
    int len = (int)strlen(sys.date_str) * 6;
    drawSmallString(w - len - 2, 2, sys.date_str, COL_GRAY, fb, w);
}

void TftInfoView::drawSensorDots(uint16_t* fb, int w, int y, const SystemInfoSnapshot& sys, const TftSnapshot& snap) {
    char buf[16];

    // 6 columns centers: 13, 40, 67, 94, 121, 147

    // 1. OC (Ocupacion)
    int ocup = (int)snap.count_in - (int)snap.count_out;
    if (ocup < 0) ocup = 0;
    snprintf(buf, sizeof(buf), "OC:%d", ocup);
    drawSmallString(13 - ((int)strlen(buf) * 6) / 2, y + 4, buf, COL_AMBER, fb, w);

    // 2. TR (Tracks)
    snprintf(buf, sizeof(buf), "TR:%d", snap.num_tracks);
    drawSmallString(40 - ((int)strlen(buf) * 6) / 2, y + 4, buf, COL_WHITE, fb, w);

    // 3. MLX
    uint16_t mlx_col = sys.sensor_ok ? COL_GREEN : COL_RED;
    drawDot(67, y + 4, 3, mlx_col, fb, w);
    drawSmallString(67 - 9, y + 10, "MLX", mlx_col, fb, w);

    // 4. RTC
    uint16_t rtc_col = sys.rtc_ok ? COL_GREEN : COL_RED;
    drawDot(94, y + 4, 3, rtc_col, fb, w);
    drawSmallString(94 - 9, y + 10, "RTC", rtc_col, fb, w);

    // 5. SD
    uint16_t sd_col = sys.sd_ok ? COL_GREEN : COL_RED;
    drawDot(121, y + 4, 3, sd_col, fb, w);
    drawSmallString(121 - 6, y + 10, "SD", sd_col, fb, w);

    // 6. WiFi (WF)
    uint16_t wifi_col = sys.wifi_clients > 0 ? COL_GREEN : COL_RED;
    drawDot(147, y + 4, 3, wifi_col, fb, w);
    snprintf(buf, sizeof(buf), "WF%d", sys.wifi_clients);
    drawSmallString(147 - ((int)strlen(buf) * 6) / 2, y + 10, buf, wifi_col, fb, w);
}

void TftInfoView::drawTemps(uint16_t* fb, int w, const SystemInfoSnapshot& sys) {
    char temp_amb_str[16];
    char temp_sens_str[16];

    if (sys.sensor_ok) {
        snprintf(temp_amb_str, sizeof(temp_amb_str), "%.1fC", sys.ambient_temp);
        snprintf(temp_sens_str, sizeof(temp_sens_str), "%.1fC", sys.sensor_temp);
    } else {
        snprintf(temp_amb_str, sizeof(temp_amb_str), "--.-C");
        snprintf(temp_sens_str, sizeof(temp_sens_str), "--.-C");
    }

    // Coordinates to center: separator '/' in x = 77
    drawSmallString(41, 88, temp_amb_str, COL_CYAN, fb, w);
    drawSmallString(77, 88, "/", COL_GRAY, fb, w);
    drawSmallString(89, 88, temp_sens_str, COL_AMBER, fb, w);
}

void TftInfoView::drawUptime(uint16_t* fb, int w, int h, const SystemInfoSnapshot& sys) {
    uint32_t sec = sys.uptime_sec;
    char val_buf[24];

    if (sec < 60) {
        snprintf(val_buf, sizeof(val_buf), "%lus", (unsigned long)sec);
    } else if (sec < 3600) {
        snprintf(val_buf, sizeof(val_buf), "%lum", (unsigned long)(sec / 60));
    } else {
        uint32_t hrs = sec / 3600;
        uint32_t mins = (sec % 3600) / 60;
        snprintf(val_buf, sizeof(val_buf), "%luh %lum", (unsigned long)hrs, (unsigned long)mins);
    }

    char full_buf[32];
    snprintf(full_buf, sizeof(full_buf), "Uptime: %s", val_buf);

    int len = (int)strlen(full_buf) * 6;
    drawSmallString((w - len) / 2, 15, full_buf, COL_GRAY, fb, w);
}

void TftInfoView::onEnter() {
    ESP_LOGI(TAG, "Info view active");
}

void TftInfoView::onExit() {
}

void TftInfoView::render(const TftSnapshot& snap, uint16_t* fb, int w, int h) {
    SystemInfoSnapshot sys = readSystemSnapshot();

    fillRect(0, 0, w, h, COL_DARK, fb, w);

    drawDate(fb, w, sys);
    drawClock(fb, w, sys);
    drawSensorDots(fb, w, 105, sys, snap);
    drawTemps(fb, w, sys);
    drawUptime(fb, w, h, sys);

    // Render RAM telemetry in percentage centered at y = 27
    uint32_t free_ram = esp_get_free_internal_heap_size();
    uint32_t total_ram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    int ram_pct = (total_ram > 0) ? (int)((free_ram * 100) / total_ram) : 0;
    char ram_buf[24];
    snprintf(ram_buf, sizeof(ram_buf), "RAM: %d%%", ram_pct);
    int ram_len = (int)strlen(ram_buf) * 6;
    drawSmallString((w - ram_len) / 2, 27, ram_buf, COL_GRAY, fb, w);
}
