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

#include "status_led.hpp"
#include "driver/gpio.h"
#include "rtc_driver.hpp"
#include "sd_manager.hpp"

static const char* TAG = "MAIN";

// Global instances for Web Server access
RTCDriver g_rtc;
SDManager g_sd;

extern "C" void app_main(void)
{
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON:   ESP_LOGI(TAG, "Boot: Power on"); break;
        case ESP_RST_SW:        ESP_LOGW(TAG, "Boot: Software reset (esp_restart)"); break;
        case ESP_RST_PANIC:     ESP_LOGE(TAG, "Boot: PANIC (exception/assertion)"); break;
        case ESP_RST_INT_WDT:   ESP_LOGE(TAG, "Boot: INT Watchdog timeout"); break;
        case ESP_RST_TASK_WDT:  ESP_LOGE(TAG, "Boot: TASK Watchdog timeout"); break;
        case ESP_RST_WDT:       ESP_LOGE(TAG, "Boot: Watchdog (generic)"); break;
        case ESP_RST_BROWNOUT:  ESP_LOGE(TAG, "Boot: Brownout (power supply issue)"); break;
        default:                ESP_LOGW(TAG, "Boot: Unknown reason (%d)", reason); break;
    }

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

    // Initialize Status LED immediately so we can show boot state
    StatusLedManager::getInstance().init();
    StatusLedManager::getInstance().setState(StatusLedManager::State::BOOTING);

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
    // Step 2.1: MicroSD Storage
    // -------------------------------------------------------------------------
    ret = g_sd.init((gpio_num_t)ThermalConfig::SD_MOSI_PIN,
                    (gpio_num_t)ThermalConfig::SD_MISO_PIN,
                    (gpio_num_t)ThermalConfig::SD_SCK_PIN,
                    (gpio_num_t)ThermalConfig::SD_CS_PIN);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Storage system ready (SD card mounted)");
    } else {
        ESP_LOGW(TAG, "Storage system UNAVAILABLE (No SD card found)");
    }

    // -------------------------------------------------------------------------
    // Step 2.2: Real Time Clock (DS3231 on I2C1)
    // -------------------------------------------------------------------------
    // Set up GPIO Powering for RTC (VCC=6, GND=7)
    gpio_config_t rtc_pwr_conf = {};
    rtc_pwr_conf.intr_type = GPIO_INTR_DISABLE;
    rtc_pwr_conf.mode = GPIO_MODE_OUTPUT;
    rtc_pwr_conf.pin_bit_mask = (1ULL << ThermalConfig::I2C1_VCC_PIN) | (1ULL << ThermalConfig::I2C1_GND_PIN);
    rtc_pwr_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    rtc_pwr_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&rtc_pwr_conf);
    
    // Turn on power to the RTC
    gpio_set_level((gpio_num_t)ThermalConfig::I2C1_VCC_PIN, 1); // VCC = HIGH
    gpio_set_level((gpio_num_t)ThermalConfig::I2C1_GND_PIN, 0); // GND = LOW
    
    // Give the RTC chip 50ms to boot up after receiving power
    vTaskDelay(pdMS_TO_TICKS(50));

    ret = g_rtc.init((gpio_num_t)ThermalConfig::I2C1_SDA_PIN,
                     (gpio_num_t)ThermalConfig::I2C1_SCL_PIN);
    if (ret == ESP_OK) {
        RTCDriver::DateTime dt;
        if (g_rtc.getTime(dt) == ESP_OK) {
            char iso[32];
            dt.toISO(iso, sizeof(iso));
            ESP_LOGI(TAG, "RTC Time synchronized: %s", iso);
        }
    } else {
        ESP_LOGW(TAG, "RTC system UNAVAILABLE (Using relative system ticks)");
    }

    // -------------------------------------------------------------------------
    // Step 3: WiFi SoftAP
    // -------------------------------------------------------------------------
    // [MAGENTA] Network-related log (Full line)
    ESP_LOG_COLOR(LOG_COLOR_MAGENTA, TAG, "Starting SoftAP '%s'...", ThermalConfig::SOFTAP_SSID);
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
        ESP_LOGE(TAG, "FAILED to initialize MLX90640 sensor — pipeline will retry in background");
        // Continuing to allow Web panel to start and show the error
    } else {
        bool rate_ok = false;
        for (int attempt = 1; attempt <= 3; attempt++) {
            ret = sensor.setRefreshRate(0x05); // 16 Hz
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "MLX90640 configured at 16Hz (attempt %d)", attempt);
                rate_ok = true;
                break;
            }
            ESP_LOGW(TAG, "setRefreshRate attempt %d/3 failed: %s", attempt, esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        if (!rate_ok) {
            ESP_LOGE(TAG, "CRITICAL: Cannot set MLX90640 to 16Hz after 3 attempts — rebooting in 2s");
            vTaskDelay(pdMS_TO_TICKS(2000)); // Let log print
            esp_restart();
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
    // P08-fix: Migrado de xQueueCreate (heap) a xQueueCreateStatic (zero-heap).
    // -------------------------------------------------------------------------
    static StaticQueue_t configQueueBuffer;
    static uint8_t configQueueStorage[10 * sizeof(AppConfigCmd)];

    QueueHandle_t configQueue = xQueueCreateStatic(
        10,
        sizeof(AppConfigCmd),
        configQueueStorage,
        &configQueueBuffer
    );
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
    } else {
        // [MAGENTA] Network-related log (Full line)
        ESP_LOG_COLOR(LOG_COLOR_MAGENTA, TAG, "UDP Transmitter initialized");
    }

    // -------------------------------------------------------------------------
    // Step 7: Thermal Vision Pipeline (Core 1 - APP_CPU)
    // -------------------------------------------------------------------------
    static ThermalPipeline pipeline(sensor, ipcQueue, configQueue);
    pipeline.init();

    static StaticTask_t pipelineTaskBuffer;
    static StackType_t  pipelineStack[6144 / sizeof(StackType_t)];

    TaskHandle_t pipelineHandle = xTaskCreateStaticPinnedToCore(
        ThermalPipeline::TaskWrapper,   // Function
        "ThermalPipe",                  // Name
        6144 / sizeof(StackType_t),     // Stack (in words)
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
    static StackType_t  telemetryStack[3584 / sizeof(StackType_t)];

    TaskHandle_t telemetryHandle = xTaskCreateStaticPinnedToCore(
        TelemetryTask::TaskWrapper,
        "TelemetryTX",
        3584 / sizeof(StackType_t),
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

    // -------------------------------------------------------------------------
    // [OTA] Mark current firmware as VALID (anti-bootloop)
    // -------------------------------------------------------------------------
    esp_err_t ota_valid = esp_ota_mark_app_valid_cancel_rollback();
    if (ota_valid == ESP_OK) {
        ESP_LOGI(TAG, "[OTA] Partition marked as VALID — rollback cancelled");
    } else if (ota_valid == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGI(TAG, "[OTA] Factory partition detected — OTA marking not required");
    } else {
        ESP_LOGE(TAG, "[OTA] esp_ota_mark_app_valid failed: %s", esp_err_to_name(ota_valid));
    }

    // -------------------------------------------------------------------------
    // Step 9: Peripheral Auto-Reconnect Watchdog
    // -------------------------------------------------------------------------
    xTaskCreate([](void* arg) {
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(60000 * 2)); // Check every 2 minutes
            
            if (!g_sd.isMounted()) {
                ESP_LOGW(TAG, "SD Card disconnected. Auto-reconnecting...");
                g_sd.init((gpio_num_t)ThermalConfig::SD_MOSI_PIN,
                          (gpio_num_t)ThermalConfig::SD_MISO_PIN,
                          (gpio_num_t)ThermalConfig::SD_SCK_PIN,
                          (gpio_num_t)ThermalConfig::SD_CS_PIN);
            }
            if (!g_rtc.isAvailable()) {
                ESP_LOGW(TAG, "RTC disconnected. Auto-reconnecting...");
                g_rtc.init((gpio_num_t)ThermalConfig::I2C1_SDA_PIN,
                           (gpio_num_t)ThermalConfig::I2C1_SCL_PIN);
            }
        }
    }, "PeriphWatchdog", 2560, NULL, tskIDLE_PRIORITY + 1, NULL);

    StatusLedManager::getInstance().setState(StatusLedManager::State::IDLE);

    // [WHITE] Memory monitoring (Full line, commented)
    // ESP_LOG_COLOR(LOG_COLOR_WHITE, TAG, "=== System operational. app_main HWM: %u words (%u bytes) ===", 
    //               (unsigned int)hwm, (unsigned int)(hwm * sizeof(StackType_t)));

    // app_main() returns here; FreeRTOS tasks run autonomously
}
