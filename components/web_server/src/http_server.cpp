#include "http_server.hpp"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_ota_ops.h"
#include "esp_system.h" // For esp_restart
#include <string.h>
#include "esp_app_format.h"
#include <sys/param.h>  // For MIN()

static const char *TAG        = "HTTP_SERVER";
static const char *NVS_NS     = "thcfg";   ///< NVS namespace for thermal config

httpd_handle_t HttpServer::server_ = NULL;
static QueueHandle_t s_configQueue = NULL;

// Static buffers for WebSocket broadcasting
// Using __attribute__((aligned(4))) to ensure memory alignment for network/DMA operations
uint8_t HttpServer::ws_buffers_[WS_BUFFER_COUNT][WS_BUFFER_SIZE] __attribute__((aligned(4)));
int     HttpServer::ws_buffer_ref_counts_[WS_BUFFER_COUNT] = {0, 0};
static portMUX_TYPE s_ws_mux    = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_config_mux = portMUX_INITIALIZER_UNLOCKED; // Bug5-fix: protects ThermalConfig global reads from Core 0

// Embedded File Pointers (From CMake EMBED_FILES)
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[]   asm("_binary_style_css_end");

extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[]   asm("_binary_app_js_end");

// =============================================================================
//  NVS HELPERS  (Core 0, called only from HTTP task context)
// =============================================================================

/**
 * @brief Load persisted configuration from NVS and apply to ThermalConfig globals.
 *        Called once during HttpServer::start().
 */
static void loadConfigFromNvs(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "NVS: no saved configuration - using default values");
        return;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS: could not open namespace (%s)", esp_err_to_name(err));
        return;
    }

    // Helper: read a float stored as int32 (value × 1000)
    auto readFloat = [&](const char *key, float &dest) {
        int32_t raw = 0;
        if (nvs_get_i32(h, key, &raw) == ESP_OK) {
            dest = (float)raw / 1000.0f;
        }
    };
    auto readInt = [&](const char *key, int &dest) {
        int32_t raw = 0;
        if (nvs_get_i32(h, key, &raw) == ESP_OK) {
            dest = (int)raw;
        }
    };

    readFloat("temp_bio",   ThermalConfig::BIOLOGICAL_TEMP_MIN);
    readFloat("delta_t",    ThermalConfig::BACKGROUND_DELTA_T);
    readFloat("alpha_ema",  ThermalConfig::EMA_ALPHA);
    readInt  ("line_entry", ThermalConfig::DEFAULT_LINE_ENTRY_Y);
    readInt  ("line_exit",  ThermalConfig::DEFAULT_LINE_EXIT_Y);
    readInt  ("dead_left",  ThermalConfig::DEFAULT_DEAD_ZONE_LEFT);
    readInt  ("dead_right", ThermalConfig::DEFAULT_DEAD_ZONE_RIGHT);
    readInt  ("nms_center", ThermalConfig::NMS_RADIUS_CENTER_SQ);
    readInt  ("nms_edge",   ThermalConfig::NMS_RADIUS_EDGE_SQ);
    readInt  ("view_mode",  ThermalConfig::VIEW_MODE);

    size_t lines_len = 512;
    char lines_buf[512];
    if (nvs_get_str(h, "seg_lines", lines_buf, &lines_len) == ESP_OK) {
        cJSON* lines_root = cJSON_Parse(lines_buf);
        if (lines_root) {
            if (cJSON_IsArray(lines_root)) {
                int n = cJSON_GetArraySize(lines_root);
                n = n > MAX_COUNTING_LINES ? MAX_COUNTING_LINES : n;
                ThermalConfig::door_lines.num_lines = 0;
                for (int i = 0; i < n; i++) {
                    cJSON* l = cJSON_GetArrayItem(lines_root, i);
                    if (cJSON_IsObject(l)) {
                        CountingSegment& s = ThermalConfig::door_lines.lines[i];
                        cJSON* jx1 = cJSON_GetObjectItem(l, "x1");
                        cJSON* jy1 = cJSON_GetObjectItem(l, "y1");
                        cJSON* jx2 = cJSON_GetObjectItem(l, "x2");
                        cJSON* jy2 = cJSON_GetObjectItem(l, "y2");
                        s.x1 = cJSON_IsNumber(jx1) ? (float)jx1->valuedouble : 0.f;
                        s.y1 = cJSON_IsNumber(jy1) ? (float)jy1->valuedouble : 0.f;
                        s.x2 = cJSON_IsNumber(jx2) ? (float)jx2->valuedouble : 0.f;
                        s.y2 = cJSON_IsNumber(jy2) ? (float)jy2->valuedouble : 0.f;
                        s.id = i + 1;
                        s.enabled = true;
                        snprintf(s.name, sizeof(s.name), "Linea %d", i + 1);
                        ThermalConfig::door_lines.num_lines++;
                    }
                }
            }
            cJSON_Delete(lines_root);
        }
    }
    int8_t use_seg = 0;
    if (nvs_get_i8(h, "use_segments", &use_seg) == ESP_OK) {
        ThermalConfig::door_lines.use_segments = (use_seg != 0);
    } else {
        ThermalConfig::door_lines.use_segments = false;
    }

    nvs_close(h);
    ESP_LOGI(TAG, "NVS: configuration loaded - temp_bio=%.1f delta_t=%.1f alpha=%.2f entry=%d exit=%d nms_c=%d nms_e=%d mode=%d",
             ThermalConfig::BIOLOGICAL_TEMP_MIN, ThermalConfig::BACKGROUND_DELTA_T,
             ThermalConfig::EMA_ALPHA,
             ThermalConfig::DEFAULT_LINE_ENTRY_Y, ThermalConfig::DEFAULT_LINE_EXIT_Y,
             ThermalConfig::NMS_RADIUS_CENTER_SQ, ThermalConfig::NMS_RADIUS_EDGE_SQ,
             ThermalConfig::VIEW_MODE);
}

