/**
 * @file http_server.cpp
 * @brief HTTP + WebSocket server for the Thermal Door Counter.
 *
 * Revision history:
 *   W1 — Bug fixes: race condition in broadcastFrame, WS_BUFFER_SIZE 4096,
 *         overflow guard in serialization, OTA content_len=0 validation,
 *         static reboot handler, static NVS string buffer, peak_temp_100 in
 *         TrackInfo (types updated in thermal_types.hpp per PLAN-W4).
 *   W3 — Soft-clock: SET_TIME command, getSystemTimeMs(), session_id,
 *         time_quality broadcast in every frame.
 *   W6 — NVS counter persistence every 10 minutes via FreeRTOS timer,
 *         session baseline tracking for correct cross-session accumulation.
 */

#include "http_server.hpp"
#include "cJSON.h"
#include "esp_app_format.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "fov_correction.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "rtc_driver.hpp"
#include "sd_manager.hpp"
#include "thermal_config.hpp"
#include "thermal_recorder.hpp"
#include <inttypes.h> // [NEW] Required for PRIu64 format macro
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>

static const char *TAG = "HTTP_SERVER";
static const char *NVS_NS = "thcfg"; ///< NVS namespace for thermal config

// Access to global hardware instances
extern RTCDriver g_rtc;
extern SDManager g_sd;

// =============================================================================
//  STATIC MEMBER DEFINITIONS
// =============================================================================

httpd_handle_t HttpServer::server_ = NULL;

uint8_t HttpServer::ws_buffers_[WS_BUFFER_COUNT][WS_BUFFER_SIZE]
    __attribute__((aligned(4)));
int HttpServer::ws_buffer_ref_counts_[WS_BUFFER_COUNT] = {0};

// =============================================================================
//  FILE-SCOPE STATE
// =============================================================================

static QueueHandle_t s_configQueue = NULL;
static portMUX_TYPE s_ws_mux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_config_mux = portMUX_INITIALIZER_UNLOCKED;

// --- W3: Soft-clock reference ---
static uint64_t s_time_ref_unix_ms =
    0; ///< Unix timestamp (ms) provided by browser
static int64_t s_time_ref_timer_us =
    0; ///< esp_timer_get_time() at the moment SET_TIME was received
static bool s_time_valid = false;
static uint8_t s_time_quality = HttpServer::TIME_QUALITY_NONE;

// --- W3/W6: Session tracking ---
static uint16_t s_session_id = 0;
static int32_t s_session_baseline_in =
    0; ///< NVS total at last boot (never changes during session)
static int32_t s_session_baseline_out = 0;

// --- H1-FIX: Buffer Watchdog ---
static uint32_t s_ws_buffer_acquired_ticks[HttpServer::WS_BUFFER_COUNT] = {0};
static constexpr uint32_t WS_BUFFER_MAX_AGE_MS = 2000; // 2 seconds recovery timeout

// --- W6: Counter mirror updated from broadcastFrame, read by timer ---
// int32_t reads/writes on Xtensa LX7 are atomic for naturally-aligned
// addresses.
static volatile int32_t s_latest_count_in = 0;
static volatile int32_t s_latest_count_out = 0;
static TimerHandle_t s_counter_save_timer = NULL;

// --- W1-6: Static buffer for NVS seg_lines string — avoids 512-byte stack
// frame ---
static char s_lines_buf[512];

// =============================================================================
//  COMPILE-TIME SAFETY: VERIFY WS_BUFFER_SIZE IS LARGE ENOUGH
//
//  Binary frame layout (protocol v2, magic 0x12):
//    [0]      magic        1 byte
//    [1]      sensor_ok    1 byte
//    [2-5]    ambient_temp 4 bytes (float32 LE)
//    [6-7]    count_in     2 bytes (uint16 LE)
//    [8-9]    count_out    2 bytes (uint16 LE)
//    [10]     num_tracks   1 byte
//    [11-12]  session_id   2 bytes (uint16 LE)          — NEW W3
//    [13]     time_quality 1 byte                       — NEW W3
//    [14+]    per track: id(1)+x(2)+y(2)+vx(2)+vy(2)+peak_temp(2) = 11 bytes  —
//    peak_temp NEW W4 [14 + MAX_TRACKS*11] pixels: TOTAL_PIXELS * 2 bytes
// =============================================================================

static_assert(1u /*magic*/ + 1u /*sensor_ok*/ + 4u /*ambient*/ + 2u /*cnt_in*/ +
                      2u /*cnt_out*/ + 1u /*num_tracks*/ + 2u /*sess_id*/ +
                      1u /*time_qual*/ +
                      (uint32_t)ThermalConfig::MAX_TRACKS * 11u +
                      (uint32_t)ThermalConfig::TOTAL_PIXELS * 2u <=
                  (uint32_t)HttpServer::WS_BUFFER_SIZE,
              "WS_BUFFER_SIZE is too small for current MAX_TRACKS and "
              "TOTAL_PIXELS — increase it in http_server.hpp");

// =============================================================================
//  EMBEDDED FILE SYMBOLS (from CMake EMBED_FILES)
// =============================================================================

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");

// =============================================================================
//  W3: SOFT-CLOCK HELPERS
// =============================================================================

/**
 * @brief Current Unix time in milliseconds.
 * @return 0 if no time reference has been set (browser hasn't sent SET_TIME
 * yet).
 */
static uint64_t __attribute__((unused)) getSystemTimeMs() {
  if (!s_time_valid)
    return 0ULL;
  int64_t elapsed_us = esp_timer_get_time() - s_time_ref_timer_us;
  // Guard against negative elapsed if timer wraps (theoretically ~292 years)
  if (elapsed_us < 0)
    elapsed_us = 0;
  return s_time_ref_unix_ms + (uint64_t)(elapsed_us / 1000LL);
}

static void getLogPathForToday(char *buf, size_t n) {
  uint64_t ms = getSystemTimeMs();
  if (ms == 0) {
    snprintf(buf, n, "logs/counts_session_%03u.csv", s_session_id);
  } else {
    time_t t = (time_t)(ms / 1000);
    struct tm ti;
    gmtime_r(&t, &ti);
    snprintf(buf, n, "logs/counts_%04d-%02d-%02d.csv", ti.tm_year + 1900,
             ti.tm_mon + 1, ti.tm_mday);
  }
}

// =============================================================================
//  W6: COUNTER PERSISTENCE HELPERS
// =============================================================================

/**
 * @brief Persist the true cumulative in/out totals to NVS.
 *        True total = session_baseline + session_count_so_far.
 *        Safe to call from any task.
 */
static void saveCountersToNvs(int32_t session_count_in,
                              int32_t session_count_out) {
  int32_t total_in = s_session_baseline_in + session_count_in;
  int32_t total_out = s_session_baseline_out + session_count_out;

  nvs_handle_t h;
  if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
    ESP_LOGW(TAG, "NVS: cannot open for counter save");
    return;
  }
  esp_err_t e1 = nvs_set_i32(h, "nvs_base_in", total_in);
  esp_err_t e2 = nvs_set_i32(h, "nvs_base_out", total_out);
  if (e1 != ESP_OK || e2 != ESP_OK) {
    ESP_LOGW(TAG, "NVS: set failed (in=%s, out=%s)",
             esp_err_to_name(e1), esp_err_to_name(e2));
    nvs_close(h);
    return;
  }
  esp_err_t err = nvs_commit(h);
  nvs_close(h);

  if (err == ESP_OK) {
    ESP_LOGI(TAG,
             "NVS: counters saved — session(in=%d out=%d) total(in=%d out=%d)",
             session_count_in, session_count_out, total_in, total_out);
  } else {
    ESP_LOGW(TAG, "NVS: counter commit failed: %s", esp_err_to_name(err));
  }
}

/**
 * @brief FreeRTOS software timer callback — fires every 10 minutes.
 *        Runs from the timerTask (Core 0). Reads volatile counter mirror
 *        and calls saveCountersToNvs().
 */
static void counterSaveTimerCb(TimerHandle_t xTimer) {
  (void)xTimer;
  // int32_t read from Xtensa LX7 aligned volatile is effectively atomic
  int32_t ci = s_latest_count_in;
  int32_t co = s_latest_count_out;

  static int32_t last_saved_in = -1;
  static int32_t last_saved_out = -1;

  if (ci != last_saved_in || co != last_saved_out) {
    saveCountersToNvs(ci, co);
    last_saved_in = ci;
    last_saved_out = co;
  }
}

// =============================================================================
//  NVS CONFIG HELPERS
// =============================================================================

/**
 * @brief Load all persisted configuration from NVS into ThermalConfig globals.
 *        Also loads counter baselines and session_id (W6/W3).
 *        Called once from HttpServer::start() before the server accepts
 * connections.
 */
