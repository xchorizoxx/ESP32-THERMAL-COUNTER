#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <atomic>
#include "stdint.h"
#include <cstdio>

class ThermalRecorder {
public:
    // --- Ring buffer size ---
    static constexpr int RING_BUF_SLOTS   = 64;    // ~2s pre-roll at 32Hz
    static constexpr int TOTAL_PIXELS     = 768;   // 32x24

    struct FrameSlot {
        int16_t  pixels[TOTAL_PIXELS];
        uint32_t timestamp_ms;
        int8_t   track_count;
        int8_t   cross_dir;   // 0=none, 1=IN, -1=OUT
    };

    // --- Configurable parameters (safe to modify from Core 0) ---
    static uint32_t pre_roll_ms;             // default 1000
    static uint32_t cooldown_ms;             // default 500  (cooldown = post_roll)
    static uint32_t max_duration_ms;         // default 30000
    static uint32_t min_duration_ms;         // default 1000

    // --- Lifecycle ---
    static esp_err_t init();
    static bool      isActive();             // true when PSRAM allocated

    // --- Called from Core 1 (pipeline) — fast, no allocation ---
    static void IRAM_ATTR pushFrame(const float* pixels_degC, int num_tracks, int cross_dir);

    // --- Called from Core 1 — just pings the cooldown timer (optional) ---
    static bool clipRecordingNow();          // true if currently recording
    static const char* getCurrentClipId();    // "CLIP_00005.thv" or ""

    // --- Writer task (Core 0) ---
    static void writerTask(void* pv);

private:
    static FrameSlot*      s_ring_buf_;        // PSRAM allocation
    static int             s_N_;
    static std::atomic<int>   s_write_idx_;        // Core 1 writes
    static int             s_read_idx_;         // Core 0 reads

    enum State : uint8_t { IDLE, RECORDING, COOLDOWN, CLOSING };
    static State           s_state_;

    // Pre-roll rewind tracking
    static uint32_t        s_clip_start_ms_;
    static uint32_t        s_last_track_ms_;
    static uint32_t        s_clip_frame_count_;
    static uint32_t        s_clip_crossings_;

    // Clip file
    static uint32_t        s_clip_counter_;
    static char            s_clip_path_[64];
    static FILE*           s_clip_file_;

    // Helpers
    static void startClip(uint32_t now_ms);
    static void closeClip();
};

// .thv file header (16 bytes) — accessible by clip list API
struct ThvHeader {
    uint32_t magic;           // "THV\0"
    uint16_t version;         // 1
    uint16_t frame_count;     // N frames in this file
    uint8_t  width;           // 32
    uint8_t  height;          // 24
    uint8_t  fps;             // 32
    uint8_t  trigger_dir;     // 0=unknown, 1=IN, 2=OUT
    uint8_t  reserved[4];
};
static_assert(sizeof(ThvHeader) == 16, "ThvHeader must be 16 bytes");

static constexpr uint32_t THV_MAGIC = 0x00564854;  // "THV\0" little-endian