/**
 * @brief Persist current ThermalConfig globals to NVS flash.
 *        Called when client sends {"cmd":"SAVE_CONFIG"}.
 *
 * @return ESP_OK on success.
 */
static esp_err_t saveConfigToNvs(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS: could not open for writing (%s)", esp_err_to_name(err));
        return err;
    }

    // Store floats as int32 (value * 1000) - NVS has no float type
    nvs_set_i32(h, "temp_bio",   (int32_t)(ThermalConfig::BIOLOGICAL_TEMP_MIN * 1000.0f));
    nvs_set_i32(h, "delta_t",    (int32_t)(ThermalConfig::BACKGROUND_DELTA_T      * 1000.0f));
    nvs_set_i32(h, "alpha_ema",  (int32_t)(ThermalConfig::EMA_ALPHA          * 1000.0f));
    nvs_set_i32(h, "line_entry", (int32_t) ThermalConfig::DEFAULT_LINE_ENTRY_Y);
    nvs_set_i32(h, "line_exit",  (int32_t) ThermalConfig::DEFAULT_LINE_EXIT_Y);
    nvs_set_i32(h, "dead_left",  (int32_t) ThermalConfig::DEFAULT_DEAD_ZONE_LEFT);
    nvs_set_i32(h, "dead_right", (int32_t) ThermalConfig::DEFAULT_DEAD_ZONE_RIGHT);
    nvs_set_i32(h, "nms_center", (int32_t) ThermalConfig::NMS_RADIUS_CENTER_SQ);
    nvs_set_i32(h, "nms_edge",   (int32_t) ThermalConfig::NMS_RADIUS_EDGE_SQ);
    nvs_set_i32(h, "view_mode",  (int32_t) ThermalConfig::VIEW_MODE);

    cJSON* lines_arr = cJSON_CreateArray();
    for (int i = 0; i < ThermalConfig::door_lines.num_lines; i++) {
        const CountingSegment& s = ThermalConfig::door_lines.lines[i];
        cJSON* l = cJSON_CreateObject();
        cJSON_AddNumberToObject(l, "x1", s.x1);
        cJSON_AddNumberToObject(l, "y1", s.y1);
        cJSON_AddNumberToObject(l, "x2", s.x2);
        cJSON_AddNumberToObject(l, "y2", s.y2);
        cJSON_AddItemToArray(lines_arr, l);
    }
    char* str_lines = cJSON_PrintUnformatted(lines_arr);
    if (str_lines) {
        nvs_set_str(h, "seg_lines", str_lines);
        free(str_lines);
    }
    cJSON_Delete(lines_arr);
    nvs_set_i8(h, "use_segments", ThermalConfig::door_lines.use_segments ? 1 : 0);

    err = nvs_commit(h);
    nvs_close(h);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "NVS: configuration saved successfully");
    } else {
        ESP_LOGE(TAG, "NVS: error committing write (%s)", esp_err_to_name(err));
    }
    return err;
}

