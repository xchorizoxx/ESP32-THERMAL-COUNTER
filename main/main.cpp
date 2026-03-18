/**
 * @file main.cpp
 * @brief Punto de entrada del sistema de conteo térmico.
 *
 * Inicializa hardware, crea objetos estáticos, lanza las tareas
 * FreeRTOS en sus núcleos asignados y retorna.
 *
 * Core 1 (APP_CPU): ThermalPipeline — Motor de visión a 16 Hz
 * Core 0 (PRO_CPU): TelemetryTask  — SoftAP + UDP broadcast
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_ota_ops.h"       // [OTA] Para marcar la app como válida tras boot exitoso

// Componentes propios
#include "thermal_config.hpp"
#include "thermal_types.hpp"
#include "mlx90640_sensor.hpp"
#include "thermal_pipeline.hpp"
#include "wifi_softap.hpp"
#include "udp_transmitter.hpp"
#include "telemetry_task.hpp"
#include "http_server.hpp" // [NEW]

static const char* TAG = "MAIN";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== Sistema de Conteo Térmico iniciando ===");

    // -------------------------------------------------------------------------
    // Paso 1: NVS (requerido por el stack WiFi)
    // -------------------------------------------------------------------------
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrupto, borrando y reiniciando...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // -------------------------------------------------------------------------
    // Paso 2: Configurar Watchdog del sistema
    // -------------------------------------------------------------------------
    esp_task_wdt_config_t wdtCfg = {
        .timeout_ms     = 5000,   // 5 segundos
        .idle_core_mask = 0,      // No vigilar tareas idle
        .trigger_panic  = true    // Reiniciar en caso de WDT trigger
    };
    ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&wdtCfg));

    // -------------------------------------------------------------------------
    // Paso 3: WiFi SoftAP
    // -------------------------------------------------------------------------
    ESP_LOGI(TAG, "Iniciando SoftAP '%s'...", ThermalConfig::SOFTAP_SSID);
    ret = WifiSoftAp::init(ThermalConfig::SOFTAP_SSID,
                           ThermalConfig::SOFTAP_PASS,
                           ThermalConfig::SOFTAP_CHANNEL,
                           ThermalConfig::SOFTAP_MAX_CONN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FALLO al iniciar SoftAP — abortando");
        return;
    }

    // -------------------------------------------------------------------------
    // Paso 4: Sensor MLX90640
    // -------------------------------------------------------------------------
    static Mlx90640Sensor sensor(
        I2C_NUM_0,
        (gpio_num_t)ThermalConfig::I2C_SDA_PIN,   // GPIO 8
        (gpio_num_t)ThermalConfig::I2C_SCL_PIN,   // GPIO 9
        ThermalConfig::MLX_ADDR                   // 0x33
    );

    // Provide a short boot delay for the MLX90640 to be ready before calling init()
    vTaskDelay(pdMS_TO_TICKS(150));

    ret = sensor.init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FALLO al inicializar sensor MLX90640 (¿está conectado?)");
        // Continuamos para permitir que arranque el panel Web y muestre el error
    } else {
        ret = sensor.setRefreshRate(0x05); // 16 Hz
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "FALLO al configurar refresh rate");
        }
    }

    // -------------------------------------------------------------------------
    // Paso 5: Queue IPC estática (Core 1 → Core 0)
    // -------------------------------------------------------------------------
    static StaticQueue_t queueBuffer;
    static uint8_t queueStorage[ThermalConfig::IPC_QUEUE_DEPTH * sizeof(IpcPacket)];

    QueueHandle_t ipcQueue = xQueueCreateStatic(
        ThermalConfig::IPC_QUEUE_DEPTH,
        sizeof(IpcPacket),
        queueStorage,
        &queueBuffer
    );
    if (ipcQueue == NULL) {
        ESP_LOGE(TAG, "FALLO al crear IPC Queue — abortando");
        return;
    }
    ESP_LOGI(TAG, "Queue IPC creada (%d slots × %zu bytes = %zu bytes)",
             ThermalConfig::IPC_QUEUE_DEPTH,
             sizeof(IpcPacket),
             ThermalConfig::IPC_QUEUE_DEPTH * sizeof(IpcPacket));

    // -------------------------------------------------------------------------
    // Paso 5.1: Queue de Comandos de Configuración (Core 0 -> Core 1)
    // -------------------------------------------------------------------------
    QueueHandle_t configQueue = xQueueCreate(10, sizeof(AppConfigCmd));
    if (configQueue == NULL) {
        ESP_LOGE(TAG, "FALLO al crear Config Queue — abortando");
        return;
    }

    // -------------------------------------------------------------------------
    // Paso 6: UDP Transmitter
    // -------------------------------------------------------------------------
    static UdpTransmitter udp(
        ThermalConfig::UDP_BROADCAST_IP,
        ThermalConfig::UDP_PORT
    );
    ret = udp.init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FALLO al inicializar UDP Transmitter — abortando");
        return;
    }

    // -------------------------------------------------------------------------
    // Paso 7: Pipeline de Visión Térmica (Core 1 - APP_CPU)
    // -------------------------------------------------------------------------
    static ThermalPipeline pipeline(sensor, ipcQueue, configQueue);
    pipeline.init();

    static StaticTask_t pipelineTaskBuffer;
    static StackType_t  pipelineStack[8192 / sizeof(StackType_t)];

    TaskHandle_t pipelineHandle = xTaskCreateStaticPinnedToCore(
        ThermalPipeline::TaskWrapper,   // Función
        "ThermalPipe",                  // Nombre
        8192 / sizeof(StackType_t),     // Stack (en words)
        &pipeline,                      // Parámetro (this)
        configMAX_PRIORITIES - 1,       // Prioridad máxima
        pipelineStack,
        &pipelineTaskBuffer,
        1                               // Core 1 (APP_CPU)
    );
    if (pipelineHandle == NULL) {
        ESP_LOGE(TAG, "FALLO al crear tarea ThermalPipeline");
        return;
    }
    ESP_LOGI(TAG, "ThermalPipeline lanzado en Core 1 (prioridad máx)");

    // -------------------------------------------------------------------------
    // Paso 7.5: Iniciar Web Server (Core 0)
    // -------------------------------------------------------------------------
    ret = HttpServer::start(configQueue);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Aviso: No se pudo iniciar el servidor web HTTP");
    }

    // -------------------------------------------------------------------------
    // Paso 8: Telemetría (Core 0 - PRO_CPU)
    // -------------------------------------------------------------------------
    static TelemetryTask telemetry(ipcQueue, udp);
    telemetry.init();

    static StaticTask_t telemetryTaskBuffer;
    static StackType_t  telemetryStack[4096 / sizeof(StackType_t)];

    TaskHandle_t telemetryHandle = xTaskCreateStaticPinnedToCore(
        TelemetryTask::TaskWrapper,
        "TelemetryTX",
        4096 / sizeof(StackType_t),
        &telemetry,
        tskIDLE_PRIORITY + 2,           // Prioridad media
        telemetryStack,
        &telemetryTaskBuffer,
        0                               // Core 0 (PRO_CPU)
    );
    if (telemetryHandle == NULL) {
        ESP_LOGE(TAG, "FALLO al crear tarea TelemetryTask");
        return;
    }
    ESP_LOGI(TAG, "TelemetryTask lanzada en Core 0 (prioridad media)");

    ESP_LOGI(TAG, "=== Sistema operativo. Tareas activas ===");

    // -------------------------------------------------------------------------
    // [OTA] Marcar el firmware actual como VÁLIDO (anti-bootloop)
    // -------------------------------------------------------------------------
    // Si llegamos aquí, WiFi + Sensor + HTTP Server + Tasks están todos activos.
    // Esta llamada transiciona el estado OTA de PENDING_VERIFY → VALID,
    // evitando que el bootloader haga rollback al factory en el próximo arranque.
    esp_err_t ota_valid = esp_ota_mark_app_valid_cancel_rollback();
    if (ota_valid == ESP_OK) {
        ESP_LOGI(TAG, "[OTA] Partición marcada como VÁLIDA — rollback cancelado");
    } else if (ota_valid == ESP_ERR_NOT_SUPPORTED) {
        // Ocurre cuando la app arranca desde factory (no desde ota_0/ota_1).
        // Es normal en el primer arranque por USB — no es un error.
        ESP_LOGI(TAG, "[OTA] Partición factory detectada — no se requiere marcado OTA");
    } else {
        ESP_LOGW(TAG, "[OTA] esp_ota_mark_app_valid falló: %s", esp_err_to_name(ota_valid));
    }

    // app_main() retorna aquí; las tareas FreeRTOS corren de forma autónoma
}
