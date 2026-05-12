#include "thermal_recorder.hpp"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <cstring>
#include <cerrno>

static const char* TAG = "RECORDER";

// ---------------------------------------------------------------------------
// Static members
// ---------------------------------------------------------------------------
ThermalRecorder::FrameSlot* ThermalRecorder::s_ring_buf_   = nullptr;
int                         ThermalRecorder::s_N_           = 0;
std::atomic<int>            ThermalRecorder::s_write_idx_   = 0;
int                         ThermalRecorder::s_read_idx_    = 0;

ThermalRecorder::State     ThermalRecorder::s_state_       = IDLE;

uint32_t ThermalRecorder::pre_roll_ms     = 1000;
uint32_t ThermalRecorder::cooldown_ms     = 500;
uint32_t ThermalRecorder::max_duration_ms = 30000;
uint32_t ThermalRecorder::min_duration_ms = 1000;

uint32_t ThermalRecorder::s_clip_start_ms_  = 0;
uint32_t ThermalRecorder::s_last_track_ms_  = 0;
uint32_t ThermalRecorder::s_clip_frame_count_ = 0;
uint32_t ThermalRecorder::s_clip_crossings_ = 0;

uint32_t ThermalRecorder::s_clip_counter_  = 0;
char     ThermalRecorder::s_clip_path_[64] = {};
FILE*    ThermalRecorder::s_clip_file_     = nullptr;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
static uint32_t nowMs() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

// ---------------------------------------------------------------------------
//  init
// ---------------------------------------------------------------------------
esp_err_t ThermalRecorder::init() {
    s_N_ = RING_BUF_SLOTS;
    size_t buf_size = s_N_ * sizeof(FrameSlot);
    s_ring_buf_ = (FrameSlot*)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!s_ring_buf_) {
        ESP_LOGW(TAG, "PSRAM not available — recording DISABLED");
        return ESP_ERR_NO_MEM;
    }
    memset(s_ring_buf_, 0, buf_size);
    ESP_LOGI(TAG, "Ring buffer: %d slots x %zu B = %zu KB in PSRAM",
             s_N_, sizeof(FrameSlot), buf_size / 1024);

    // Load clip counter from NVS
    nvs_handle_t h;
    if (nvs_open("thermal_registry", NVS_READONLY, &h) == ESP_OK) {
        int32_t v = 0;
        nvs_get_i32(h, "clip_num", &v);
        s_clip_counter_ = (uint32_t)v;
        nvs_close(h);
    }
    ESP_LOGI(TAG, "Clip counter starts at %lu", (unsigned long)s_clip_counter_);

    s_state_ = IDLE;
    s_write_idx_ = 0;
    s_read_idx_ = 0;

    static StaticTask_t s_task_buf;
    static StackType_t  s_task_stack[2048 / sizeof(StackType_t)];
    TaskHandle_t hdl = xTaskCreateStaticPinnedToCore(
        writerTask, "Recorder",
        2048 / sizeof(StackType_t), nullptr,
        tskIDLE_PRIORITY + 1, s_task_stack, &s_task_buf, 0);
    if (!hdl) {
        ESP_LOGE(TAG, "Failed to create writer task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Recorder writer task running on Core 0");
    return ESP_OK;
}

bool ThermalRecorder::isActive() { return s_ring_buf_ != nullptr; }