/**
 * @brief Send a JSON text frame back to the requesting WebSocket client.
 */
static void wsSendJson(httpd_req_t *req, cJSON *root) {
    char *str = cJSON_PrintUnformatted(root);
    if (!str) return;
    httpd_ws_frame_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type    = HTTPD_WS_TYPE_TEXT;
    pkt.payload = (uint8_t *)str;
    pkt.len     = strlen(str);
    httpd_ws_send_frame(req, &pkt);
    free(str);
}

// =============================================================================
//  HTTP HANDLERS
// =============================================================================

esp_err_t HttpServer::indexGetHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    return httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
}

esp_err_t HttpServer::styleGetHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400"); // CSS puede durar
    return httpd_resp_send(req, (const char *)style_css_start, style_css_end - style_css_start);
}

esp_err_t HttpServer::appJsGetHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    return httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start);
}

// =============================================================================
//  OTA POST HANDLER (/update)
// =============================================================================

esp_err_t HttpServer::otaPostHandler(httpd_req_t *req) {
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    if (update_partition == NULL) {
        ESP_LOGE(TAG, "OTA partition not found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Partition Not Found");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting OTA on partition: %s", update_partition->label);

    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Begin Failed");
        return err;
    }

    int remaining = req->content_len;
    char buf[1024];

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Error receiving OTA data");
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Receive Failed");
            return ESP_FAIL;
        }
        
        err = esp_ota_write(ota_handle, buf, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Write Failed");
            return err;
        }
        remaining -= recv_len;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA End Failed");
        return err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Set Boot Failed");
        return err;
    }

    ESP_LOGI(TAG, "OTA flash successful. Restarting in 1s...");
    httpd_resp_sendstr(req, "OTA Success");

    // Delay to allow HTTP response to be fully sent
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

// =============================================================================
//  WEBSOCKET — INCOMING MESSAGE HANDLER
// =============================================================================

