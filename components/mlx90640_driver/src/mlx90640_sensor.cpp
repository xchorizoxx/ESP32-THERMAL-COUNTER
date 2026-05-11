#include "mlx90640_sensor.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "MLX_SENSOR";

// External function declared in mlx90640_i2c_esp.cpp and official Melexis files
extern "C" {
#include "MLX90640_I2C_Driver.h"
void MLX90640_I2CDeinit(void);
}

Mlx90640Sensor::Mlx90640Sensor(i2c_port_t port, gpio_num_t sda,
                                gpio_num_t scl, int i2c_freq_hz, uint8_t addr)
    : port_(port), sda_(sda), scl_(scl), i2c_freq_hz_(i2c_freq_hz),
      addr_(addr), ambientTemp_(0.0f), lastSubPageID_(0), initialized_(false)
{
    memset(&params_, 0, sizeof(params_));
    memset(eeData_, 0, sizeof(eeData_));
    memset(frameData_, 0, sizeof(frameData_));
}

esp_err_t Mlx90640Sensor::init()
{
    ESP_LOGI(TAG, "Attempting to probe MLX90640 (addr=0x%02X, SDA=%d, SCL=%d)",
             addr_, sda_, scl_);

    // 1. Configure and start I2C
    MLX90640_I2CSetConfig((int)port_, (int)sda_, (int)scl_, i2c_freq_hz_);
    if (MLX90640_I2CInit() != 0) {
        return ESP_FAIL;
    }

    // 2. Configure Chess mode (with retries)
    int status = -1;
    for (int i = 0; i < 3; i++) {
        status = MLX90640_SetChessMode(addr_);
        if (status == 0) break;
        ESP_LOGW(TAG, "Retrying Chess Mode configuration (%d/3)...", i + 1);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (status != 0) {
        ESP_LOGE(TAG, "Critical Failure: Could not configure Chess Mode (status=%d)", status);
    } else {
        ESP_LOGI(TAG, "Chess Mode configured correctly");
    }

    // 3. Extract parameters from EEPROM
    status = MLX90640_DumpEE(addr_, eeData_);
    if (status != 0) {
        ESP_LOGE(TAG, "Critical Failure: Could not read EEPROM (status=%d)", status);
        return ESP_FAIL;
    }

    status = MLX90640_ExtractParameters(eeData_, &params_);
    if (status != 0) {
        ESP_LOGE(TAG, "Critical Failure: Parameter extraction failed (status=%d)", status);
        return ESP_FAIL;
    }

    // FIX-4: Refresh rate is set by the caller (main.cpp), not here.
    // Setting it here and then overriding in main.cpp wastes an I2C write.

    initialized_ = true;
    ESP_LOGI(TAG, "Sensor Hardware and Parameters Initialized successfully");
    return ESP_OK;
}

esp_err_t Mlx90640Sensor::readFrame(float* outBuffer)
{
    if (!initialized_) return ESP_FAIL;

    int status = MLX90640_GetFrameData(addr_, frameData_);

    if (status == -MLX90640_FRAME_DATA_ERROR) {
        ESP_LOGD(TAG, "Frame data race detected (status=-8), retrying in 2ms...");
        vTaskDelay(pdMS_TO_TICKS(2));
        status = MLX90640_GetFrameData(addr_, frameData_);
        if (status < 0) {
            ESP_LOGW(TAG, "Frame data error persists after retry (status=%d) — frame skipped", status);
            return ESP_FAIL;
        }
        ESP_LOGD(TAG, "Frame recovered after retry");

    } else if (status < 0) {
        ESP_LOGW(TAG, "I2C bus error (status=%d), performing bus recovery...", status);
        resetI2C();
        vTaskDelay(pdMS_TO_TICKS(2));
        status = MLX90640_GetFrameData(addr_, frameData_);
        if (status < 0) {
            ESP_LOGE(TAG, "I2C error persists after bus recovery (status=%d)", status);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Frame recovered after I2C bus reset");
    }

    lastSubPageID_ = MLX90640_GetSubPageNumber(frameData_);

    float ta = MLX90640_GetTa(frameData_, &params_);
    float tr = ta - 8.0f;
    ambientTemp_ = ta;

    MLX90640_CalculateTo(frameData_, &params_, 0.95f, tr, outBuffer);

    return ESP_OK;
}

esp_err_t Mlx90640Sensor::setRefreshRate(uint8_t rate)
{
    if (MLX90640_SetRefreshRate(addr_, rate) != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

float Mlx90640Sensor::getAmbientTemp()
{
    return ambientTemp_;
}

esp_err_t Mlx90640Sensor::resetI2C()
{
    ESP_LOGW(TAG, "Resetting I2C Bus...");
    MLX90640_I2CDeinit();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    MLX90640_I2CSetConfig((int)port_, (int)sda_, (int)scl_, i2c_freq_hz_);
    if (MLX90640_I2CInit() == 0) {
        ESP_LOGI(TAG, "I2C Bus Recovered");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "I2C Recovery FAILED");
    return ESP_FAIL;
}