static void loadConfigFromNvs(void) {
  nvs_handle_t h;
  esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGI(TAG, "NVS: no saved config — using defaults");
    return;
  }
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "NVS: open failed (%s)", esp_err_to_name(err));
    return;
  }

  auto readFloat = [&](const char *key, float &dest) {
    int32_t raw = 0;
    if (nvs_get_i32(h, key, &raw) == ESP_OK)
      dest = (float)raw / 1000.0f;
  };
  auto readInt = [&](const char *key, int &dest) {
    int32_t raw = 0;
    if (nvs_get_i32(h, key, &raw) == ESP_OK)
      dest = (int)raw;
  };
  auto readInt32 = [&](const char *key, int32_t &dest) {
    nvs_get_i32(h, key, &dest);
  };

  readFloat("temp_bio", ThermalConfig::BIOLOGICAL_TEMP_MIN);
  readFloat("delta_t", ThermalConfig::BACKGROUND_DELTA_T);
  readFloat("alpha_ema", ThermalConfig::EMA_ALPHA);
  readInt("line_entry", ThermalConfig::DEFAULT_LINE_ENTRY_Y);
  readInt("line_exit", ThermalConfig::DEFAULT_LINE_EXIT_Y);
  readInt("dead_left", ThermalConfig::DEFAULT_DEAD_ZONE_LEFT);
  readInt("dead_right", ThermalConfig::DEFAULT_DEAD_ZONE_RIGHT);
  readFloat("sensor_h", ThermalConfig::SENSOR_HEIGHT_M);
  readFloat("person_d", ThermalConfig::PERSON_DIAMETER_M);
  readInt("view_mode", ThermalConfig::VIEW_MODE);

  // W6: Load cumulative counter baselines
  readInt32("nvs_base_in", s_session_baseline_in);
  readInt32("nvs_base_out", s_session_baseline_out);

  // Load counting line segments using static buffer (W1-6: no stack allocation)
  size_t lines_len = sizeof(s_lines_buf);
  if (nvs_get_str(h, "seg_lines", s_lines_buf, &lines_len) == ESP_OK) {
    cJSON *lines_root = cJSON_Parse(s_lines_buf);
    if (lines_root && cJSON_IsArray(lines_root)) {
      int n = cJSON_GetArraySize(lines_root);
      if (n > MAX_COUNTING_LINES)
        n = MAX_COUNTING_LINES;
      ThermalConfig::door_lines.num_lines = 0;
      for (int i = 0; i < n; i++) {
        cJSON *l = cJSON_GetArrayItem(lines_root, i);
        if (!cJSON_IsObject(l))
          continue;
        CountingSegment &s = ThermalConfig::door_lines.lines[i];
        cJSON *jx1 = cJSON_GetObjectItem(l, "x1");
        cJSON *jy1 = cJSON_GetObjectItem(l, "y1");
        cJSON *jx2 = cJSON_GetObjectItem(l, "x2");
        cJSON *jy2 = cJSON_GetObjectItem(l, "y2");
        s.x1 = cJSON_IsNumber(jx1) ? (float)jx1->valuedouble : 0.f;
        s.y1 = cJSON_IsNumber(jy1) ? (float)jy1->valuedouble : 0.f;
        s.x2 = cJSON_IsNumber(jx2) ? (float)jx2->valuedouble : 0.f;
        s.y2 = cJSON_IsNumber(jy2) ? (float)jy2->valuedouble : 0.f;
        s.id = (uint8_t)(i + 1);
        s.enabled = true;
        snprintf(s.name, sizeof(s.name), "Linea %d", i + 1);
        ThermalConfig::door_lines.num_lines++;
      }
    }
    if (lines_root)
      cJSON_Delete(lines_root);
  }

  int8_t use_seg = 0;
  if (nvs_get_i8(h, "use_segments", &use_seg) == ESP_OK) {
    ThermalConfig::door_lines.use_segments = (use_seg != 0);
  }

  nvs_close(h);
  ESP_LOGI(
      TAG,
      "NVS: loaded — temp_bio=%.1f dt=%.1f alpha=%.2f baseline(in=%d out=%d)",
      ThermalConfig::BIOLOGICAL_TEMP_MIN, ThermalConfig::BACKGROUND_DELTA_T,
      ThermalConfig::EMA_ALPHA, s_session_baseline_in, s_session_baseline_out);
}

/**
 * @brief Persist algorithm parameters and line segments to NVS.
 *        Does NOT save counters (those are saved by the timer — W6).
 */
static esp_err_t saveConfigToNvs(void) {
  nvs_handle_t h;
  esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS: open for write failed (%s)", esp_err_to_name(err));
    return err;
  }

  nvs_set_i32(h, "temp_bio",
              (int32_t)(ThermalConfig::BIOLOGICAL_TEMP_MIN * 1000.0f));
  nvs_set_i32(h, "delta_t",
              (int32_t)(ThermalConfig::BACKGROUND_DELTA_T * 1000.0f));
  nvs_set_i32(h, "alpha_ema", (int32_t)(ThermalConfig::EMA_ALPHA * 1000.0f));
  nvs_set_i32(h, "line_entry", (int32_t)ThermalConfig::DEFAULT_LINE_ENTRY_Y);
  nvs_set_i32(h, "line_exit", (int32_t)ThermalConfig::DEFAULT_LINE_EXIT_Y);
  nvs_set_i32(h, "dead_left", (int32_t)ThermalConfig::DEFAULT_DEAD_ZONE_LEFT);
  nvs_set_i32(h, "dead_right", (int32_t)ThermalConfig::DEFAULT_DEAD_ZONE_RIGHT);
  nvs_set_i32(h, "sensor_h",
              (int32_t)(ThermalConfig::SENSOR_HEIGHT_M * 1000.0f));
  nvs_set_i32(h, "person_d",
              (int32_t)(ThermalConfig::PERSON_DIAMETER_M * 1000.0f));
  nvs_set_i32(h, "view_mode", (int32_t)ThermalConfig::VIEW_MODE);

  // Serialize counting line segments to JSON string
  // C3-FIX: Take snapshot under spinlock, then read from it
  ThermalConfig::DoorLineConfig dl_snap;
  portENTER_CRITICAL(&ThermalConfig::door_lines_mux);
  dl_snap = ThermalConfig::door_lines;
  portEXIT_CRITICAL(&ThermalConfig::door_lines_mux);

  cJSON *lines_arr = cJSON_CreateArray();
  if (lines_arr) {
    for (int i = 0; i < dl_snap.num_lines; i++) {
      const CountingSegment &s = dl_snap.lines[i];
      cJSON *l = cJSON_CreateObject();
      if (l) {
        cJSON_AddNumberToObject(l, "x1", s.x1);
        cJSON_AddNumberToObject(l, "y1", s.y1);
        cJSON_AddNumberToObject(l, "x2", s.x2);
        cJSON_AddNumberToObject(l, "y2", s.y2);
        cJSON_AddItemToArray(lines_arr, l);
      }
    }
    char *str_lines = cJSON_PrintUnformatted(lines_arr);
    if (str_lines) {
      nvs_set_str(h, "seg_lines", str_lines);
      free(str_lines);
    }
    cJSON_Delete(lines_arr);
  }
  nvs_set_i8(h, "use_segments", dl_snap.use_segments ? 1 : 0);

  err = nvs_commit(h);
  nvs_close(h);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "NVS: config saved");
  } else {
    ESP_LOGE(TAG, "NVS: commit failed (%s)", esp_err_to_name(err));
  }
  return err;
}

// =============================================================================
//  WEBSOCKET JSON HELPER
// =============================================================================

static void wsSendJson(httpd_req_t *req, cJSON *root) {
  char *str = cJSON_PrintUnformatted(root);
  if (!str)
    return;
  httpd_ws_frame_t pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = HTTPD_WS_TYPE_TEXT;
  pkt.payload = (uint8_t *)str;
  pkt.len = strlen(str);
  httpd_ws_send_frame(req, &pkt);
  free(str);
}

// =============================================================================
//  HTTP HANDLERS — STATIC FILES
// =============================================================================

esp_err_t HttpServer::indexGetHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control",
                     "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, (const char *)index_html_start,
                         index_html_end - index_html_start);
}

esp_err_t HttpServer::styleGetHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/css");
  httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");
  return httpd_resp_send(req, (const char *)style_css_start,
                         style_css_end - style_css_start);
}

esp_err_t HttpServer::appJsGetHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "application/javascript");
  httpd_resp_set_hdr(req, "Cache-Control",
                     "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, (const char *)app_js_start,
                         app_js_end - app_js_start);
}

// =============================================================================
//  HTTP HANDLER — OTA (/update)
// =============================================================================

esp_err_t HttpServer::otaPostHandler(httpd_req_t *req) {
  // W1-4: Reject upload if content_len is 0 or suspiciously small
  if (req->content_len < 256) {
    ESP_LOGW(TAG, "OTA: rejected empty/tiny upload (len=%d)", req->content_len);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                        "Empty or invalid firmware");
    return ESP_ERR_INVALID_SIZE;
  }

  const esp_partition_t *update_partition =
      esp_ota_get_next_update_partition(NULL);
  if (!update_partition) {
    ESP_LOGE(TAG, "OTA: update partition not found");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "OTA Partition Not Found");
    return ESP_FAIL;
  }

  esp_ota_handle_t ota_handle = 0;
  esp_err_t err =
      esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA: begin failed (%s)", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "OTA Begin Failed");
    return err;
  }

  int remaining = req->content_len;
  char buf[1024];
  while (remaining > 0) {
    int recv_len = httpd_req_recv(req, buf, MIN(remaining, (int)sizeof(buf)));
    if (recv_len <= 0) {
      if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
        continue;
      ESP_LOGE(TAG, "OTA: recv error");
      esp_ota_abort(ota_handle);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                          "OTA Receive Failed");
      return ESP_FAIL;
    }
    err = esp_ota_write(ota_handle, buf, recv_len);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "OTA: write failed (%s)", esp_err_to_name(err));
      esp_ota_abort(ota_handle);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                          "OTA Write Failed");
      return err;
    }
    remaining -= recv_len;
  }

  err = esp_ota_end(ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA: end failed (%s)", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "OTA Validation Failed");
    return err;
  }

  err = esp_ota_set_boot_partition(update_partition);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA: set boot partition failed (%s)", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "OTA Set Boot Failed");
    return err;
  }

  ESP_LOGI(TAG, "OTA: success — rebooting in 1s");
  httpd_resp_sendstr(req, "OTA OK — rebooting");
  vTaskDelay(pdMS_TO_TICKS(1000));
  esp_restart();
  return ESP_OK;
}