void HttpServer::handleWebSocketMessage(httpd_req_t *req, httpd_ws_frame_t *ws_pkt) {
    if (ws_pkt->type != HTTPD_WS_TYPE_TEXT) return;

    cJSON *root = cJSON_Parse((const char *)ws_pkt->payload);
    if (!root) {
        ESP_LOGE(TAG, "Invalid WS JSON");
        return;
    }

    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (!cJSON_IsString(cmd)) {
        cJSON_Delete(root);
        return;
    }

    // -------------------------------------------------------------------------
    //  GET_CONFIG — returns current configuration as JSON
    // -------------------------------------------------------------------------
    if (strcmp(cmd->valuestring, "GET_CONFIG") == 0) {
        // Bug5-fix: atomic snapshot — all globals read inside a single critical section
        // to prevent torn reads on Xtensa LX7 during a Core 0/Core 1 context switch.
        float snap_temp_bio, snap_delta_t, snap_alpha_ema;
        int   snap_line_entry, snap_line_exit, snap_nms_center, snap_nms_edge, snap_view_mode;

        portENTER_CRITICAL(&s_config_mux);
        snap_temp_bio   = ThermalConfig::BIOLOGICAL_TEMP_MIN;
        snap_delta_t    = ThermalConfig::BACKGROUND_DELTA_T;
        snap_alpha_ema  = ThermalConfig::EMA_ALPHA;
        snap_line_entry = ThermalConfig::DEFAULT_LINE_ENTRY_Y;
        snap_line_exit  = ThermalConfig::DEFAULT_LINE_EXIT_Y;
        int snap_dead_left = ThermalConfig::DEFAULT_DEAD_ZONE_LEFT;
        int snap_dead_right = ThermalConfig::DEFAULT_DEAD_ZONE_RIGHT;
        snap_nms_center = ThermalConfig::NMS_RADIUS_CENTER_SQ;
        snap_nms_edge   = ThermalConfig::NMS_RADIUS_EDGE_SQ;
        snap_view_mode  = ThermalConfig::VIEW_MODE;
        portEXIT_CRITICAL(&s_config_mux);

        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "type",       "config");
        cJSON_AddNumberToObject(resp, "temp_bio",   (double)snap_temp_bio);
        cJSON_AddNumberToObject(resp, "delta_t",    (double)snap_delta_t);
        cJSON_AddNumberToObject(resp, "alpha_ema",  (double)snap_alpha_ema);
        cJSON_AddNumberToObject(resp, "line_entry", (double)snap_line_entry);
        cJSON_AddNumberToObject(resp, "line_exit",  (double)snap_line_exit);
        cJSON_AddNumberToObject(resp, "dead_left",  (double)snap_dead_left);
        cJSON_AddNumberToObject(resp, "dead_right", (double)snap_dead_right);
        cJSON_AddNumberToObject(resp, "nms_center", (double)snap_nms_center);
        cJSON_AddNumberToObject(resp, "nms_edge",   (double)snap_nms_edge);
        cJSON_AddNumberToObject(resp, "view_mode",  (double)snap_view_mode);

        cJSON_AddBoolToObject(resp, "use_segments", ThermalConfig::door_lines.use_segments);
        cJSON* lines_arr = cJSON_CreateArray();
        for (int i = 0; i < ThermalConfig::door_lines.num_lines; i++) {
            const CountingSegment& s = ThermalConfig::door_lines.lines[i];
            cJSON* l = cJSON_CreateObject();
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
    //  SET_PARAM — applies an individual parameter
    // -------------------------------------------------------------------------
    else if (strcmp(cmd->valuestring, "SET_PARAM") == 0) {
        cJSON *param = cJSON_GetObjectItem(root, "param");
        cJSON *val   = cJSON_GetObjectItem(root, "val");
        if (cJSON_IsString(param) && cJSON_IsNumber(val)) {
            AppConfigCmd cfgCmd;
            bool send = true;
            if      (strcmp(param->valuestring, "temp_bio")   == 0) { cfgCmd.type = ConfigCmdType::SET_TEMP_BIO;  cfgCmd.value = (float)val->valuedouble; }
            else if (strcmp(param->valuestring, "delta_t")    == 0) { cfgCmd.type = ConfigCmdType::SET_DELTA_T;   cfgCmd.value = (float)val->valuedouble; }
            else if (strcmp(param->valuestring, "alpha_ema")  == 0) { cfgCmd.type = ConfigCmdType::SET_EMA_ALPHA; cfgCmd.value = (float)val->valuedouble; }
            else if (strcmp(param->valuestring, "line_entry") == 0) { cfgCmd.type = ConfigCmdType::SET_LINE_ENTRY; cfgCmd.value = (float)val->valuedouble; }
            else if (strcmp(param->valuestring, "line_exit")  == 0) { cfgCmd.type = ConfigCmdType::SET_LINE_EXIT;  cfgCmd.value = (float)val->valuedouble; }
            else if (strcmp(param->valuestring, "dead_left")  == 0) { cfgCmd.type = ConfigCmdType::SET_DEAD_LEFT;  cfgCmd.value = (float)val->valuedouble; }
            else if (strcmp(param->valuestring, "dead_right") == 0) { cfgCmd.type = ConfigCmdType::SET_DEAD_RIGHT; cfgCmd.value = (float)val->valuedouble; }
            else if (strcmp(param->valuestring, "nms_center") == 0) { cfgCmd.type = ConfigCmdType::SET_NMS_CENTER; cfgCmd.value = (float)val->valuedouble; }
            else if (strcmp(param->valuestring, "nms_edge")   == 0) { cfgCmd.type = ConfigCmdType::SET_NMS_EDGE;   cfgCmd.value = (float)val->valuedouble; }
            else if (strcmp(param->valuestring, "view_mode")  == 0) { cfgCmd.type = ConfigCmdType::SET_VIEW_MODE;  cfgCmd.value = (float)val->valuedouble; }
            else { send = false; }

            if (send && s_configQueue) {
                xQueueSend(s_configQueue, &cfgCmd, 0);
                ESP_LOGI(TAG, "Config queued: %s = %.3f", param->valuestring, val->valuedouble);
            }
        }
    }

    // -------------------------------------------------------------------------
    //  SET_COUNTING_LINES — applies segment configuration
    // -------------------------------------------------------------------------
    else if (strcmp(cmd->valuestring, "SET_COUNTING_LINES") == 0) {
        cJSON* lines_arr = cJSON_GetObjectItem(root, "lines");
        if (cJSON_IsArray(lines_arr)) {
            int n = cJSON_GetArraySize(lines_arr);
            n = n > MAX_COUNTING_LINES ? MAX_COUNTING_LINES : n;
            
            portENTER_CRITICAL(&s_config_mux);
            ThermalConfig::door_lines.num_lines = 0;
            for (int i = 0; i < n; i++) {
                cJSON* line = cJSON_GetArrayItem(lines_arr, i);
                if (!cJSON_IsObject(line)) continue;
                
                CountingSegment& seg = ThermalConfig::door_lines.lines[i];
                cJSON* x1 = cJSON_GetObjectItem(line, "x1");
                cJSON* y1 = cJSON_GetObjectItem(line, "y1");
                cJSON* x2 = cJSON_GetObjectItem(line, "x2");
                cJSON* y2 = cJSON_GetObjectItem(line, "y2");
                
                if (cJSON_IsNumber(x1) && cJSON_IsNumber(y1) &&
                    cJSON_IsNumber(x2) && cJSON_IsNumber(y2)) {
                    seg.x1 = (float)x1->valuedouble;
                    seg.y1 = (float)y1->valuedouble;
                    seg.x2 = (float)x2->valuedouble;
                    seg.y2 = (float)y2->valuedouble;
                    seg.enabled = true;
                    seg.id = i + 1;
                    snprintf(seg.name, sizeof(seg.name), "Linea %d", i + 1);
                    ThermalConfig::door_lines.num_lines++;
                }
            }
            ThermalConfig::door_lines.use_segments = (ThermalConfig::door_lines.num_lines > 0);
            portEXIT_CRITICAL(&s_config_mux);
            
            if (s_configQueue) {
                AppConfigCmd cfgCmd;
                cfgCmd.type  = ConfigCmdType::APPLY_CONFIG;
                cfgCmd.value = 0;
                xQueueSend(s_configQueue, &cfgCmd, 0);
            }
            
            ESP_LOGI(TAG, "Counting lines updated: %d segments, use_segments=%d",
                     ThermalConfig::door_lines.num_lines,
                     ThermalConfig::door_lines.use_segments);
        }
    }

    // -------------------------------------------------------------------------
    //  SAVE_CONFIG — persists current configuration in NVS
    // -------------------------------------------------------------------------
    else if (strcmp(cmd->valuestring, "SAVE_CONFIG") == 0) {
        esp_err_t err = saveConfigToNvs();
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "type", "config_saved");
        cJSON_AddBoolToObject  (resp, "ok",   (err == ESP_OK));
        wsSendJson(req, resp);
        cJSON_Delete(resp);
    }

    // -------------------------------------------------------------------------
    //  RESET_COUNTS
    // -------------------------------------------------------------------------
    else if (strcmp(cmd->valuestring, "RESET_COUNTS") == 0) {
        if (s_configQueue) {
            AppConfigCmd cfgCmd { ConfigCmdType::RESET_COUNTS, 0.0f };
            xQueueSend(s_configQueue, &cfgCmd, 0);
        }
        ESP_LOGI(TAG, "RESET_COUNTS requested");
    }

    // -------------------------------------------------------------------------
    //  RETRY_SENSOR
    // -------------------------------------------------------------------------
    else if (strcmp(cmd->valuestring, "RETRY_SENSOR") == 0) {
        if (s_configQueue) {
            AppConfigCmd cfgCmd { ConfigCmdType::RETRY_SENSOR, 0.0f };
            xQueueSend(s_configQueue, &cfgCmd, 0);
        }
        ESP_LOGI(TAG, "RETRY_SENSOR requested");
    }

    cJSON_Delete(root);
}

