/**
 * @file thermal_pipeline.cpp
 * @brief Implementación del orquestador del pipeline de visión térmica.
 *
 * Ejecuta los 5 pasos a 16 Hz en Core 1 con vTaskDelayUntil,
 * y despacha resultados al Core 0 vía FreeRTOS Queue.
 */

#include "thermal_pipeline.hpp"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include <cstring>

namespace ThermalConfig {
    float EMA_ALPHA = 0.05f;
    float TEMP_BIOLOGICO_MIN = 25.0f;
    float DELTA_T_FONDO = 1.5f;
    int NMS_RADIUS_CENTER_SQ = 16;
    int NMS_RADIUS_EDGE_SQ = 4;
    int DEFAULT_LINE_ENTRY_Y = 11;
    int DEFAULT_LINE_EXIT_Y = 13;
    int VIEW_MODE = 0;
    bool APP_RESET_COUNTS = false;
    bool APP_RETRY_SENSOR = false;
}

static const char* TAG = "PIPELINE";

ThermalPipeline::ThermalPipeline(Mlx90640Sensor& sensor, QueueHandle_t ipcQueue, QueueHandle_t configQueue)
    : sensor_(sensor)
    , ipcQueue_(ipcQueue)
    , configQueue_(configQueue)
    , tracker_()
    , num_objetivos_(0)
    , count_in_(0)
    , count_out_(0)
    , frame_id_(0)
    , fondoInit_(false)
    , sensor_initialized_(false)
{
    memset(frame_actual_,    0, sizeof(frame_actual_));
    memset(frame_display_,   0, sizeof(frame_display_));
    memset(mapa_fondo_,      0, sizeof(mapa_fondo_));
    memset(mascara_bloqueo_, 0, sizeof(mascara_bloqueo_));
    memset(objetivos_crudos_, 0, sizeof(objetivos_crudos_));
}

void ThermalPipeline::init()
{
    ESP_LOGI(TAG, "Pipeline inicializado (target: %d Hz, stack estático: %.1f KB)",
             ThermalConfig::PIPELINE_FREQ_HZ,
             (float)(sizeof(frame_actual_) + sizeof(mapa_fondo_) +
                     sizeof(mascara_bloqueo_)) / 1024.0f);
}

void ThermalPipeline::TaskWrapper(void* pvParameters)
{
    auto* self = static_cast<ThermalPipeline*>(pvParameters);

    // Registrar esta tarea en el Watchdog del sistema
    esp_task_wdt_add(NULL);

    self->run();

    // No debería llegar aquí nunca
    vTaskDelete(NULL);
}

