/**
 * @file main.cpp
 * @brief System entry point for thermal counting.
 *
 * Initializes hardware, creates static objects, launches FreeRTOS tasks
 * on their assigned cores and returns.
 *
 * Core 1 (APP_CPU): ThermalPipeline — Vision Engine @ 16 Hz
 * Core 0 (PRO_CPU): TelemetryTask  — SoftAP + UDP broadcast
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_ota_ops.h"       // [OTA] To mark the app as valid after successful boot

// Custom components
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
    ESP_LOGI(TAG, "=== Thermal Counting System initializing ===");

    // -------------------------------------------------------------------------
    // Step 1: NVS (required by WiFi stack)
    // -------------------------------------------------------------------------
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrupt, erasing and restarting...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // -------------------------------------------------------------------------
    // Step 2: Configure system Watchdog
    // -------------------------------------------------------------------------
    esp_task_wdt_config_t wdtCfg = {
        .timeout_ms     = 5000,   // 5 seconds
        .idle_core_mask = 0,      // Do not monitor idle tasks
        .trigger_panic  = true    // Restart in case of WDT trigger
    };
    ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&wdtCfg));

    // -------------------------------------------------------------------------
    // Step 3: WiFi SoftAP
    // -------------------------------------------------------------------------
    ESP_LOGI(TAG, "Starting SoftAP '%s'...", ThermalConfig::SOFTAP_SSID);
    ret = WifiSoftAp::init(ThermalConfig::SOFTAP_SSID,
                           ThermalConfig::SOFTAP_PASS,
                           ThermalConfig::SOFTAP_CHANNEL,
                           ThermalConfig::SOFTAP_MAX_CONN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAILED to start SoftAP — aborting");
        return;
    }

    // -------------------------------------------------------------------------
    // Step 4: MLX90640 Sensor
    // -------------------------------------------------------------------------
    static Mlx90640Sensor sensor(
        I2C_NUM_0,
        (gpio_num_t)ThermalConfig::I2C_SDA_PIN,   // GPIO 8
        (gpio_num_t)ThermalConfig::I2C_SCL_PIN,   // GPIO 9
        ThermalConfig::I2C_FREQ_HZ,
        ThermalConfig::MLX_ADDR                   // 0x33
    );

    // Provide a short boot delay for the MLX90640 to be ready before calling init()
    vTaskDelay(pdMS_TO_TICKS(150));

    ret = sensor.init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAILED to initialize MLX90640 sensor (is it connected?)");
        // Continuing to allow Web panel to start and show the error
    } else {
        ret = sensor.setRefreshRate(0x05); // 16 Hz
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "FAILED to configure refresh rate");
        }
    }

    // -------------------------------------------------------------------------
    // Step 5: Static IPC Queue (Core 1 → Core 0)
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
        ESP_LOGE(TAG, "FAILED to create IPC Queue — aborting");
        return;
    }
    ESP_LOGI(TAG, "IPC Queue created (%d slots × %zu bytes = %zu bytes)",
             ThermalConfig::IPC_QUEUE_DEPTH,
             sizeof(IpcPacket),
             ThermalConfig::IPC_QUEUE_DEPTH * sizeof(IpcPacket));

    // -------------------------------------------------------------------------
    // Step 5.1: Configuration Command Queue (Core 0 -> Core 1)
    // -------------------------------------------------------------------------
    QueueHandle_t configQueue = xQueueCreate(10, sizeof(AppConfigCmd));
    if (configQueue == NULL) {
        ESP_LOGE(TAG, "FAILED to create Config Queue — aborting");
        return;
    }

    // -------------------------------------------------------------------------
    // Step 6: UDP Transmitter
    // -------------------------------------------------------------------------
    static UdpTransmitter udp(
        ThermalConfig::UDP_BROADCAST_IP,
        ThermalConfig::UDP_PORT
    );
    ret = udp.init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAILED to initialize UDP Transmitter — aborting");
        return;
    }

    // -------------------------------------------------------------------------
    // Step 7: Thermal Vision Pipeline (Core 1 - APP_CPU)
    // -------------------------------------------------------------------------
    static ThermalPipeline pipeline(sensor, ipcQueue, configQueue);
    pipeline.init();

    static StaticTask_t pipelineTaskBuffer;
    static StackType_t  pipelineStack[8192 / sizeof(StackType_t)];

    TaskHandle_t pipelineHandle = xTaskCreateStaticPinnedToCore(
        ThermalPipeline::TaskWrapper,   // Function
        "ThermalPipe",                  // Name
        8192 / sizeof(StackType_t),     // Stack (in words)
        &pipeline,                      // Parameter (this)
        configMAX_PRIORITIES - 1,       // Maximum priority
        pipelineStack,
        &pipelineTaskBuffer,
        1                               // Core 1 (APP_CPU)
    );
    if (pipelineHandle == NULL) {
        ESP_LOGE(TAG, "FAILED to create ThermalPipeline task");
        return;
    }
    ESP_LOGI(TAG, "ThermalPipeline launched on Core 1 (max priority)");

    // -------------------------------------------------------------------------
    // Step 7.5: Start Web Server (Core 0)
    // -------------------------------------------------------------------------
    ret = HttpServer::start(configQueue);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Warning: Could not start HTTP web server");
    }

    // -------------------------------------------------------------------------
    // Step 8: Telemetry (Core 0 - PRO_CPU)
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
        tskIDLE_PRIORITY + 2,           // Medium priority
        telemetryStack,
        &telemetryTaskBuffer,
        0                               // Core 0 (PRO_CPU)
    );
    if (telemetryHandle == NULL) {
        ESP_LOGE(TAG, "FAILED to create TelemetryTask task");
        return;
    }
    ESP_LOGI(TAG, "TelemetryTask launched on Core 0 (medium priority)");

    ESP_LOGI(TAG, "=== System operational. Tasks active ===");

    // -------------------------------------------------------------------------
    // [OTA] Mark current firmware as VALID (anti-bootloop)
    // -------------------------------------------------------------------------
    // If we arrive here, WiFi + Sensor + HTTP Server + Tasks are all active.
    // This call transitions the OTA state from PENDING_VERIFY → VALID,
    // preventing the bootloader from rolling back to factory on the next boot.
    esp_err_t ota_valid = esp_ota_mark_app_valid_cancel_rollback();
    if (ota_valid == ESP_OK) {
        ESP_LOGI(TAG, "[OTA] Partition marked as VALID — rollback cancelled");
    } else if (ota_valid == ESP_ERR_NOT_SUPPORTED) {
        // Occurs when the app starts from factory (not from ota_0/ota_1).
        // It is normal during the first USB boot — not an error.
        ESP_LOGI(TAG, "[OTA] Factory partition detected — OTA marking not required");
    } else {
        ESP_LOGW(TAG, "[OTA] esp_ota_mark_app_valid failed: %s", esp_err_to_name(ota_valid));
    }

    // app_main() returns here; FreeRTOS tasks run autonomously
}
