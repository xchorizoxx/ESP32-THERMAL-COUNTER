/**
 * @file mlx90640_sensor.cpp
 * @brief Implementación de la clase Mlx90640Sensor.
 *
 * Wraps the Melexis C library with a C++ OOP interface,
 * managing initialization, frame reading, and error recovery.
 */

#include "mlx90640_sensor.hpp"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "MLX_SENSOR";

// Función externa declarada en mlx90640_i2c_esp.cpp y las oficiales de Melexis
extern "C" {
#include "MLX90640_I2C_Driver.h"
void MLX90640_I2CSetConfig(i2c_port_t port, gpio_num_t sda,
                          gpio_num_t scl, int freqHz);
}

Mlx90640Sensor::Mlx90640Sensor(i2c_port_t port, gpio_num_t sda,
                                gpio_num_t scl, uint8_t addr)
    : port_(port)
    , sda_(sda)
    , scl_(scl)
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
    ESP_LOGI(TAG, "Inicializando sensor MLX90640 (addr=0x%02X, SDA=%d, SCL=%d)",
             addr_, sda_, scl_);

    // 1. Configurar y arrancar I2C
    // NOTA: Revertimos a 400kHz por solicitud del usuario para mayor estabilidad.
    MLX90640_I2CSetConfig(port_, sda_, scl_, 400000);
    MLX90640_I2CInit();

    // 2. Configurar modo Chess (con reintentos)
    int status = -1;
    for (int i = 0; i < 3; i++) {
        status = MLX90640_SetChessMode(addr_);
        if (status == 0) break;
        ESP_LOGW(TAG, "Reintentando configuración Chess Mode (%d/3)...", i + 1);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (status != 0) {
        ESP_LOGE(TAG, "Fallo crítico: No se pudo configurar Chess Mode (status=%d)", status);
        // Si falla el modo chess, la imagen se verá con rejilla. Intentamos seguir pero marcamos error.
    } else {
        ESP_LOGI(TAG, "Modo Chess configurado correctamente");
    }

    // 3. Leer EEPROM del sensor
    status = MLX90640_DumpEE(addr_, eeData_);
    if (status != 0) {
        ESP_LOGE(TAG, "Fallo al leer EEPROM (status=%d)", status);
        // Intentar reset y reintentar una vez
        if (resetI2C() == ESP_OK) {
            status = MLX90640_DumpEE(addr_, eeData_);
            if (status != 0) {
                ESP_LOGE(TAG, "Segundo intento de EEPROM falló (status=%d)", status);
                return ESP_FAIL;
            }
        } else {
            return ESP_FAIL;
        }
    }
    ESP_LOGI(TAG, "EEPROM leída correctamente (832 words)");

    // Safety check: Detect if EEPROM is all zeros or all 0xFF (I2C failure)
    uint32_t sum = 0;
    for (int i = 0; i < 832; i++) sum += eeData_[i];
    if (sum == 0 || sum == (832 * 0xFFFF)) {
        ESP_LOGE(TAG, "FATAL: EEPROM corrupta o vacía (Suma=0x%08lX). Verifique cableado I2C/Pull-ups.", sum);
        return ESP_FAIL;
    }

    // 4. Extraer parámetros de calibración
    ESP_LOGI(TAG, "Extrayendo parámetros de calibración...");
    status = MLX90640_ExtractParameters(eeData_, &params_);
    if (status != 0) {
        ESP_LOGW(TAG, "ExtractParameters retornó advertencia (status=%d)", status);
        // No es necesariamente fatal, puede ser por píxeles rotos
    }
    ESP_LOGI(TAG, "Parámetros de calibración extraídos");

    initialized_ = true;
    return ESP_OK;
}

esp_err_t Mlx90640Sensor::readFrame(float* outBuffer)
{
    if (!initialized_) {
        ESP_LOGE(TAG, "readFrame() llamado sin inicializar el sensor");
        return ESP_FAIL;
    }

    // Leemos estrictamente 1 sola subpágina por Frame del ESP32.
    // El MLX opera intercalando subpágina 0 y 1. Así la tarea principal 
    // fluirá efectivamente a 16Hz reales (62.5ms por subpágina) sin atascarse.
    int status = MLX90640_GetFrameData(addr_, frameData_);
    if (status < 0) {
        ESP_LOGW(TAG, "GetFrameData falló (status=%d). Intentando reset...", status);
        if (resetI2C() == ESP_OK) {
            status = MLX90640_GetFrameData(addr_, frameData_);
            if (status < 0) {
                ESP_LOGE(TAG, "GetFrameData falló tras reset (status=%d)", status);
                return ESP_FAIL;
            }
        } else {
            return ESP_FAIL;
        }
    }

    // Identificar qué subpágina acabamos de leer
    lastSubPageID_ = MLX90640_GetSubPageNumber(frameData_);
    
    // Calcula la temperatura ambiente
    ambientTemp_ = MLX90640_GetTa(frameData_, &params_);
    float tr = ambientTemp_ - 8.0f; // Aproximación estándar reflectada
    
    // Calcula térmicas SÓLO en la subpágina extraída, superponiéndola al buffer.
    MLX90640_CalculateTo(frameData_, &params_, 0.95f, tr, outBuffer);

    return ESP_OK;
}

esp_err_t Mlx90640Sensor::setRefreshRate(uint8_t rate)
{
    if (rate > 0x07) {
        ESP_LOGE(TAG, "Refresh rate inválido: 0x%02X (máx 0x07)", rate);
        return ESP_ERR_INVALID_ARG;
    }

    int status = MLX90640_SetRefreshRate(addr_, rate);
    if (status != 0) {
        ESP_LOGE(TAG, "Fallo al configurar refresh rate (status=%d)", status);
        return ESP_FAIL;
    }

    // Tabla de refresh rates para log legible
    static const float rateTable[] = {0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f};
    ESP_LOGI(TAG, "Refresh rate configurado a %.1f Hz (code=0x%02X)", rateTable[rate], rate);
    return ESP_OK;
}

float Mlx90640Sensor::getAmbientTemp()
{
    return ambientTemp_;
}

esp_err_t Mlx90640Sensor::resetI2C()
{
    ESP_LOGW(TAG, "Ejecutando reset del bus I2C...");

    // Desinstalar y reinstalar el driver I2C
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
        ESP_LOGE(TAG, "Reset: i2c_param_config falló: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = i2c_driver_install(port_, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Reset: i2c_driver_install falló: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Bus I2C reseteado exitosamente");
    return ESP_OK;
}