// =============================================================================
//  HTTP HANDLER — REBOOT (/reboot)  W1-5: static function, not lambda
// =============================================================================

esp_err_t HttpServer::rebootPostHandler(httpd_req_t *req) {
  // Save latest counters to NVS before restarting so no counts are lost
  saveCountersToNvs(s_latest_count_in, s_latest_count_out);
  httpd_resp_sendstr(req, "Rebooting...");
  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
  return ESP_OK;
}

// =============================================================================
//  HTTP HANDLER — SAVE NVS NOW (/save_nvs)
// =============================================================================

esp_err_t HttpServer::saveNvsHandler(httpd_req_t *req) {
  int32_t ci = s_latest_count_in;
  int32_t co = s_latest_count_out;
  saveCountersToNvs(ci, co);
  saveConfigToNvs();

  // Build JSON response
  char resp[128];
  int32_t total_in = s_session_baseline_in + ci;
  int32_t total_out = s_session_baseline_out + co;
  snprintf(resp, sizeof(resp),
           "{\"ok\":true,\"saved_in\":%ld,\"saved_out\":%ld}", (long)total_in,
            (long)total_out);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, resp);
  return ESP_OK;
}

// =============================================================================
//  WEBSOCKET — INCOMING MESSAGE HANDLER
// =============================================================================