// ---------------------------------------------------------------------------
//  pushFrame  (Core 1 — fast, no allocation)
// ---------------------------------------------------------------------------
void IRAM_ATTR ThermalRecorder::pushFrame(const float* pixels_degC,
                                          int num_tracks,
                                          int cross_dir) {
    if (!s_ring_buf_) return;

    int w = s_write_idx_.load(std::memory_order_relaxed);
    int idx = w % s_N_;
    for (int i = 0; i < TOTAL_PIXELS; i++) {
        float v = pixels_degC[i] * 100.0f;
        if      (v > 32767.0f)  s_ring_buf_[idx].pixels[i] = 32767;
        else if (v < -32768.0f) s_ring_buf_[idx].pixels[i] = -32768;
        else                    s_ring_buf_[idx].pixels[i] = (int16_t)v;
    }
    s_ring_buf_[idx].timestamp_ms = nowMs();
    s_ring_buf_[idx].track_count  = (int8_t)(num_tracks > 127 ? 127 : num_tracks);
    s_ring_buf_[idx].cross_dir    = (int8_t)(cross_dir > 1 ? 1 : (cross_dir < -1 ? -1 : cross_dir));

    // Release the frame to Core 0 writer with cross-core visibility
    s_write_idx_.store(w + 1, std::memory_order_release);
}

bool ThermalRecorder::clipRecordingNow() {
    return s_state_ == RECORDING || s_state_ == COOLDOWN;
}

const char* ThermalRecorder::getCurrentClipId() {
    if (s_clip_file_ && (s_state_ == RECORDING || s_state_ == COOLDOWN)) {
        // Extract basename from path (/sdcard/clips/CLIP_00005.thv → CLIP_00005.thv)
        const char* slash = strrchr(s_clip_path_, '/');
        return slash ? slash + 1 : s_clip_path_;
    }
    return "";
}

