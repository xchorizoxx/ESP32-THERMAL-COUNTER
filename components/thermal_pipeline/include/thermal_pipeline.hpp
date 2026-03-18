#pragma once
/**
 * @file thermal_pipeline.hpp
 * @brief Orquestador del pipeline de visión térmica (Core 1).
 *
 * Ejecuta los 5 pasos del pipeline a 16 Hz usando vTaskDelayUntil,
 * gestiona el Watchdog y envía resultados al Core 0 vía FreeRTOS Queue.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "mlx90640_sensor.hpp"
#include "thermal_types.hpp"
#include "background_model.hpp"
#include "peak_detector.hpp"
#include "nms_suppressor.hpp"
#include "alpha_beta_tracker.hpp"
#include "mask_generator.hpp"
#include "thermal_config.hpp"

class ThermalPipeline {
public:
    /**
     * @brief Constructor.
     * @param sensor      Referencia al sensor ya inicializado
     * @param ipcQueue    Queue hacia Core 0 (tamaño IPC_QUEUE_DEPTH × sizeof(IpcPacket))
     * @param configQueue Queue desde Core 0 para recibir comandos de configuración
     */
    ThermalPipeline(Mlx90640Sensor& sensor, QueueHandle_t ipcQueue, QueueHandle_t configQueue);

    /**
     * @brief Inicializa la tarea: configura Watchdog.
     * Llamar ANTES de xTaskCreatePinnedToCore.
     */
    void init();

    /**
     * @brief Wrapper estático para FreeRTOS (pasa `this` como parámetro).
     * Usar en xTaskCreatePinnedToCore:
     *   xTaskCreatePinnedToCore(ThermalPipeline::TaskWrapper, ..., &pipeline, ...);
     */
    static void TaskWrapper(void* pvParameters);

private:
    /**
     * @brief Bucle principal del pipeline. Nunca retorna.
     * Tempo: vTaskDelayUntil a 1000/PIPELINE_FREQ_HZ ms.
     */
    void run();

    Mlx90640Sensor&  sensor_;
    QueueHandle_t    ipcQueue_;
    QueueHandle_t    configQueue_;
    AlphaBetaTracker tracker_;

    // ---------- Buffers estáticos (sin malloc en runtime) ----------
    float   frame_actual_[ThermalConfig::TOTAL_PIXELS];
    float   frame_display_[ThermalConfig::TOTAL_PIXELS];  ///< EMA temporal solo para visualización
    float   mapa_fondo_[ThermalConfig::TOTAL_PIXELS];
    uint8_t mascara_bloqueo_[ThermalConfig::TOTAL_PIXELS];
    PicoTermico objetivos_crudos_[ThermalConfig::MAX_OBJETIVOS];

    int      num_objetivos_  = 0;
    int      count_in_       = 0;
    int      count_out_      = 0;
    uint32_t frame_id_       = 0;
    bool     fondoInit_      = false; // si el fondo ya fue inicializado con el 1er frame
    bool     displayInit_    = false; // si el buffer de display fue inicializado
    bool     sensor_initialized_ = false;
};