void HttpServer::handleWebSocketMessage(httpd_req_t *req,
                                        httpd_ws_frame_t *ws_pkt) {
  if (ws_pkt->type != HTTPD_WS_TYPE_TEXT) {
    ESP_LOGW(TAG, "WS: non-text frame (type=%u len=%zu) dropped",
             ws_pkt->type, ws_pkt->len);
    return;
  }

  cJSON *root = cJSON_Parse((const char *)ws_pkt->payload);
  if (!root) {
    ESP_LOGE(TAG, "WS: invalid JSON");
    return;
  }

  cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
  if (!cJSON_IsString(cmd)) {
    cJSON_Delete(root);
    return;
  }
  const char *cmdStr = cmd->valuestring;

  // -------------------------------------------------------------------------
  //  GET_CONFIG — Returns algorithm config + session info + persistent counters
  // -------------------------------------------------------------------------
  if (strcmp(cmdStr, "GET_CONFIG") == 0) {
    float snap_temp_bio, snap_delta_t, snap_alpha_ema, snap_sensor_h,
        snap_person_d;
    int snap_entry, snap_exit, snap_dead_l, snap_dead_r, snap_view;

    portENTER_CRITICAL(&s_config_mux);
    snap_temp_bio = ThermalConfig::BIOLOGICAL_TEMP_MIN;
    snap_delta_t = ThermalConfig::BACKGROUND_DELTA_T;
    snap_alpha_ema = ThermalConfig::EMA_ALPHA;
    snap_entry = ThermalConfig::DEFAULT_LINE_ENTRY_Y;
    snap_exit = ThermalConfig::DEFAULT_LINE_EXIT_Y;
    snap_dead_l = ThermalConfig::DEFAULT_DEAD_ZONE_LEFT;
    snap_dead_r = ThermalConfig::DEFAULT_DEAD_ZONE_RIGHT;
    snap_sensor_h = ThermalConfig::SENSOR_HEIGHT_M;
    snap_person_d = ThermalConfig::PERSON_DIAMETER_M;
    snap_view = ThermalConfig::VIEW_MODE;
    portEXIT_CRITICAL(&s_config_mux);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", "config");
    cJSON_AddNumberToObject(resp, "temp_bio", (double)snap_temp_bio);
    cJSON_AddNumberToObject(resp, "delta_t", (double)snap_delta_t);
    cJSON_AddNumberToObject(resp, "alpha_ema", (double)snap_alpha_ema);
    cJSON_AddNumberToObject(resp, "line_entry", (double)snap_entry);
    cJSON_AddNumberToObject(resp, "line_exit", (double)snap_exit);
    cJSON_AddNumberToObject(resp, "dead_left", (double)snap_dead_l);
    cJSON_AddNumberToObject(resp, "dead_right", (double)snap_dead_r);
    cJSON_AddNumberToObject(resp, "sensor_height", (double)snap_sensor_h);
    cJSON_AddNumberToObject(resp, "person_diameter", (double)snap_person_d);
    cJSON_AddNumberToObject(resp, "view_mode", (double)snap_view);

    // W3/W6: Session and persistence info
    cJSON_AddNumberToObject(resp, "session_id", (double)s_session_id);
    cJSON_AddNumberToObject(resp, "time_quality", (double)s_time_quality);
    cJSON_AddNumberToObject(resp, "nvs_base_in", (double)s_session_baseline_in);
    cJSON_AddNumberToObject(resp, "nvs_base_out",
                            (double)s_session_baseline_out);

    // W-C1: Hardware health indicators for boot sync
    cJSON_AddBoolToObject(resp, "rtc_ok", g_rtc.isAvailable());
    cJSON_AddBoolToObject(resp, "sd_ok", g_sd.isMounted());
    if (g_rtc.isAvailable()) {
        RTCDriver::DateTime dt;
        if (g_rtc.getTime(dt) == ESP_OK) {
            char iso[32];
            dt.toISO(iso, sizeof(iso));
            cJSON_AddStringToObject(resp, "rtc_time", iso);
        }
    }

    // Counting line segments
    ThermalConfig::DoorLineConfig dl_snap;
    portENTER_CRITICAL(&ThermalConfig::door_lines_mux);
    dl_snap = ThermalConfig::door_lines;
    portEXIT_CRITICAL(&ThermalConfig::door_lines_mux);

    cJSON_AddBoolToObject(resp, "use_segments", dl_snap.use_segments);
    cJSON *lines_arr = cJSON_CreateArray();
    for (int i = 0; i < dl_snap.num_lines; i++) {
      const CountingSegment &s = dl_snap.lines[i];
      cJSON *l = cJSON_CreateObject();
      cJSON_AddNumberToObject(l, "x1", s.x1);
      cJSON_AddNumberToObject(l, "y1", s.y1);
      cJSON_AddNumberToObject(l, "x2", s.x2);
      cJSON_AddNumberToObject(l, "y2", s.y2);
      cJSON_AddNumberToObject(l, "id", s.id);
      cJSON_AddStringToObject(l, "name", s.name);
      cJSON_AddItemToArray(lines_arr, l);
    }
    cJSON_AddItemToObject(resp, "lines", lines_arr);

    wsSendJson(req, resp);
    cJSON_Delete(resp);
  }

  // -------------------------------------------------------------------------
  //  GET_STATUS — Lightweight status ping (RTC, clock, session) — W8 stub
  // -------------------------------------------------------------------------
  else if (strcmp(cmdStr, "GET_STATUS") == 0) {
    uint64_t uptime_ms = (uint64_t)(esp_timer_get_time() / 1000LL);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", "status");
    cJSON_AddBoolToObject(resp, "rtc_ok", g_rtc.isAvailable());
    cJSON_AddBoolToObject(resp, "sd_ok", g_sd.isMounted());
    cJSON_AddNumberToObject(resp, "session_id", (double)s_session_id);
    cJSON_AddNumberToObject(resp, "time_quality", (double)s_time_quality);
    cJSON_AddNumberToObject(resp, "nvs_base_in", (double)s_session_baseline_in);
    cJSON_AddNumberToObject(resp, "nvs_base_out",
                            (double)s_session_baseline_out);
    cJSON_AddNumberToObject(resp, "uptime_ms", (double)uptime_ms);

    if (g_rtc.isAvailable()) {
      RTCDriver::DateTime dt;
      if (g_rtc.getTime(dt) == ESP_OK) {
        char iso[32];
        dt.toISO(iso, sizeof(iso));
        cJSON_AddStringToObject(resp, "rtc_time", iso);
      }
    }

    wsSendJson(req, resp);
    cJSON_Delete(resp);
  }

  // -------------------------------------------------------------------------
  //  SET_TIME — Browser provides current Unix time for soft-clock (W3)
  // -------------------------------------------------------------------------
  else if (strcmp(cmdStr, "SET_TIME") == 0) {
    cJSON *unix_ms_j = cJSON_GetObjectItem(root, "unix_ms");
    if (cJSON_IsNumber(unix_ms_j) && unix_ms_j->valuedouble > 1000000000000.0) {
      s_time_ref_unix_ms = (uint64_t)unix_ms_j->valuedouble;
      s_time_ref_timer_us = esp_timer_get_time();
      s_time_valid = true;
      s_time_quality = TIME_QUALITY_BROWSER;
      ESP_LOGI(TAG, "Soft-clock: unix_ms=%" PRIu64, s_time_ref_unix_ms);

      // Persist the reference time so a GET_STATUS can confirm sync
      nvs_handle_t h;
      if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        // Store unix_ms as two int32 (high 32 bits and low 32 bits)
        nvs_set_i32(h, "tref_hi", (int32_t)(s_time_ref_unix_ms >> 32));
        nvs_set_i32(h, "tref_lo",
                    (int32_t)(s_time_ref_unix_ms & 0xFFFFFFFFULL));
        nvs_commit(h);
        nvs_close(h);
      }

      // Update hardware RTC if available
      if (g_rtc.isAvailable()) {
        RTCDriver::DateTime dt = RTCDriver::DateTime::fromUnix(
            (uint32_t)(s_time_ref_unix_ms / 1000));
        if (g_rtc.setTime(dt) == ESP_OK) {
          ESP_LOGI(TAG, "Hardware RTC synchronized with browser time");
        }
      }
    } else {
      ESP_LOGW(TAG, "SET_TIME: invalid or missing unix_ms");
    }
  }

  // -------------------------------------------------------------------------
  //  SET_PARAM — Applies a single algorithm parameter
  // -------------------------------------------------------------------------
  else if (strcmp(cmdStr, "SET_PARAM") == 0) {
    cJSON *param = cJSON_GetObjectItem(root, "param");
    cJSON *val = cJSON_GetObjectItem(root, "val");
    if (cJSON_IsString(param) && cJSON_IsNumber(val)) {
      struct ParamRange {
        const char *name;
        ConfigCmdType type;
        float min_val;
        float max_val;
      };
      static const ParamRange kParams[] = {
          {"temp_bio", ConfigCmdType::SET_TEMP_BIO, 15.0f, 45.0f},
          {"delta_t", ConfigCmdType::SET_DELTA_T, 0.3f, 10.0f},
          {"alpha_ema", ConfigCmdType::SET_EMA_ALPHA, 0.01f, 0.5f},
          {"line_entry", ConfigCmdType::SET_LINE_ENTRY, 0.0f, 24.0f},
          {"line_exit", ConfigCmdType::SET_LINE_EXIT, 0.0f, 24.0f},
          {"dead_left", ConfigCmdType::SET_DEAD_LEFT, 0.0f, 32.0f},
          {"dead_right", ConfigCmdType::SET_DEAD_RIGHT, 0.0f, 32.0f},
          {"sensor_height", ConfigCmdType::SET_SENSOR_HEIGHT, 0.5f, 10.0f},
          {"person_diameter", ConfigCmdType::SET_PERSON_DIAMETER, 0.2f, 2.0f},
          {"view_mode", ConfigCmdType::SET_VIEW_MODE, 0.0f, 4.0f},
      };
      AppConfigCmd cfgCmd;
      bool matched = false;
      for (const auto &p : kParams) {
        if (strcmp(param->valuestring, p.name) == 0) {
          float v = (float)val->valuedouble;
          if (v < p.min_val || v > p.max_val) {
            ESP_LOGW(TAG, "SET_PARAM: %s=%.3f out of range [%.2f,%.2f]", p.name,
                     v, p.min_val, p.max_val);
            cJSON_Delete(root);
            return;
          }
          cfgCmd.type = p.type;
          cfgCmd.value = v;
          matched = true;
          break;
        }
      }
      if (matched && s_configQueue) {
        if (xQueueSend(s_configQueue, &cfgCmd, pdMS_TO_TICKS(10)) == pdTRUE) {
          ESP_LOGI(TAG, "SET_PARAM: %s = %.3f", param->valuestring,
                   (double)cfgCmd.value);
        } else {
          ESP_LOGW(TAG, "SET_PARAM: Queue full, command %s dropped",
                   param->valuestring);
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  //  SET_COUNTING_LINES — Applies segment line configuration
  // -------------------------------------------------------------------------
  else if (strcmp(cmdStr, "SET_COUNTING_LINES") == 0) {
    cJSON *lines_arr = cJSON_GetObjectItem(root, "lines");
    if (cJSON_IsArray(lines_arr)) {
      int n = cJSON_GetArraySize(lines_arr);
      if (n > MAX_COUNTING_LINES)
        n = MAX_COUNTING_LINES;

      portENTER_CRITICAL(&ThermalConfig::door_lines_mux);
      ThermalConfig::door_lines.num_lines = 0;
      for (int i = 0; i < n; i++) {
        cJSON *line = cJSON_GetArrayItem(lines_arr, i);
        if (!cJSON_IsObject(line))
          continue;
        CountingSegment &seg = ThermalConfig::door_lines.lines[i];
        cJSON *x1 = cJSON_GetObjectItem(line, "x1");
        cJSON *y1 = cJSON_GetObjectItem(line, "y1");
        cJSON *x2 = cJSON_GetObjectItem(line, "x2");
        cJSON *y2 = cJSON_GetObjectItem(line, "y2");
        if (cJSON_IsNumber(x1) && cJSON_IsNumber(y1) && cJSON_IsNumber(x2) &&
            cJSON_IsNumber(y2)) {
          seg.x1 = (float)x1->valuedouble;
          seg.y1 = (float)y1->valuedouble;
          seg.x2 = (float)x2->valuedouble;
          seg.y2 = (float)y2->valuedouble;
          seg.enabled = true;
          seg.id = (uint8_t)(i + 1);
          snprintf(seg.name, sizeof(seg.name), "Linea %d", i + 1);
          ThermalConfig::door_lines.num_lines++;
        }
      }
      ThermalConfig::door_lines.use_segments =
          (ThermalConfig::door_lines.num_lines > 0);
      portEXIT_CRITICAL(&ThermalConfig::door_lines_mux);

      if (s_configQueue) {
        AppConfigCmd cfgCmd{ConfigCmdType::APPLY_CONFIG, 0.0f};
        if (xQueueSend(s_configQueue, &cfgCmd, pdMS_TO_TICKS(10)) != pdTRUE) {
          ESP_LOGW(TAG,
                   "SET_COUNTING_LINES: Queue full, apply command dropped");
        }
      }
      ESP_LOGI(TAG, "SET_COUNTING_LINES: %d segments",
               ThermalConfig::door_lines.num_lines);
      saveConfigToNvs(); // M3-FIX: auto-persist lines to NVS
    }
  }

  // -------------------------------------------------------------------------
  //  SAVE_CONFIG — Persists algorithm parameters to NVS
  // -------------------------------------------------------------------------
  else if (strcmp(cmdStr, "SAVE_CONFIG") == 0) {
    esp_err_t err = saveConfigToNvs();
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", "config_saved");
    cJSON_AddBoolToObject(resp, "ok", (err == ESP_OK));
    wsSendJson(req, resp);
    cJSON_Delete(resp);
  }

  // -------------------------------------------------------------------------
  //  RESET_COUNTS
  // -------------------------------------------------------------------------
  else if (strcmp(cmdStr, "RESET_COUNTS") == 0) {
    bool reset_nvs = false;
    cJSON *jnvs = cJSON_GetObjectItem(root, "reset_nvs");
    if (cJSON_IsTrue(jnvs))
      reset_nvs = true;

    if (s_configQueue) {
      AppConfigCmd cfgCmd{ConfigCmdType::RESET_COUNTS, 0.0f};
      if (xQueueSend(s_configQueue, &cfgCmd, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "RESET_COUNTS: Queue full, command dropped");
      }
    }
    s_latest_count_in = 0;
    s_latest_count_out = 0;

    if (reset_nvs) {
      s_session_baseline_in = 0;
      s_session_baseline_out = 0;
      saveCountersToNvs(0, 0); // Fija a 0 en la memoria Flash
      ESP_LOGI(TAG, "RESET_COUNTS: RAM and NVS baselines wiped to 0");
    } else {
      ESP_LOGI(TAG, "RESET_COUNTS: RAM wiped (NVS baselines preserved)");
    }

    // M6-FIX: Confirm reset to the requesting client
    cJSON *reset_resp = cJSON_CreateObject();
    if (reset_resp) {
      cJSON_AddStringToObject(reset_resp, "type", "counts_reset");
      cJSON_AddBoolToObject(reset_resp, "ok", true);
      wsSendJson(req, reset_resp);
      cJSON_Delete(reset_resp);
    }
  }

  // -------------------------------------------------------------------------
  //  RETRY_SENSOR
  // -------------------------------------------------------------------------
  else if (strcmp(cmdStr, "RETRY_SENSOR") == 0) {
    if (s_configQueue) {
      AppConfigCmd cfgCmd{ConfigCmdType::RETRY_SENSOR, 0.0f};
      if (xQueueSend(s_configQueue, &cfgCmd, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "RETRY_SENSOR: Queue full, command dropped");
      }
    }
    ESP_LOGI(TAG, "RETRY_SENSOR requested");
  } else if (strcmp(cmdStr, "RETRY_RTC") == 0) {
    g_rtc.init((gpio_num_t)ThermalConfig::I2C1_SDA_PIN,
               (gpio_num_t)ThermalConfig::I2C1_SCL_PIN);
    ESP_LOGI(TAG, "RETRY_RTC requested");
  } else if (strcmp(cmdStr, "RETRY_SD") == 0) {
    g_sd.init((gpio_num_t)ThermalConfig::SD_MOSI_PIN,
              (gpio_num_t)ThermalConfig::SD_MISO_PIN,
              (gpio_num_t)ThermalConfig::SD_SCK_PIN,
              (gpio_num_t)ThermalConfig::SD_CS_PIN);
    ESP_LOGI(TAG, "RETRY_SD requested");

  // -------------------------------------------------------------------------
  //  ERASE_NVS — Full NVS wipe + reboot
  // -------------------------------------------------------------------------
  } else if (strcmp(cmdStr, "ERASE_NVS") == 0) {
    ESP_LOGW(TAG, "ERASE_NVS: wiping NVS and rebooting...");

    // Save counters one last time before erasing
    saveCountersToNvs(s_latest_count_in, s_latest_count_out);

    esp_err_t err = nvs_flash_erase();
    if (err == ESP_OK) {
      ESP_LOGI(TAG, "NVS erased successfully — restarting");
    } else {
      ESP_LOGE(TAG, "NVS erase failed: %s", esp_err_to_name(err));
    }

    cJSON *resp = cJSON_CreateObject();
    if (resp) {
      cJSON_AddStringToObject(resp, "type", "nvs_erased");
      cJSON_AddBoolToObject(resp, "ok", err == ESP_OK);
      wsSendJson(req, resp);
      cJSON_Delete(resp);
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
  }

  cJSON_Delete(root);
}

// =============================================================================
//  WEBSOCKET URI HANDLER
// =============================================================================

// C1-FIX: Static buffer for WS receives — avoids heap allocation per message
static uint8_t s_ws_rx_buf[1024];

esp_err_t HttpServer::wsHandler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "New WebSocket connection from fd=%d",
             httpd_req_to_sockfd(req));
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(ws_pkt));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;

  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK)
    return ret;

  if (ws_pkt.len == 0)
    return ESP_OK;

  // W1 / P06: Limit frame size to prevent heap exhaustion from malformed
  // clients
  if (ws_pkt.len > 1024) {
    ESP_LOGW(TAG, "WS frame too large (%zu bytes) — rejected", ws_pkt.len);
    return ESP_ERR_INVALID_SIZE;
  }

  // C1-FIX: Use static buffer. Frame size limited to 1024 above.
  memset(s_ws_rx_buf, 0, sizeof(s_ws_rx_buf));
  ws_pkt.payload = s_ws_rx_buf;

  ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
  if (ret == ESP_OK) {
    handleWebSocketMessage(req, &ws_pkt);
  }
  return ret;
}