// =============================================================================
//  WEBSOCKET URI HANDLER
// =============================================================================

esp_err_t HttpServer::wsHandler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "New WebSocket connection");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    if (ws_pkt.len > 0) {
        buf = (uint8_t *)calloc(1, ws_pkt.len + 1);
        if (!buf) return ESP_ERR_NO_MEM;
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret == ESP_OK) {
            handleWebSocketMessage(req, &ws_pkt);
        }
        free(buf);
    }
    return ret;
}

// =============================================================================
//  START / STOP
// =============================================================================

esp_err_t HttpServer::start(QueueHandle_t configQueue) {
    if (server_ != NULL) return ESP_ERR_INVALID_STATE;

    s_configQueue = configQueue;

    // Load saved parameters from NVS before starting (NVS already init'd in main)
    loadConfigFromNvs();

    httpd_config_t config      = HTTPD_DEFAULT_CONFIG();
    config.core_id             = 0;   // PRO_CPU (Core 0)
    config.max_open_sockets    = 7;    // Increase from 4 to 7 (ESP-IDF default) for better connection handling
    config.stack_size          = 16384;  // Increased for OTA: firmware writes consume extra stack

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    esp_err_t ret = httpd_start(&server_, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error starting server: %s", esp_err_to_name(ret));
        return ret;
    }

    const httpd_uri_t uri_get = {
        .uri                   = "/",
        .method                = HTTP_GET,
        .handler               = indexGetHandler,
        .user_ctx              = NULL,
        .is_websocket          = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server_, &uri_get);

    const httpd_uri_t css_get = {
        .uri                   = "/style.css",
        .method                = HTTP_GET,
        .handler               = styleGetHandler,
        .user_ctx              = NULL,
        .is_websocket          = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server_, &css_get);

    const httpd_uri_t js_get = {
        .uri                   = "/app.js",
        .method                = HTTP_GET,
        .handler               = appJsGetHandler,
        .user_ctx              = NULL,
        .is_websocket          = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server_, &js_get);

    const httpd_uri_t ws_uri = {
        .uri                   = "/ws",
        .method                = HTTP_GET,
        .handler               = wsHandler,
        .user_ctx              = NULL,
        .is_websocket          = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server_, &ws_uri);

    // OTA endpoint registration
    httpd_uri_t ota_uri = {
        .uri      = "/update",
        .method   = HTTP_POST,
        .handler  = HttpServer::otaPostHandler,
        .user_ctx = nullptr,
        .is_websocket = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server_, &ota_uri);

    // REBOOT endpoint registration
    httpd_uri_t reboot_uri = {
        .uri      = "/reboot",
        .method   = HTTP_POST,
        .handler  = [](httpd_req_t *req) {
            httpd_resp_sendstr(req, "Rebooting...");
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
            return ESP_OK;
        },
        .user_ctx = nullptr,
        .is_websocket = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server_, &reboot_uri);

    ESP_LOGI(TAG, "Web Server started with WS, OTA, and Reboot");

    return ESP_OK;
}