void ThermalPipeline::run()
{
    const TickType_t period = pdMS_TO_TICKS(1000 / ThermalConfig::PIPELINE_FREQ_HZ); // 62 ms
    TickType_t lastWakeTime = xTaskGetTickCount();

    ESP_LOGI(TAG, "Pipeline arrancado en Core %d a %d Hz",
             xPortGetCoreID(), ThermalConfig::PIPELINE_FREQ_HZ);

    // Initialize local state based on actual sensor state at boot
    sensor_initialized_ = sensor_.isInitialized();

    while (true) {
        // ============================================================
        // PASO 0.1: Procesar comandos de configuración desde UI (Thread-Safe)
        // ============================================================
        AppConfigCmd cmd;
        while (configQueue_ && xQueueReceive(configQueue_, &cmd, 0) == pdTRUE) {
            switch (cmd.type) {
                case ConfigCmdType::SET_TEMP_BIO:    ThermalConfig::TEMP_BIOLOGICO_MIN = cmd.value; break;
                case ConfigCmdType::SET_DELTA_T:     ThermalConfig::DELTA_T_FONDO = cmd.value; break;
                case ConfigCmdType::SET_EMA_ALPHA:   ThermalConfig::EMA_ALPHA = cmd.value; break;
                case ConfigCmdType::SET_LINE_ENTRY:  ThermalConfig::DEFAULT_LINE_ENTRY_Y = (int)cmd.value; break;
                case ConfigCmdType::SET_LINE_EXIT:   ThermalConfig::DEFAULT_LINE_EXIT_Y = (int)cmd.value; break;
                case ConfigCmdType::SET_NMS_CENTER:  ThermalConfig::NMS_RADIUS_CENTER_SQ = (int)cmd.value; break;
                case ConfigCmdType::SET_NMS_EDGE:    ThermalConfig::NMS_RADIUS_EDGE_SQ = (int)cmd.value; break;
                case ConfigCmdType::SET_VIEW_MODE:   ThermalConfig::VIEW_MODE = (int)cmd.value; break;
                case ConfigCmdType::RESET_COUNTS:
                    count_in_ = 0;
                    count_out_ = 0;
                    ESP_LOGI(TAG, "Contadores reseteados");
                    break;
                case ConfigCmdType::RETRY_SENSOR:
                    ESP_LOGI(TAG, "Intentando inicializar sensor por petición web...");
                    if (sensor_.init() == ESP_OK && sensor_.setRefreshRate(0x05) == ESP_OK) {
                        sensor_initialized_ = true;
                        ESP_LOGI(TAG, "Sensor inicializado correctamente");
                    } else {
                        sensor_initialized_ = false;
                        ESP_LOGE(TAG, "Fallo al inicializar sensor");
                    }
                    break;
                case ConfigCmdType::SAVE_CONFIG:   // Handled in Core 0 (http_server) — no-op here
                case ConfigCmdType::APPLY_CONFIG:  // Handled in Core 0 (http_server) — no-op here
                    break;
            }
        }

        // ============================================================
        // PASO 0: Adquisición del frame desde el sensor
        // ============================================================
        bool sensor_ok = false;
        if (sensor_initialized_) {
            esp_err_t readErr = sensor_.readFrame(frame_actual_);
            if (readErr == ESP_OK) {
                sensor_ok = true;
            } else {
                ESP_LOGW(TAG, "Frame %lu: error de lectura (probablemente sensor desconectado)", frame_id_);
                sensor_initialized_ = false; // Fuerza requerir un nuevo init
            }
        }

        if (sensor_ok) {

            // ============================================================
            // PASO 0.5: Filtro Espacial (Ruido Salt & Pepper) y Estabilidad Uniforme
            // ============================================================
            float max_temp = -100.0f;
            float min_temp = 100.0f;
            
            // Temporary buffer to hold filtered pixels so we don't bleed values
            static float filtered_frame[ThermalConfig::TOTAL_PIXELS];
            
            for (int r = 0; r < ThermalConfig::MLX_ROWS; r++) {
                for (int c = 0; c < ThermalConfig::MLX_COLS; c++) {
                    int idx = r * ThermalConfig::MLX_COLS + c;
                    float val = frame_actual_[idx];
                    
                    if (val > max_temp) max_temp = val;
                    if (val < min_temp) min_temp = val;

                    // --- RECONSTRUCCIÓN ESPACIAL (Interpolación para evitar Ajedrez) ---
                    // Determinamos si este píxel pertenece a la subpágina que acaba de llegar.
                    // En Chess Mode: (r+c)%2 == lastSubPageID
                    bool is_new_pixel = ((r + c) % 2) == sensor_.getLastSubPageID();

                    if (!is_new_pixel) {
                        // Es un píxel de la subpágina "vieja". Lo estimamos promediando sus 4 vecinos "nuevos".
                        if (r > 0 && r < ThermalConfig::MLX_ROWS - 1 && c > 0 && c < ThermalConfig::MLX_COLS - 1) {
                            float up    = frame_actual_[(r-1) * ThermalConfig::MLX_COLS + c];
                            float down  = frame_actual_[(r+1) * ThermalConfig::MLX_COLS + c];
                            float left  = frame_actual_[r * ThermalConfig::MLX_COLS + (c-1)];
                            float right = frame_actual_[r * ThermalConfig::MLX_COLS + (c+1)];
                            val = (up + down + left + right) / 4.0f;
                        }
                    } else {
                        // Es un píxel "nuevo". Aplicamos Filtro de Outliers (Salt & Pepper)
                        if (r > 0 && r < ThermalConfig::MLX_ROWS - 1 && c > 0 && c < ThermalConfig::MLX_COLS - 1) {
                            float up    = frame_actual_[(r-1) * ThermalConfig::MLX_COLS + c];
                            float down  = frame_actual_[(r+1) * ThermalConfig::MLX_COLS + c];
                            float left  = frame_actual_[r * ThermalConfig::MLX_COLS + (c-1)];
                            float right = frame_actual_[r * ThermalConfig::MLX_COLS + (c+1)];
                            float avg_neighbors = (up + down + left + right) / 4.0f;
                            
                            if (val > avg_neighbors + 2.5f || val < avg_neighbors - 2.5f) {
                                val = avg_neighbors; 
                            }
                        }
                    }
                    filtered_frame[idx] = val;
                }
            }
            
            // Apply filtered values back
            for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
                frame_actual_[i] = filtered_frame[i];
            }

            // ============================================================
            // PASO 0.6: Imagen para Display (Sin filtros temporales para evitar blur)
            // ============================================================
            // Copiar directamente el frame actual al de display para máxima nitidez en movimiento.
            memcpy(frame_display_, frame_actual_, sizeof(frame_display_));

            // ============================================================
            // PASO 1: Fondo Dinámico — EMA Selectiva
            // ============================================================
            if (!fondoInit_) {
                // Primer frame: inicializar el fondo con los datos crudos
                BackgroundModel::initialize(frame_actual_, mapa_fondo_,
                                            ThermalConfig::TOTAL_PIXELS);
                fondoInit_ = true;
                ESP_LOGI(TAG, "Mapa de fondo inicializado con el primer frame");
            } else {
                BackgroundModel::update(frame_actual_, mapa_fondo_, mascara_bloqueo_,
                                        ThermalConfig::TOTAL_PIXELS, ThermalConfig::EMA_ALPHA);
            }
            
            // Check for completely uniform surface noise
            bool is_uniform = (max_temp - min_temp) < 1.0f;

            // ============================================================
            // PASO 2: Topología — Detección de Picos
            // ============================================================
            num_objetivos_ = 0;
            if (!is_uniform) {
                PeakDetector::detect(frame_actual_, mapa_fondo_,
                                     objetivos_crudos_, &num_objetivos_,
                                     ThermalConfig::TEMP_BIOLOGICO_MIN,
                                     ThermalConfig::DELTA_T_FONDO,
                                     ThermalConfig::MAX_OBJETIVOS);
            }

            // ============================================================
            // PASO 3: NMS Adaptativa
            // ============================================================
            NmsSuppressor::suppress(objetivos_crudos_, num_objetivos_,
                                    ThermalConfig::NMS_RADIUS_CENTER_SQ,
                                    ThermalConfig::NMS_RADIUS_EDGE_SQ,
                                    ThermalConfig::NMS_CENTER_X_MIN,
                                    ThermalConfig::NMS_CENTER_X_MAX);

            // ============================================================
            // PASO 4: Tracking Alpha-Beta + Conteo
            // ============================================================
            tracker_.update(objetivos_crudos_, num_objetivos_,
                            ThermalConfig::ALPHA_TRK, ThermalConfig::BETA_TRK,
                            ThermalConfig::MAX_MATCH_DIST_SQ, ThermalConfig::TRACK_MAX_AGE,
                            ThermalConfig::DEFAULT_LINE_ENTRY_Y,
                            ThermalConfig::DEFAULT_LINE_EXIT_Y,
                            count_in_, count_out_);

            // ============================================================
            // PASO 5: Retroalimentación — Generación de Máscara
            // ============================================================
            MaskGenerator::generate(tracker_.getTracks(), tracker_.getMaxTracks(),
                                    mascara_bloqueo_, ThermalConfig::MASK_HALF_SIZE);
        }

        // ============================================================
        // DESPACHO: Construir y enviar IpcPacket al Core 0
        // ============================================================
        static IpcPacket packet;

        // --- Telemetría ---
        packet.sensor_ok             = sensor_ok;
        packet.telemetria.ambient_temp = sensor_.getAmbientTemp();
        packet.telemetria.frame_id   = frame_id_;
        packet.telemetria.count_in   = (int16_t)count_in_;
        packet.telemetria.count_out  = (int16_t)count_out_;
        packet.telemetria.num_tracks = (uint8_t)(sensor_ok ? tracker_.getActiveCount() : 0);

        const Track* tracks = tracker_.getTracks();
        int tidx = 0;
        for (int i = 0; i < tracker_.getMaxTracks() && tidx < ThermalConfig::MAX_TRACKS; i++) {
            if (tracks[i].activo) {
                packet.telemetria.tracks[tidx].id      = tracks[i].id;
                packet.telemetria.tracks[tidx].x_100   = (int16_t)(tracks[i].x * 100.0f);
                packet.telemetria.tracks[tidx].y_100   = (int16_t)(tracks[i].y * 100.0f);
                packet.telemetria.tracks[tidx].v_x_100 = (int16_t)(tracks[i].v_x * 100.0f);
                packet.telemetria.tracks[tidx].v_y_100 = (int16_t)(tracks[i].v_y * 100.0f);
                tidx++;
            }
        }

        // --- Imagen Térmica (pixels como int16 × 100) ---
        packet.imagen.frame_id = frame_id_;
        if (ThermalConfig::VIEW_MODE == 1) { // Modo Sustractor
            for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
                float diff = frame_actual_[i] - mapa_fondo_[i];
                // Noise Gate: Ignorar ruido térmico menor a 0.6°C
                if (diff < 0.6f && diff > -0.6f) diff = 0.0f;
                packet.imagen.pixels[i] = (int16_t)(diff * 100.0f);
            }
        } else { // Modo Normal
            for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
                float val = frame_display_[i];
                float bg = mapa_fondo_[i];
                // Noise Gate en modo normal: Si la diferencia con el fondo es mínima, mostrar fondo puro
                if (val - bg < 0.4f && val - bg > -0.4f) val = bg; 
                packet.imagen.pixels[i] = (int16_t)(val * 100.0f);
            }
        }

        // Envío no-bloqueante: si la queue está llena, descartar frame
        BaseType_t sent = xQueueSend(ipcQueue_, &packet, 0);
        if (sent != pdTRUE) {
            ESP_LOGW(TAG, "Frame %lu: Queue IPC llena, frame descartado", frame_id_);
        }

        ESP_LOGD(TAG, "Frame %lu: %d picos, %d tracks, IN=%d, OUT=%d",
                 frame_id_, num_objetivos_, tracker_.getActiveCount(),
                 count_in_, count_out_);

        frame_id_++;

        // ============================================================
        // FIN DE CICLO: Reset WDT + espera determinista
        // ============================================================
        esp_task_wdt_reset();
        vTaskDelayUntil(&lastWakeTime, period);
    }
}