// =============================================================================
//  START / STOP
// =============================================================================

esp_err_t HttpServer::start(QueueHandle_t configQueue) {
  if (server_ != NULL)
    return ESP_ERR_INVALID_STATE;

  s_configQueue = configQueue;

  // Load config from NVS (also sets s_session_baseline_in/out)
  loadConfigFromNvs();

  // W3: Increment and persist session_id
  {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
      int32_t sid = 0;
      nvs_get_i32(h, "session_id", &sid);
      sid = (sid >= 65535) ? 1 : sid + 1;
      s_session_id = (uint16_t)sid;
      nvs_set_i32(h, "session_id", sid);
      nvs_commit(h);
      nvs_close(h);
    }
    ESP_LOGI(TAG, "Session ID: %u", s_session_id);
  }

  // Reinitialize FOV LUT with the height loaded from NVS (Bug-01 fix)
  FovCorrection::init(ThermalConfig::SENSOR_HEIGHT_M);

  // Start HTTP server on Core 0
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.core_id = 0;
  config.max_uri_handlers = 16;
  config.max_open_sockets = 7;
  config.stack_size = 16384;

  esp_err_t ret = httpd_start(&server_, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // Register URI handlers
  const httpd_uri_t uris[] = {
      {"/", HTTP_GET, indexGetHandler, NULL, false, false, NULL},
      {"/style.css", HTTP_GET, styleGetHandler, NULL, false, false, NULL},
      {"/app.js", HTTP_GET, appJsGetHandler, NULL, false, false, NULL},
      {"/update", HTTP_POST, otaPostHandler, NULL, false, false, NULL},
      {"/reboot", HTTP_POST, rebootPostHandler, NULL, false, false, NULL},
      {"/save_nvs", HTTP_POST, saveNvsHandler, NULL, false, false, NULL},
  };
  for (const auto &u : uris)
    httpd_register_uri_handler(server_, &u);

  const httpd_uri_t ws_uri = {"/ws",
                              HTTP_GET,
                              wsHandler,
                              NULL,
                              /*.is_websocket=*/true,
                              /*.handle_ws_control_frames=*/false,
                              /*.supported_subprotocol=*/NULL};
  httpd_register_uri_handler(server_, &ws_uri);

  const httpd_uri_t log_uri = {
      "/download_log", HTTP_GET, downloadLogHandler, NULL, false, false, NULL};
  httpd_register_uri_handler(server_, &log_uri);

  const httpd_uri_t sd_info_uri = {
      "/api/sd/info", HTTP_GET, sdInfoHandler, NULL, false, false, NULL};
  httpd_register_uri_handler(server_, &sd_info_uri);

  const httpd_uri_t sd_down_uri = {"/api/sd/download",
                                   HTTP_GET,
                                   sdDownloadHandler,
                                   NULL,
                                   false,
                                   false,
                                   NULL};
  httpd_register_uri_handler(server_, &sd_down_uri);

  const httpd_uri_t sd_del_uri = {
      "/api/sd/delete", HTTP_POST, sdDeleteHandler, NULL, false, false, NULL};
  httpd_register_uri_handler(server_, &sd_del_uri);

  const httpd_uri_t sd_erase_all_uri = {
      "/api/sd/erase_all", HTTP_POST, sdEraseAllHandler, NULL, false, false, NULL};
  httpd_register_uri_handler(server_, &sd_erase_all_uri);

  const httpd_uri_t nvs_backup_uri = {
      "/api/nvs/backup", HTTP_GET, nvsBackupHandler, NULL, false, false, NULL};
  httpd_register_uri_handler(server_, &nvs_backup_uri);

  const httpd_uri_t clips_uri = {
      "/api/clips", HTTP_GET, clipListHandler, NULL, false, false, NULL};
  httpd_register_uri_handler(server_, &clips_uri);

  // W6: Start periodic counter save timer (10 minutes)
  s_counter_save_timer =
      xTimerCreate("ctr_save",
                   pdMS_TO_TICKS(10UL * 60UL * 1000UL), // 10 minutes
                   pdTRUE,                              // auto-reload
                   NULL, counterSaveTimerCb);
  if (s_counter_save_timer) {
    xTimerStart(s_counter_save_timer, 0);
    ESP_LOGI(TAG, "Counter save timer started (10-min interval)");
  } else {
    ESP_LOGW(
        TAG,
        "Counter save timer creation failed — counters will NOT be persisted");
  }

  // Register clip event callback from ThermalRecorder
  ThermalRecorder::s_on_clip_event = &HttpServer::onClipEvent;

  ESP_LOGI(TAG, "HTTP server started (session=%u, base_in=%d, base_out=%d)",
           s_session_id, s_session_baseline_in, s_session_baseline_out);
  return ESP_OK;
}

void HttpServer::stop() {
  if (s_counter_save_timer) {
    xTimerStop(s_counter_save_timer, pdMS_TO_TICKS(200));
    xTimerDelete(s_counter_save_timer, pdMS_TO_TICKS(200));
    s_counter_save_timer = NULL;
  }
  if (server_) {
    httpd_stop(server_);
    server_ = NULL;
  }
}

// =============================================================================
//  BROADCAST FRAME  (called from Core 1 at 16 Hz)
// =============================================================================

