/**
 * @file mlx90640_sensor.cpp
 * @brief Implementation of the Mlx90640Sensor class.
 *
 * Wraps the Melexis C library with a C++ OOP interface,
 * managing initialization, frame reading, and error recovery.
 */

#include "mlx90640_sensor.hpp"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "MLX_SENSOR";

// External function declared in mlx90640_i2c_esp.cpp and official Melexis files
extern "C" {
#include "MLX90640_I2C_Driver.h"
void MLX90640_I2CSetConfig(i2c_port_t port, gpio_num_t sda,
                          gpio_num_t scl, int freqHz);
}

Mlx90640Sensor::Mlx90640Sensor(i2c_port_t port, gpio_num_t sda,
                                gpio_num_t scl, int i2c_freq_hz, uint8_t addr)
    : port_(port)
    , sda_(sda)
    , scl_(scl)
    , i2c_freq_hz_(i2c_freq_hz)
    , addr_(addr)
    , ambientTemp_(25.0f)
    , initialized_(false)
{
    memset(&params_, 0, sizeof(params_));
    memset(eeData_, 0, sizeof(eeData_));
    memset(frameData_, 0, sizeof(frameData_));
}

esp_err_t Mlx90640Sensor::init()
{
    ESP_LOGI(TAG, "Initializing MLX90640 sensor (addr=0x%02X, SDA=%d, SCL=%d)",
             addr_, sda_, scl_);

    // 1. Configure and start I2C
    MLX90640_I2CSetConfig(port_, sda_, scl_, i2c_freq_hz_);
    MLX90640_I2CInit();

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
        // If chess mode fails, the image will appear with a grid. We continue but mark the error.
    } else {
        ESP_LOGI(TAG, "Chess Mode configured correctly");
    }

    // 3. Read sensor EEPROM
    status = MLX90640_DumpEE(addr_, eeData_);
    if (status != 0) {
        ESP_LOGE(TAG, "Failed to read EEPROM (status=%d)", status);
        // Attempt reset and retry once
        if (resetI2C() == ESP_OK) {
            status = MLX90640_DumpEE(addr_, eeData_);
            if (status != 0) {
                ESP_LOGE(TAG, "Second attempt to read EEPROM failed (status=%d)", status);
                return ESP_FAIL;
            }
        } else {
            return ESP_FAIL;
        }
    }
    ESP_LOGI(TAG, "EEPROM read successfully (832 words)");

    // Safety check: Detect if EEPROM is all zeros or all 0xFF (I2C failure)
    uint32_t sum = 0;
    for (int i = 0; i < 832; i++) sum += eeData_[i];
    if (sum == 0 || sum == (832 * 0xFFFF)) {
        ESP_LOGE(TAG, "FATAL: Corrupt or empty EEPROM (Sum=0x%08lX). Check I2C wiring/Pull-ups.", sum);
        return ESP_FAIL;
    }

    // 4. Extract calibration parameters
    ESP_LOGI(TAG, "Extracting calibration parameters...");
    status = MLX90640_ExtractParameters(eeData_, &params_);
    if (status != 0) {
        ESP_LOGW(TAG, "ExtractParameters returned warning (status=%d)", status);
        // Not necessarily fatal, could be due to broken pixels
    }
    ESP_LOGI(TAG, "Calibration parameters extracted");

    initialized_ = true;
    return ESP_OK;
}

esp_err_t Mlx90640Sensor::readFrame(float* outBuffer)
{
    if (!initialized_) {
        ESP_LOGE(TAG, "readFrame() called without initializing the sensor");
        return ESP_FAIL;
    }

    // We strictly read 1 single subpage per ESP32 Frame.
    // The MLX operates by interleaving subpage 0 and 1. This way the main task 
    // will effectively flow at 16Hz real (62.5ms per subpage) without stalling.
    int status = MLX90640_GetFrameData(addr_, frameData_);
    if (status < 0) {
        ESP_LOGW(TAG, "GetFrameData failed (status=%d). Attempting reset...", status);
        if (resetI2C() == ESP_OK) {
            status = MLX90640_GetFrameData(addr_, frameData_);
            if (status < 0) {
                ESP_LOGE(TAG, "GetFrameData failed after reset (status=%d)", status);
                return ESP_FAIL;
            }
        } else {
            return ESP_FAIL;
        }
    }

    // Identify which subpage we just read
    lastSubPageID_ = MLX90640_GetSubPageNumber(frameData_);
    
    // Calculates ambient temperature with a very strong EMA filter to avoid global flickering
    float rawTa = MLX90640_GetTa(frameData_, &params_);
    float vdd = MLX90640_GetVdd(frameData_, &params_);

    // --- DATA PROTECTION (Data Sanity Guard) ---
    // If I2C fails silently (bit flip), VDD usually gives crazy values.
    // The MLX operates between 3.0V and 3.6V. If it's outside, the frame is garbage.
    if (vdd < 2.5f || vdd > 4.0f || rawTa < -40.0f || rawTa > 150.0f) {
        ESP_LOGW(TAG, "Frame discarded due to electrical/I2C anomaly: VDD=%.2fV, Ta=%.2fC", vdd, rawTa);
        return ESP_ERR_INVALID_STATE; 
    }

    // --- PATTERN VERIFICATION ---
    // Ensure the Chess Mode bit (bit 12 of Ctrl Reg) is active.
    // frameData[832] contains a copy of Control Register 1.
    // If there is an I2C glitch at the end of the frame, this bit may change
    // causing CalculateTo to use the Interleave pattern (GRID pattern).
    frameData_[832] |= 0x1000; 

    if (!initialized_) {
        ambientTemp_ = rawTa;
    } else {
        // EMA Filter (0.1): we only allow slow changes in the reference
        ambientTemp_ = (rawTa * 0.1f) + (ambientTemp_ * 0.9f);
    }
    
    float tr = ambientTemp_ - 8.0f; // Standard reflected approximation
    
    // Calculates thermals ONLY in the extracted subpage, overlaying it on the buffer.
    MLX90640_CalculateTo(frameData_, &params_, 0.95f, tr, outBuffer);

    return ESP_OK;
}

esp_err_t Mlx90640Sensor::setRefreshRate(uint8_t rate)
{
    if (rate > 0x07) {
        ESP_LOGE(TAG, "Invalid refresh rate: 0x%02X (max 0x07)", rate);
        return ESP_ERR_INVALID_ARG;
    }

    int status = MLX90640_SetRefreshRate(addr_, rate);
    if (status != 0) {
        ESP_LOGE(TAG, "Failed to configure refresh rate (status=%d)", status);
        return ESP_FAIL;
    }

    // Table of refresh rates for readable log
    static const float rateTable[] = {0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f};
    ESP_LOGI(TAG, "Refresh rate configured to %.1f Hz (code=0x%02X)", rateTable[rate], rate);
    return ESP_OK;
}

float Mlx90640Sensor::getAmbientTemp()
{
    return ambientTemp_;
}

esp_err_t Mlx90640Sensor::resetI2C()
{
    ESP_LOGW(TAG, "Executing I2C bus reset...");

    // Uninstall and reinstall I2C driver
    i2c_driver_delete(port_);

    i2c_config_t conf = {};
    conf.mode             = I2C_MODE_MASTER;
    conf.sda_io_num       = sda_;
    conf.scl_io_num       = scl_;
    conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;

    esp_err_t err = i2c_param_config(port_, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Reset: i2c_param_config failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = i2c_driver_install(port_, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Reset: i2c_driver_install failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "I2C bus reset successfully");
    return ESP_OK;
}