void HttpServer::stop() {
    if (server_) {
        httpd_stop(server_);
        server_ = NULL;
    }
}
void HttpServer::broadcastFrame(const ImagePayload& img, const TelemetryPayload& tel, bool sensor_ok)
{
    if (!server_) return;

    // Optimization: Send every frame to browser (S3 has plenty of BW)
    // if (tel.frame_id % 2 != 0) return;

    // 1. Find a free buffer (ref_count == 0)
    int buf_idx = -1;
    portENTER_CRITICAL(&s_ws_mux);
    for (int i = 0; i < (int)WS_BUFFER_COUNT; i++) {
        if (ws_buffer_ref_counts_[i] == 0) {
            ws_buffer_ref_counts_[i] = -1; // Marked as "filling" (reserved)
            buf_idx = i;
            break;
        }
    }
    portEXIT_CRITICAL(&s_ws_mux);

    if (buf_idx == -1) {
        // All buffers busy (slow network), skipping this frame
        return;
    }

    // 2. Fill the reserved buffer
    uint8_t *p   = ws_buffers_[buf_idx];
    size_t   ofs = 0;

    p[ofs++] = 0x11;                             // Header
    p[ofs++] = sensor_ok ? 1u : 0u;              // SensorOk
    memcpy(&p[ofs], &tel.ambient_temp, 4);       // Ambient Temp
    ofs += 4;
    p[ofs++] = (uint8_t)(tel.count_in  & 0xFF);  // CountIn
    p[ofs++] = (uint8_t)(tel.count_in  >> 8);
    p[ofs++] = (uint8_t)(tel.count_out & 0xFF);  // CountOut
    p[ofs++] = (uint8_t)(tel.count_out >> 8);
    p[ofs++] = tel.num_tracks;                   // Tracks
    for (int i = 0; i < tel.num_tracks && i < ThermalConfig::MAX_TRACKS; i++) {
        p[ofs++] = tel.tracks[i].id;
        p[ofs++] = (uint8_t)(tel.tracks[i].x_100 & 0xFF);
        p[ofs++] = (uint8_t)(tel.tracks[i].x_100 >> 8);
        p[ofs++] = (uint8_t)(tel.tracks[i].y_100 & 0xFF);
        p[ofs++] = (uint8_t)(tel.tracks[i].y_100 >> 8);
        p[ofs++] = (uint8_t)(tel.tracks[i].v_x_100 & 0xFF);
        p[ofs++] = (uint8_t)(tel.tracks[i].v_x_100 >> 8);
        p[ofs++] = (uint8_t)(tel.tracks[i].v_y_100 & 0xFF);
        p[ofs++] = (uint8_t)(tel.tracks[i].v_y_100 >> 8);
    }
    memcpy(&p[ofs], img.pixels, ThermalConfig::TOTAL_PIXELS * sizeof(int16_t));
    const size_t total_len = ofs + (ThermalConfig::TOTAL_PIXELS * sizeof(int16_t));

    // 3. Get client list and prepare transmission
    size_t clients_max = 4;
    int    client_fds[4];
    size_t clients = clients_max;
    int    ws_clients_count = 0;

    if (httpd_get_client_list(server_, &clients, client_fds) == ESP_OK) {
        // Count how many are actually WebSockets
        for (size_t i = 0; i < clients; i++) {
            if (httpd_ws_get_fd_info(server_, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
                ws_clients_count++;
            }
        }
    }

    if (ws_clients_count == 0) {
        portENTER_CRITICAL(&s_ws_mux);
        ws_buffer_ref_counts_[buf_idx] = 0; // Release immediately
        portEXIT_CRITICAL(&s_ws_mux);
        return;
    }

    // 4. Start asynchronous transmissions
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type    = HTTPD_WS_TYPE_BINARY;
    ws_pkt.payload = p;
    ws_pkt.len     = total_len;
    ws_pkt.final   = true;

    portENTER_CRITICAL(&s_ws_mux);
    ws_buffer_ref_counts_[buf_idx] = ws_clients_count; 
    portEXIT_CRITICAL(&s_ws_mux);

    // 5. Send to all WS clients
    size_t successful_async_sends = 0;
    for (size_t i = 0; i < clients; i++) {
        if (httpd_ws_get_fd_info(server_, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
            // Pass buffer index as argument (cast to void*)
            esp_err_t err = httpd_ws_send_data_async(server_, client_fds[i], &ws_pkt, 
                                                    wsAsyncCompletionCb, (void*)(uintptr_t)buf_idx);
            if (err == ESP_OK) {
                successful_async_sends++;
            } else {
                // Rate-limited logging for Error 11 (EAGAIN) to avoid console flooding during disconnections
                static uint32_t last_error_log_frame = 0;
                if (tel.frame_id - last_error_log_frame > 80) { // Every ~10 seconds at 8Hz delivery
                    ESP_LOGW(TAG, "Failed to queue async WS send to fd %d: %s (Check if client disconnected)", 
                             client_fds[i], esp_err_to_name(err));
                    last_error_log_frame = tel.frame_id;
                }
            }
        }
    }

    // 6. Final Adjustment: If no sends were successfully queued, release buffer immediately
    if (successful_async_sends == 0) {
        portENTER_CRITICAL(&s_ws_mux);
        ws_buffer_ref_counts_[buf_idx] = 0;
        portEXIT_CRITICAL(&s_ws_mux);
    } else if (successful_async_sends < ws_clients_count) {
        // Some sends failed to queue, adjust ref count so it eventualy reaches 0
        portENTER_CRITICAL(&s_ws_mux);
        ws_buffer_ref_counts_[buf_idx] -= (ws_clients_count - successful_async_sends);
        if (ws_buffer_ref_counts_[buf_idx] < 0) ws_buffer_ref_counts_[buf_idx] = 0;
        portEXIT_CRITICAL(&s_ws_mux);
    }
}

void HttpServer::wsAsyncCompletionCb(esp_err_t err, int socket, void *arg) {
    int buf_idx = (int)(uintptr_t)arg;
    if (buf_idx < 0 || buf_idx >= (int)WS_BUFFER_COUNT) return;

    portENTER_CRITICAL(&s_ws_mux);
    if (ws_buffer_ref_counts_[buf_idx] > 0) {
        ws_buffer_ref_counts_[buf_idx]--;
    }
    portEXIT_CRITICAL(&s_ws_mux);

    if (err != ESP_OK && err != ESP_ERR_HTTPD_INVALID_REQ) {
        ESP_LOGV(TAG, "wsAsync: Error fd %d: %s", socket, esp_err_to_name(err));
    }
}