void HttpServer::broadcastFrame(const ImagePayload &img,
                                const TelemetryPayload &tel, bool sensor_ok) {
  if (!server_)
    return;

  // W6: Update counter mirror for the NVS save timer
  // int32_t write to aligned address is atomic on Xtensa LX7
  s_latest_count_in = (int32_t)tel.count_in;
  s_latest_count_out = (int32_t)tel.count_out;

  // Release buffers that have been stuck for > 2 seconds
  uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
  int stuck_idx = -1;
  uint32_t stuck_time = 0;

  portENTER_CRITICAL(&s_ws_mux);
  for (int i = 0; i < (int)WS_BUFFER_COUNT; i++) {
    if (ws_buffer_ref_counts_[i] > 0 || ws_buffer_ref_counts_[i] == -1) {
      if (now - s_ws_buffer_acquired_ticks[i] > WS_BUFFER_MAX_AGE_MS) {
        ws_buffer_ref_counts_[i] = 0; // Force free
        stuck_idx = i;
        stuck_time = now - s_ws_buffer_acquired_ticks[i];
      }
    }
  }
  portEXIT_CRITICAL(&s_ws_mux);

  if (stuck_idx != -1) {
    ESP_LOGW(TAG, "Buffer %d stuck for %lu ms, forcing release", stuck_idx,
             stuck_time);
  }

  // ---- Find a free buffer ----
  int buf_idx = -1;
  portENTER_CRITICAL(&s_ws_mux);
  for (int i = 0; i < (int)WS_BUFFER_COUNT; i++) {
    if (ws_buffer_ref_counts_[i] == 0) {
      ws_buffer_ref_counts_[i] = -1; // Mark as "filling"
      s_ws_buffer_acquired_ticks[i] = now;
      buf_idx = i;
      break;
    }
  }
  portEXIT_CRITICAL(&s_ws_mux);

  if (buf_idx == -1) {
    static uint32_t last_skip_log = 0;
    if (tel.frame_id - last_skip_log > 80) { // Log every ~5s @ 16Hz
      ESP_LOGW(TAG, "All WS buffers busy — frame skipped");
      last_skip_log = tel.frame_id;
    }
    return;
  }

  // ---- Serialize binary frame ----
  uint8_t *p = ws_buffers_[buf_idx];
  size_t ofs = 0;

  // W1-3: Clamp num_tracks defensively — static_assert guarantees the full
  // frame fits when num_tracks <= MAX_TRACKS.  The assert is at file scope.
  const uint8_t n_tracks =
      (tel.num_tracks <= (uint8_t)ThermalConfig::MAX_TRACKS)
          ? tel.num_tracks
          : (uint8_t)ThermalConfig::MAX_TRACKS;

  // Unchecked write helpers — safe because of the static_assert
#define WS_WRITE_BYTE(b) p[ofs++] = (uint8_t)(b)
#define WS_WRITE_U16_LE(v)                                                     \
  do {                                                                         \
    p[ofs++] = (uint8_t)((v) & 0xFFu);                                         \
    p[ofs++] = (uint8_t)(((v) >> 8) & 0xFFu);                                  \
  } while (0)
#define WS_WRITE_S16_LE(v) WS_WRITE_U16_LE((uint16_t)(int16_t)(v))
#define WS_WRITE_BYTES(src, len)                                               \
  do {                                                                         \
    memcpy(&p[ofs], (src), (len));                                             \
    ofs += (len);                                                              \
  } while (0)

  // Header (14 bytes)
  WS_WRITE_BYTE(WS_FRAME_MAGIC);            // [0]  magic 0x12
  WS_WRITE_BYTE(sensor_ok ? 1u : 0u);       // [1]  sensor_ok
  WS_WRITE_BYTES(&tel.ambient_temp, 4);     // [2-5] ambient_temp float32 LE
  WS_WRITE_U16_LE(tel.count_in);   // [6-7]  count_in
  WS_WRITE_U16_LE(tel.count_out);  // [8-9]  count_out
  WS_WRITE_BYTE(n_tracks);                  // [10]   num_tracks
  WS_WRITE_U16_LE(s_session_id);            // [11-12] session_id  (W3)
  WS_WRITE_BYTE(s_time_quality);            // [13]   time_quality (W3)

  // Track data (11 bytes each, W4: includes peak_temp_100)
  for (int i = 0; i < n_tracks; i++) {
    const TrackInfo &t = tel.tracks[i];
    WS_WRITE_BYTE(t.id);
    WS_WRITE_S16_LE(t.x_100);
    WS_WRITE_S16_LE(t.y_100);
    WS_WRITE_S16_LE(t.v_x_100);
    WS_WRITE_S16_LE(t.v_y_100);
    WS_WRITE_S16_LE(t.peak_temp_100);
  }

  // Pixel data (1536 bytes)
  if (ofs + ThermalConfig::TOTAL_PIXELS * sizeof(int16_t) > WS_BUFFER_SIZE) {
    ESP_LOGE(TAG, "broadcastFrame: pixel block overflow");
    portENTER_CRITICAL(&s_ws_mux);
    ws_buffer_ref_counts_[buf_idx] = 0;
    portEXIT_CRITICAL(&s_ws_mux);
    return;
  }
  memcpy(&p[ofs], img.pixels, ThermalConfig::TOTAL_PIXELS * sizeof(int16_t));
  ofs += ThermalConfig::TOTAL_PIXELS * sizeof(int16_t);

  const size_t total_len = ofs;

#undef WS_WRITE_BYTE
#undef WS_WRITE_U16_LE
#undef WS_WRITE_S16_LE
#undef WS_WRITE_BYTES

  // ---- W4-FIX: Atomic reference counting + Async broadcast ----
  // This addresses the fatal FreeRTOS violation where httpd_ws_get_fd_info
  // (which takes a mutex) was called inside a spinlock (interrupts disabled).
  int ws_fds[7]; // max_open_sockets is 7
  int ws_count = 0;
  int client_fds[7];
  size_t clients = sizeof(client_fds) / sizeof(client_fds[0]);

  // 1. Identify valid WS clients OUTSIDE critical section (safe to take mutexes
  // here)
  if (httpd_get_client_list(server_, &clients, client_fds) == ESP_OK) {
    for (size_t i = 0; i < clients; i++) {
      if (httpd_ws_get_fd_info(server_, client_fds[i]) ==
          HTTPD_WS_CLIENT_WEBSOCKET) {
        ws_fds[ws_count++] = client_fds[i];
      }
    }
  }

  // 2. Atomic update of reference counter (Spinlock duration: < 1 microsecond)
  portENTER_CRITICAL(&s_ws_mux);
  ws_buffer_ref_counts_[buf_idx] = ws_count;
  portEXIT_CRITICAL(&s_ws_mux);

  if (ws_count == 0)
    return;

  // 3. Queue async sends using the local validated list
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(ws_pkt));
  ws_pkt.type = HTTPD_WS_TYPE_BINARY;
  ws_pkt.payload = p;
  ws_pkt.len = total_len;
  ws_pkt.final = true;

  int successful = 0;
  for (int i = 0; i < ws_count; i++) {
    esp_err_t err = httpd_ws_send_data_async(server_, ws_fds[i], &ws_pkt,
                                             wsAsyncCompletionCb,
                                             (void *)(uintptr_t)buf_idx);
    if (err == ESP_OK) {
      successful++;
    }
  }

  // If some sends failed to queue, decrement ref_count accordingly so the
  // buffer is eventually released (completion callback handles the rest).
  if (successful < ws_count) {
    portENTER_CRITICAL(&s_ws_mux);
    int failed = ws_count - successful;
    if ((int)ws_buffer_ref_counts_[buf_idx] >= failed) {
      ws_buffer_ref_counts_[buf_idx] -= failed;
    } else {
      ws_buffer_ref_counts_[buf_idx] = 0;
    }
    portEXIT_CRITICAL(&s_ws_mux);
  }
}

// =============================================================================
//  FIX-3: Event buffer pool for broadcastEvent (async, non-blocking)
// =============================================================================
#define WS_EVENT_BUFFER_COUNT 4
#define WS_EVENT_BUFFER_SIZE  256
static uint8_t  s_event_buffers[WS_EVENT_BUFFER_COUNT][WS_EVENT_BUFFER_SIZE];
static int      s_event_ref_counts[WS_EVENT_BUFFER_COUNT];
static uint32_t s_event_acquired_ticks[WS_EVENT_BUFFER_COUNT];
static portMUX_TYPE s_event_mux = portMUX_INITIALIZER_UNLOCKED;

static void eventAsyncCompletionCb(esp_err_t err, int socket, void *arg) {
    int buf_idx = (int)(uintptr_t)arg;
    if (buf_idx < 0 || buf_idx >= WS_EVENT_BUFFER_COUNT)
        return;
    portENTER_CRITICAL(&s_event_mux);
    if (s_event_ref_counts[buf_idx] > 0) {
        s_event_ref_counts[buf_idx]--;
    }
    portEXIT_CRITICAL(&s_event_mux);
    if (err != ESP_OK && err != ESP_ERR_HTTPD_INVALID_REQ) {
        ESP_LOGV(TAG, "eventAsync: fd=%d err=%s", socket, esp_err_to_name(err));
    }
}

// =============================================================================
//  W1-3: REFERENCE-COUNTED WS ASYNC COMPLETION CALLBACK
// =============================================================================

void HttpServer::wsAsyncCompletionCb(esp_err_t err, int socket, void *arg) {
  int buf_idx = (int)(uintptr_t)arg;
  if (buf_idx < 0 || buf_idx >= (int)WS_BUFFER_COUNT)
    return;

  portENTER_CRITICAL(&s_ws_mux);
  if (ws_buffer_ref_counts_[buf_idx] > 0) {
    ws_buffer_ref_counts_[buf_idx]--;
  }
  portEXIT_CRITICAL(&s_ws_mux);

  if (err != ESP_OK && err != ESP_ERR_HTTPD_INVALID_REQ) {
    ESP_LOGV(TAG, "wsAsync: fd=%d err=%s", socket, esp_err_to_name(err));
  }
}

esp_err_t HttpServer::sdInfoHandler(httpd_req_t *req) {
  if (!g_sd.isMounted()) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "SD Card not mounted");
    return ESP_FAIL;
  }

  double free_bytes = (double)g_sd.getFreeSpaceBytes();
  double total_bytes = (double)g_sd.getTotalSpaceBytes();

  cJSON *resp = cJSON_CreateObject();
  cJSON_AddNumberToObject(resp, "free_bytes", free_bytes);
  cJSON_AddNumberToObject(resp, "total_bytes", total_bytes);

  // Merge files from logs/ and clips/ into single array
  cJSON *all_files = cJSON_CreateArray();
  char json_buf[2048];

  auto mergeDir = [&](const char* dir) {
    if (g_sd.listDirectory(dir, json_buf, sizeof(json_buf)) == ESP_OK) {
      cJSON *arr = cJSON_Parse(json_buf);
      if (arr && cJSON_IsArray(arr)) {
        int n = cJSON_GetArraySize(arr);
        for (int i = 0; i < n; i++) {
          cJSON *item = cJSON_Duplicate(cJSON_GetArrayItem(arr, i), true);
          if (item) cJSON_AddItemToArray(all_files, item);
        }
      }
      if (arr) cJSON_Delete(arr);
    }
  };

  mergeDir("logs");
  mergeDir("clips");
  cJSON_AddItemToObject(resp, "files", all_files);

  int file_count = cJSON_GetArraySize(all_files);
  ESP_LOG_COLOR(LOG_COLOR_CYAN, TAG, "SD info: free=%.0f/%.0f MB  files=%d",
                free_bytes / 1048576.0, total_bytes / 1048576.0, file_count);

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_type(req, "application/json");
  char *json_str = cJSON_PrintUnformatted(resp);
  cJSON_Delete(resp);
  if (json_str) {
      httpd_resp_sendstr(req, json_str);
      free(json_str);
  } else {
      httpd_resp_send_500(req);
  }
  return ESP_OK;
}