// ---------------------------------------------------------------------------
//  Writer Task  (Core 0 — state machine + SD I/O)
// ---------------------------------------------------------------------------
void ThermalRecorder::writerTask(void* pv) {
    (void)pv;
    ESP_LOGI(TAG, "Writer task started");

    while (true) {
        int avail = s_write_idx_.load(std::memory_order_acquire) - s_read_idx_;
        if (avail <= 0) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        int idx = s_read_idx_ % s_N_;
        const FrameSlot& slot = s_ring_buf_[idx];
        s_read_idx_++;

        uint32_t now = slot.timestamp_ms;

        switch (s_state_) {

        // -------- IDLE --------
        case IDLE:
            if (slot.track_count > 0) {
                startClip(now);
            }
            break;

        // -------- RECORDING --------
        case RECORDING:
            if (slot.track_count > 0) {
                s_last_track_ms_ = now;
            }
            if (slot.cross_dir != 0) {
                s_clip_crossings_++;
            }
            s_clip_frame_count_++;

            // Max duration reached?
            if (now - s_clip_start_ms_ >= max_duration_ms) {
                if (slot.track_count > 0) {
                    // Split: close current, start new clip immediately
                    closeClip();
                    startClip(now);
                } else {
                    s_state_ = COOLDOWN;
                }
                break;
            }

            // No tracks → cooldown
            if (slot.track_count == 0) {
                s_state_ = COOLDOWN;
            }
            break;

        // -------- COOLDOWN --------
        case COOLDOWN:
            // Track came back → resume recording
            if (slot.track_count > 0) {
                s_state_ = RECORDING;
                if (slot.cross_dir != 0) {
                    s_clip_crossings_++;
                }
                s_last_track_ms_ = now;
            }
            // Still no tracks — check if cooldown expired
            if (now - s_last_track_ms_ >= cooldown_ms) {
                s_state_ = CLOSING;
            }
            s_clip_frame_count_++;
            break;

        // -------- CLOSING --------
        case CLOSING:
            closeClip();
            break;
        }

        // Write pixel data to SD if recording
        if (s_clip_file_ && (s_state_ == RECORDING || s_state_ == COOLDOWN)) {
            if (fwrite(slot.pixels, 2, TOTAL_PIXELS, s_clip_file_) != TOTAL_PIXELS) {
                ESP_LOGW(TAG, "SD write error in clip — closing early");
                fclose(s_clip_file_);
                s_clip_file_ = nullptr;
                s_state_ = CLOSING;
            }
        }

        // Yield to other tasks between frames
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ---------------------------------------------------------------------------
//  startClip  — open file, reserve header space, write pre-roll frames
// ---------------------------------------------------------------------------
void ThermalRecorder::startClip(uint32_t now_ms) {
    s_clip_counter_++;
    snprintf(s_clip_path_, sizeof(s_clip_path_),
             "/sdcard/clips/CLIP_%05lu.thv", (unsigned long)s_clip_counter_);

    s_clip_file_ = fopen(s_clip_path_, "wb");
    if (!s_clip_file_) {
        ESP_LOGW(TAG, "Cannot create %s: %s", s_clip_path_, strerror(errno));
        s_clip_path_[0] = '\0';
        return;
    }

    // Reserve 16 bytes for the .thv header (written at closeClip)
    ThvHeader tmp;
    memset(&tmp, 0, sizeof(tmp));
    fwrite(&tmp, sizeof(tmp), 1, s_clip_file_);

    // Write pre-roll frames directly from ring buffer (before the trigger frame)
    // The trigger frame is at position s_read_idx_ - 1
    int trigger_idx = s_read_idx_ - 1;
    int max_pre = (pre_roll_ms * 32 / 1000);
    if (max_pre > s_N_ - 4) max_pre = s_N_ - 4;

    int pre_count = 0;
    int pre_start = trigger_idx - max_pre;
    if (pre_start < 0) pre_start = 0;
    for (int i = pre_start; i < trigger_idx; i++) {
        const FrameSlot& pre = s_ring_buf_[i % s_N_];
        if (fwrite(pre.pixels, 2, TOTAL_PIXELS, s_clip_file_) == TOTAL_PIXELS) {
            pre_count++;
        }
    }

    s_clip_start_ms_     = now_ms;
    s_last_track_ms_     = now_ms;
    s_clip_frame_count_  = pre_count;
    s_clip_crossings_    = 0;
    s_state_             = RECORDING;

    ESP_LOGI(TAG, "Clip %s START (%d pre-roll frames)", s_clip_path_, pre_count);
}

// ---------------------------------------------------------------------------
//  closeClip  — write .thv header, close file, persist counter
// ---------------------------------------------------------------------------
void ThermalRecorder::closeClip() {
    if (!s_clip_file_) {
        s_state_ = IDLE;
        return;
    }

    uint32_t dur_ms = nowMs() - s_clip_start_ms_;
    bool keep = (dur_ms >= min_duration_ms);

    if (!keep) {
        fclose(s_clip_file_);
        remove(s_clip_path_);
        ESP_LOGI(TAG, "Clip %s DISCARDED (%lu ms, %lu crossings)",
                 s_clip_path_, (unsigned long)dur_ms,
                 (unsigned long)s_clip_crossings_);
    } else {
        // Write .thv header at offset 0 (overwrites the 16-byte reservation)
        fseek(s_clip_file_, 0, SEEK_SET);
        ThvHeader hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.magic       = THV_MAGIC;
        hdr.version     = 1;
        hdr.frame_count = (uint16_t)(s_clip_frame_count_ > 65535 ? 65535 : s_clip_frame_count_);
        hdr.width       = 32;
        hdr.height      = 24;
        hdr.fps         = 32;
        hdr.trigger_dir = (uint8_t)(s_clip_crossings_ > 0 ? 1 : 0);
        fwrite(&hdr, sizeof(hdr), 1, s_clip_file_);
        fclose(s_clip_file_);

        // Persist clip counter
        nvs_handle_t h;
        if (nvs_open("thermal_registry", NVS_READWRITE, &h) == ESP_OK) {
            int32_t v = (int32_t)s_clip_counter_;
            nvs_set_i32(h, "clip_num", v);
            nvs_commit(h);
            nvs_close(h);
        }

        ESP_LOGI(TAG, "Clip %s SAVED (%lu ms, %lu frames, %lu crossings)",
                 s_clip_path_, (unsigned long)dur_ms,
                 (unsigned long)s_clip_frame_count_,
                 (unsigned long)s_clip_crossings_);
    }

    s_clip_file_    = nullptr;
    s_clip_path_[0] = '\0';
    s_state_        = IDLE;
}
