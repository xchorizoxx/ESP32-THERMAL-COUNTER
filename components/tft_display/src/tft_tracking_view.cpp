#include "tft_tracking_view.hpp"
#include "tft_config.hpp"
#include "esp_log.h"
#include <math.h>

const char* TftTrackingView::TAG = "TFT_TRACK";

static constexpr uint16_t overlay_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return (c >> 8) | (c << 8);
}

static const uint16_t COLOR_LINE_ENTRY = overlay_rgb565(0, 255, 136);
static const uint16_t COLOR_LINE_EXIT  = overlay_rgb565(0, 212, 255);
static const uint16_t COLOR_SEG_LINE   = overlay_rgb565(0, 255, 136);
static const uint16_t COLOR_TRACK_HOT  = overlay_rgb565(255, 0, 0);    // Rojo intenso
static const uint16_t COLOR_TRACK_WARM = overlay_rgb565(180, 0, 0);    // Rojo oscuro
static const uint16_t COLOR_TRACK_COLD = overlay_rgb565(110, 0, 0);    // Rojo muy oscuro

static constexpr int TRACK_CIRCLE_R = 7;

void TftTrackingView::onEnter() {
    if (!initialized_) {
        renderer_.init();
        initialized_ = true;
    }
    ESP_LOGI(TAG, "Tracking view active");
}

void TftTrackingView::onExit() {
}

void TftTrackingView::render(const TftSnapshot& snap,
                              uint16_t* fb, int w, int h) {
    drawHeatmap(snap.pixels, fb, w, h);

    if (!snap.sensor_ok) {
        return;
    }

    if (snap.num_tracks > 0) {
        drawTracks(snap.tracks, snap.num_tracks, fb, w, h);
    }

    drawLines(snap.door_lines, fb, w, h);
}

void TftTrackingView::drawHeatmap(const int16_t* pixels,
                                   uint16_t* fb, int w, int h) {
    renderer_.renderHeatmap(pixels, fb, w, h);
}

void TftTrackingView::clampSensorCoords(float& x, float& y) {
    if (!isfinite(x)) x = 0.0f;
    if (!isfinite(y)) y = 0.0f;
    if (x < 0.0f) x = 0.0f;
    if (x > 31.0f) x = 31.0f;
    if (y < 0.0f) y = 0.0f;
    if (y > 23.0f) y = 23.0f;
}

void TftTrackingView::putPixel(int px, int py, uint16_t color,
                                uint16_t* fb, int w, int h) {
    if (px < 0 || px >= w || py < 0 || py >= h) return;
    fb[py * w + px] = color;
}

void TftTrackingView::fillCircle(int cx, int cy, int r, uint16_t color,
                                  uint16_t* fb, int w, int h) {
    int y0 = cy - r;
    int y1 = cy + r;
    int x0 = cx - r;
    int x1 = cx + r;
    int rr = r * r;
    for (int py = y0; py <= y1; py++) {
        for (int px = x0; px <= x1; px++) {
            int dx = px - cx;
            int dy = py - cy;
            if (dx * dx + dy * dy <= rr) {
                putPixel(px, py, color, fb, w, h);
            }
        }
    }
}

void TftTrackingView::drawLineSeg(int x0, int y0, int x1, int y1,
                                   uint16_t color, uint16_t* fb, int w, int h) {
    auto drawThick = [&](int off_x, int off_y) {
        int bx = x0 + off_x, by = y0 + off_y;
        int ex = x1 + off_x, ey = y1 + off_y;
        int dx = abs(ex - bx);
        int dy = -abs(ey - by);
        int sx = bx < ex ? 1 : -1;
        int sy = by < ey ? 1 : -1;
        int err = dx + dy;
        while (true) {
            putPixel(bx, by, color, fb, w, h);
            if (bx == ex && by == ey) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; bx += sx; }
            if (e2 <= dx) { err += dx; by += sy; }
        }
    };

    drawThick(0, 0);

    int dx = x1 - x0;
    int dy = y1 - y0;
    if (abs(dx) >= abs(dy)) {
        drawThick(0, 1);
    } else {
        drawThick(1, 0);
    }
}

void TftTrackingView::drawTracks(const TrackInfo* tracks, int num_tracks,
                                  uint16_t* fb, int w, int h) {
    const float x_scale = (float)(TftConfig::WIDTH - 1) / (float)(TftConfig::SRC_COLS - 1);
    const float y_scale = (float)(TftConfig::HEIGHT - 1) / (float)(TftConfig::SRC_ROWS - 1);

    for (int i = 0; i < num_tracks; i++) {
        const TrackInfo& t = tracks[i];

        float sx = t.x_100 / 100.0f;
        float sy = t.y_100 / 100.0f;
        clampSensorCoords(sx, sy);

        int dx = (int)(sx * x_scale);
        int dy = (int)(sy * y_scale);

        uint16_t color;
        if (t.peak_temp_100 > 3500) {
            color = COLOR_TRACK_HOT;
        } else if (t.peak_temp_100 > 3000) {
            color = COLOR_TRACK_WARM;
        } else {
            color = COLOR_TRACK_COLD;
        }

        fillCircle(dx, dy, TRACK_CIRCLE_R, color, fb, w, h);
    }
}

void TftTrackingView::drawLines(const ThermalConfig::DoorLineConfig& lines,
                                 uint16_t* fb, int w, int h) {
    const float y_scale = (float)(TftConfig::HEIGHT - 1) / (float)(TftConfig::SRC_ROWS - 1);

    if (!lines.use_segments) {
        int entry_dy = (int)((float)ThermalConfig::DEFAULT_LINE_ENTRY_Y * y_scale);
        int exit_dy  = (int)((float)ThermalConfig::DEFAULT_LINE_EXIT_Y  * y_scale);

        for (int px = 0; px < w; px++) {
            putPixel(px, entry_dy, COLOR_LINE_ENTRY, fb, w, h);
            putPixel(px, entry_dy + 1, COLOR_LINE_ENTRY, fb, w, h);

            putPixel(px, exit_dy, COLOR_LINE_EXIT, fb, w, h);
            putPixel(px, exit_dy + 1, COLOR_LINE_EXIT, fb, w, h);
        }
        return;
    }

    if (lines.num_lines == 0) return;

    const float x_scale = (float)(TftConfig::WIDTH - 1) / (float)(TftConfig::SRC_COLS - 1);

    for (int i = 0; i < lines.num_lines; i++) {
        const CountingSegment& seg = lines.lines[i];
        if (!seg.enabled) continue;

        float x1s = seg.x1, y1s = seg.y1;
        float x2s = seg.x2, y2s = seg.y2;
        clampSensorCoords(x1s, y1s);
        clampSensorCoords(x2s, y2s);

        int x1d = (int)(x1s * x_scale);
        int y1d = (int)(y1s * y_scale);
        int x2d = (int)(x2s * x_scale);
        int y2d = (int)(y2s * y_scale);

        drawLineSeg(x1d, y1d, x2d, y2d, COLOR_SEG_LINE, fb, w, h);
    }
}