esp_err_t HttpServer::sdDownloadHandler(httpd_req_t *req) {
  if (!g_sd.isMounted()) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "SD Card not mounted");
    return ESP_FAIL;
  }

  char query[128];
  char path[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "file", path, sizeof(path)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing file param");
    return ESP_FAIL;
  }

  // Basic path traversal protection
  if (strstr(path, "..")) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
    return ESP_FAIL;
  }

  if (!g_sd.fileExists(path)) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    return ESP_FAIL;
  }

  char full_path[256];
  snprintf(full_path, sizeof(full_path), "/sdcard/%s", path);

  g_sd.lock();
  FILE *f = fopen(full_path, "r");
  if (!f) {
    g_sd.unlock();
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Failed to open file");
    return ESP_FAIL;
  }

  if (strstr(path, ".thv")) {
    httpd_resp_set_type(req, "application/octet-stream");
  } else {
    httpd_resp_set_type(req, "text/csv");
  }

  // Extraer el nombre de archivo (basename) para Content-Disposition
  const char *basename = strrchr(path, '/');
  basename = basename ? (basename + 1) : path;

  char disp[256];
  snprintf(disp, sizeof(disp), "attachment; filename=%s", basename);
  httpd_resp_set_hdr(req, "Content-Disposition", disp);

  char chunk[1024];
  size_t n;
  while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
    if (httpd_resp_send_chunk(req, chunk, n) != ESP_OK) {
      fclose(f);
      g_sd.unlock();
      return ESP_FAIL;
    }
  }
  fclose(f);
  g_sd.unlock();
  httpd_resp_send_chunk(req, NULL, 0); // End
  return ESP_OK;
}

esp_err_t HttpServer::sdDeleteHandler(httpd_req_t *req) {
  if (!g_sd.isMounted()) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "SD Card not mounted");
    return ESP_FAIL;
  }

  char buf[128];
  int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf) - 1));
  if (ret <= 0) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty request");
    return ESP_FAIL;
  }
  buf[ret] = '\0';

  cJSON *root = cJSON_Parse(buf);
  if (!root) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    return ESP_FAIL;
  }

  cJSON *file_j = cJSON_GetObjectItem(root, "file");
  if (!cJSON_IsString(file_j) || strstr(file_j->valuestring, "..")) {
    cJSON_Delete(root);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid file param");
    return ESP_FAIL;
  }

  if (g_sd.deleteFile(file_j->valuestring) != ESP_OK) {
    cJSON_Delete(root);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Failed to delete file");
    return ESP_FAIL;
  }

  cJSON_Delete(root);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
}

// =============================================================================
//  HTTP HANDLER — SD ERASE ALL (/api/sd/erase_all)
// =============================================================================

esp_err_t HttpServer::sdEraseAllHandler(httpd_req_t *req) {
  if (!g_sd.isMounted()) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "SD Card not mounted");
    return ESP_FAIL;
  }

  auto eraseDir = [&](const char* dir) -> bool {
    char json_buf[1024];
    if (g_sd.listDirectory(dir, json_buf, sizeof(json_buf)) != ESP_OK) {
      return true; // dir doesn't exist — not an error
    }
    cJSON *arr = cJSON_Parse(json_buf);
    if (!arr || !cJSON_IsArray(arr)) {
      if (arr) cJSON_Delete(arr);
      return true;
    }
    int n = cJSON_GetArraySize(arr);
    bool ok = true;
    for (int i = 0; i < n; i++) {
      cJSON *item = cJSON_GetArrayItem(arr, i);
      cJSON *name_j = cJSON_GetObjectItem(item, "name");
      if (cJSON_IsString(name_j)) {
        char file_path[128];
        snprintf(file_path, sizeof(file_path), "%s/%s", dir, name_j->valuestring);
        if (g_sd.deleteFile(file_path) != ESP_OK) {
          ok = false;
        }
      }
    }
    cJSON_Delete(arr);
    return ok;
  };

  bool logs_ok = eraseDir("logs");
  bool clips_ok = eraseDir("clips");

  // Recreate the base directories
  g_sd.mkdir("logs");
  g_sd.mkdir("clips");

  cJSON *resp = cJSON_CreateObject();
  cJSON_AddBoolToObject(resp, "ok", true);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_type(req, "application/json");
  char *json_str = cJSON_PrintUnformatted(resp);
  cJSON_Delete(resp);
  if (json_str) {
    httpd_resp_sendstr(req, json_str);
    free(json_str);
  } else {
    httpd_resp_send_500(req);
  }
  return ESP_OK;
}

// =============================================================================
//  HTTP HANDLER — NVS BACKUP DOWNLOAD (/api/nvs/backup)
// =============================================================================

esp_err_t HttpServer::nvsBackupHandler(httpd_req_t *req) {
  nvs_handle_t h;
  esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
  if (err != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "NVS open failed");
    return ESP_FAIL;
  }

  cJSON *root = cJSON_CreateObject();
  if (!root) { nvs_close(h); return ESP_ERR_NO_MEM; }

  auto readInt = [&](const char *key, const char *json_key) {
    int32_t v = 0;
    if (nvs_get_i32(h, key, &v) == ESP_OK)
      cJSON_AddNumberToObject(root, json_key, (double)v);
  };

  readInt("nvs_base_in",   "baseline_in");
  readInt("nvs_base_out",  "baseline_out");
  readInt("temp_bio",      "temp_bio_raw");
  readInt("delta_t",       "delta_t_raw");
  readInt("alpha_ema",     "alpha_ema_raw");
  readInt("line_entry",    "line_entry");
  readInt("line_exit",     "line_exit");
  readInt("dead_left",     "dead_left");
  readInt("dead_right",    "dead_right");
  readInt("sensor_h",      "sensor_h_raw");
  readInt("person_d",      "person_d_raw");
  readInt("view_mode",     "view_mode");

  int8_t use_seg = 0;
  if (nvs_get_i8(h, "use_segments", &use_seg) == ESP_OK)
    cJSON_AddBoolToObject(root, "use_segments", use_seg != 0);

  char seg_lines[512];
  size_t slen = sizeof(seg_lines);
  if (nvs_get_str(h, "seg_lines", seg_lines, &slen) == ESP_OK)
    cJSON_AddStringToObject(root, "seg_lines", seg_lines);

  int32_t sid = 0;
  if (nvs_get_i32(h, "session_id", &sid) == ESP_OK)
    cJSON_AddNumberToObject(root, "session_id", (double)sid);

  nvs_close(h);

  char *json_str = cJSON_Print(root);
  cJSON_Delete(root);

  if (!json_str) return ESP_ERR_NO_MEM;

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Content-Disposition",
                     "attachment; filename=nvs_backup.json");
  httpd_resp_sendstr(req, json_str);
  free(json_str);
  return ESP_OK;
}

// =============================================================================
//  HTTP HANDLER — CLIP LIST (/api/clips)
// =============================================================================

esp_err_t HttpServer::clipListHandler(httpd_req_t *req) {
  if (!g_sd.isMounted()) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "SD Card not mounted");
    return ESP_FAIL;
  }

  char list_json[2048];
  if (g_sd.listDirectory("clips", list_json, sizeof(list_json)) != ESP_OK) {
    ESP_LOGW(TAG, "clipList: listDirectory('clips') FAILED");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Failed to list clips");
    return ESP_FAIL;
  }

  cJSON *files = cJSON_Parse(list_json);
  if (!files || !cJSON_IsArray(files)) {
    if (files) cJSON_Delete(files);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Parse error");
    return ESP_FAIL;
  }

  int total_files = cJSON_GetArraySize(files);
  int thv_count = 0;
  int magic_ok = 0;

  cJSON *resp = cJSON_CreateObject();
  cJSON *items = cJSON_CreateArray();

  cJSON *f;
  cJSON_ArrayForEach(f, files) {
    cJSON *name_j = cJSON_GetObjectItem(f, "name");
    if (!cJSON_IsString(name_j)) continue;
    const char *name = name_j->valuestring;

    // Only .thv files
    const char *dot = strrchr(name, '.');
    if (!dot || strcmp(dot, ".thv") != 0) continue;
    thv_count++;

    // Read ThvHeader from file
    char full_path[96];
    snprintf(full_path, sizeof(full_path), "/sdcard/clips/%s", name);
    FILE *fp = fopen(full_path, "rb");
    if (!fp) {
      ESP_LOGW(TAG, "clipList: cannot open %s", name);
      continue;
    }

    ThvHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    size_t nread = fread(&hdr, 1, sizeof(hdr), fp);
    fclose(fp);
    if (nread != sizeof(hdr) || hdr.magic != THV_MAGIC) {
      ESP_LOGD(TAG, "clipList: %s magic=0x%08X (expected 0x%08X) nread=%u",
               name, (unsigned)hdr.magic, (unsigned)THV_MAGIC,
               (unsigned)nread);
      continue;
    }
    magic_ok++;

    // Build item JSON
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "name", name);
    cJSON_AddNumberToObject(item, "frame_count", hdr.frame_count);
    cJSON_AddNumberToObject(item, "fps", hdr.fps);
    cJSON_AddNumberToObject(item, "width", hdr.width);
    cJSON_AddNumberToObject(item, "height", hdr.height);
    cJSON_AddNumberToObject(item, "trigger_dir", hdr.trigger_dir);

    // File size
    struct stat st;
    if (stat(full_path, &st) == 0) {
      cJSON_AddNumberToObject(item, "size_bytes", (double)st.st_size);
    }

    cJSON_AddItemToArray(items, item);
  }

  ESP_LOG_COLOR(LOG_COLOR_CYAN, TAG, "Clips: %d files in dir, %d .thv, %d magic OK → %d in response",
                total_files, thv_count, magic_ok, magic_ok);

  cJSON_AddItemToObject(resp, "clips", items);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_type(req, "application/json");

  char *json_str = cJSON_Print(resp);
  cJSON_Delete(resp);
  if (!json_str) return ESP_ERR_NO_MEM;

  httpd_resp_sendstr(req, json_str);
  free(json_str);
  return ESP_OK;
}

// downloadLogHandler (legacy) remains for compatibility if needed, or we can
// leave it.
esp_err_t HttpServer::downloadLogHandler(httpd_req_t *req) {
  if (!g_sd.isMounted()) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "SD Card not mounted");
    return ESP_FAIL;
  }

  // H4-FIX: Use date-aware log path
  char path[64];
  getLogPathForToday(path, sizeof(path));
  if (!g_sd.fileExists(path)) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Log file not found");
    return ESP_FAIL;
  }

  g_sd.lock();
  char full_path[80];
  snprintf(full_path, sizeof(full_path), "/sdcard/%s", path);
  FILE *f = fopen(full_path, "r");
  if (!f) {
    g_sd.unlock();
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Failed to open log file");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "text/csv");
  httpd_resp_set_hdr(req, "Content-Disposition",
                     "attachment; filename=thermal_counts_full.csv");

  char chunk[1024];
  size_t n;
  while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
    if (httpd_resp_send_chunk(req, chunk, n) != ESP_OK) {
      fclose(f);
      g_sd.unlock();
      return ESP_FAIL;
    }
  }
  fclose(f);
  g_sd.unlock();
  httpd_resp_send_chunk(req, NULL, 0); // End of stream
  return ESP_OK;
}

void HttpServer::broadcastEvent(const CrossingEvent &ev, float ambient_temp,
                                uint8_t active_tracks) {
  if (server_ == NULL)
    return;

  const char* clip_id = ThermalRecorder::getCurrentClipId();

  // W4-CSV: Persist to SD if available
  if (g_sd.isMounted()) {
    char log_file[64];
    getLogPathForToday(log_file, sizeof(log_file));

    char line[192];
    // Format:
    // event_type,session,timestamp_ms,clip_id,dir,count_in,count_out,track_temp,ambient_temp,active_tracks,track_id,duration_ms,frame_count,crossings,saved
    snprintf(line, sizeof(line),
             "CROSSING,%u,%" PRIu64 ",%s,%s,%d,%d,%.2f,%.2f,%u,%u,,,,,",
             s_session_id, (uint64_t)ev.timestamp_ms, clip_id,
             ev.is_in ? "IN" : "OUT",
             ev.count_in, ev.count_out, ev.temperature, ambient_temp,
             active_tracks, ev.id);

    if (!g_sd.fileExists(log_file)) {
      g_sd.appendLine(log_file,
                      "event_type,session,timestamp_ms,clip_id,direction,count_in,"
                      "count_out,track_temp,ambient_temp,active_tracks,"
                      "track_id,duration_ms,frame_count,crossings,saved");
    }
    g_sd.appendLine(log_file, line);
  }

  // FIX-3: Async pool with ref-counting (mirrors broadcastFrame pattern)
  // Ensures TelemetryTask never blocks on slow WebSocket clients.

  // Watchdog: release stuck buffers
  uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
  portENTER_CRITICAL(&s_event_mux);
  for (int i = 0; i < WS_EVENT_BUFFER_COUNT; i++) {
    if (s_event_ref_counts[i] > 0 || s_event_ref_counts[i] == -1) {
      if (now - s_event_acquired_ticks[i] > WS_BUFFER_MAX_AGE_MS) {
        s_event_ref_counts[i] = 0;
        ESP_LOGW(TAG, "event buf %d stuck for %lu ms, forced free", i,
                 now - s_event_acquired_ticks[i]);
      }
    }
  }
  portEXIT_CRITICAL(&s_event_mux);

  // Find free buffer
  int buf_idx = -1;
  portENTER_CRITICAL(&s_event_mux);
  for (int i = 0; i < WS_EVENT_BUFFER_COUNT; i++) {
    if (s_event_ref_counts[i] == 0) {
      s_event_ref_counts[i] = -1;
      s_event_acquired_ticks[i] = now;
      buf_idx = i;
      break;
    }
  }
  portEXIT_CRITICAL(&s_event_mux);

  if (buf_idx == -1)
    return; // All buffers busy — skip event

  // Serialize JSON
  int written =
      snprintf((char *)s_event_buffers[buf_idx], WS_EVENT_BUFFER_SIZE,
               "{\"type\":\"crossing\",\"dir\":\"%s\",\"cnt_in\":%d,\"cnt_"
               "out\":%d,\"temp\":%.2f,\"id\":%u,\"ts\":%" PRIu64
               ",\"clip\":\"%s\"}",
               ev.is_in ? "IN" : "OUT", ev.count_in, ev.count_out,
               ev.temperature, ev.id, (uint64_t)ev.timestamp_ms,
               (clip_id[0] ? clip_id : ""));

  if (written <= 0 || written >= WS_EVENT_BUFFER_SIZE) {
    ESP_LOGW(TAG, "broadcastEvent: JSON truncated or error");
    portENTER_CRITICAL(&s_event_mux);
    s_event_ref_counts[buf_idx] = 0;
    portEXIT_CRITICAL(&s_event_mux);
    return;
  }

  // Identify WS clients
  int ws_fds[7];
  int ws_count = 0;
  int client_fds[7];
  size_t clients = sizeof(client_fds) / sizeof(client_fds[0]);

  if (httpd_get_client_list(server_, &clients, client_fds) == ESP_OK) {
    for (size_t i = 0; i < clients; i++) {
      if (httpd_ws_get_fd_info(server_, client_fds[i]) ==
          HTTPD_WS_CLIENT_WEBSOCKET) {
        ws_fds[ws_count++] = client_fds[i];
      }
    }
  }

  // Set ref count
  portENTER_CRITICAL(&s_event_mux);
  s_event_ref_counts[buf_idx] = ws_count;
  portEXIT_CRITICAL(&s_event_mux);

  if (ws_count == 0)
    return;

  // Queue async sends
  httpd_ws_frame_t pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type = HTTPD_WS_TYPE_TEXT;
  pkt.payload = s_event_buffers[buf_idx];
  pkt.len = (size_t)written;
  pkt.final = true;

  int successful = 0;
  for (int i = 0; i < ws_count; i++) {
    esp_err_t err = httpd_ws_send_data_async(server_, ws_fds[i], &pkt,
                                              eventAsyncCompletionCb,
                                              (void *)(uintptr_t)buf_idx);
    if (err == ESP_OK)
      successful++;
  }

  if (successful < ws_count) {
    portENTER_CRITICAL(&s_event_mux);
    int failed = ws_count - successful;
    if (s_event_ref_counts[buf_idx] >= failed) {
      s_event_ref_counts[buf_idx] -= failed;
    } else {
      s_event_ref_counts[buf_idx] = 0;
    }
    portEXIT_CRITICAL(&s_event_mux);
  }
}

// ---------------------------------------------------------------------------
//  onClipEvent  — called from ThermalRecorder writer task via callback
//                 Appends CLIP_START / CLIP_END rows to the daily CSV log
// ---------------------------------------------------------------------------
void HttpServer::onClipEvent(const char *event_type, const char *clip_id,
                             uint32_t timestamp_ms, uint32_t dur_ms,
                             uint32_t frame_count, uint32_t crossings,
                             bool saved) {
  if (server_ == NULL || !g_sd.isMounted())
    return;

  // Extract basename from full path (/sdcard/clips/CLIP_00005.thv → CLIP_00005.thv)
  const char *basename = strrchr(clip_id, '/');
  if (basename)
    basename++;
  else
    basename = clip_id;

  char log_file[64];
  getLogPathForToday(log_file, sizeof(log_file));

  // Always ensure header exists (harmless no-op if already present)
  if (!g_sd.fileExists(log_file)) {
    g_sd.appendLine(log_file,
                    "event_type,session,timestamp_ms,clip_id,direction,count_in,"
                    "count_out,track_temp,ambient_temp,active_tracks,"
                    "track_id,duration_ms,frame_count,crossings,saved");
  }

  char line[192];
  snprintf(line, sizeof(line),
           "%s,%u,%" PRIu64 ",%s,,,,,,,%lu,%lu,%lu,%u",
           event_type, s_session_id, (uint64_t)timestamp_ms, basename,
           (unsigned long)dur_ms, (unsigned long)frame_count,
           (unsigned long)crossings, (unsigned int)(saved ? 1 : 0));

  g_sd.appendLine(log_file, line);
}
