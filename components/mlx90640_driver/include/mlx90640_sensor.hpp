#pragma once
/**
 * @file mlx90640_sensor.hpp
 * @brief Wrapper C++ OOP para el sensor térmico MLX90640.
 *
 * Encapsula la librería C de Melexis y el bus I2C de ESP-IDF
 * en una clase limpia con buffers estáticos internos.
 */

#include "driver/i2c.h"
#include "esp_err.h"
#include <cstdint>

// Forward declaration — la struct está definida en MLX90640_API.h
extern "C" {
#include "MLX90640_API.h"
}

class Mlx90640Sensor {
public:
    /**
     * @brief Constructor del sensor.
     * @param port   Puerto I2C (I2C_NUM_0 o I2C_NUM_1)
     * @param sda    Pin GPIO para SDA
     * @param scl    Pin GPIO para SCL
     * @param addr   Dirección I2C del sensor (por defecto 0x33)
     */
    Mlx90640Sensor(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, int i2c_freq_hz, uint8_t addr);

    /**
     * @brief Inicializa el bus I2C, lee la EEPROM y extrae parámetros de calibración.
     * @return ESP_OK en éxito, ESP_FAIL en caso de error.
     */
    esp_err_t init();

    /**
     * @brief Lee un frame completo de temperaturas (ambos sub-frames).
     * @param outBuffer Array de 768 floats donde se escriben las temperaturas en °C.
     * @return ESP_OK en éxito, ESP_FAIL si falla la lectura I2C.
     *
     * @note Este método lee secuencialmente los sub-frames 0 y 1 para obtener
     *       los 768 píxeles completos (modo chess).
     */
    esp_err_t readFrame(float* outBuffer);

    /**
     * @brief Configura la frecuencia de refresco del sensor.
     * @param rate Código de refresh rate:
     *   - 0x00 = 0.5 Hz
     *   - 0x01 = 1 Hz
     *   - 0x02 = 2 Hz
     *   - 0x03 = 4 Hz
     *   - 0x04 = 8 Hz
     *   - 0x05 = 16 Hz
     *   - 0x06 = 32 Hz
     *   - 0x07 = 64 Hz
     * @return ESP_OK en éxito.
     */
    esp_err_t setRefreshRate(uint8_t rate);

    /**
     * @brief Obtiene la temperatura ambiente (Ta) del último frame leído.
     * @return Temperatura ambiente en °C.
     */
    float getAmbientTemp();

    /**
     * @brief Obtiene el ID de la última subpágina leída (0 o 1).
     * @return ID de la subpágina.
     */
    int getLastSubPageID() const { return lastSubPageID_; }

    bool isInitialized() const { return initialized_; }


private:
    i2c_port_t port_;
    gpio_num_t sda_;
    gpio_num_t scl_;
    int        i2c_freq_hz_;
    uint8_t    addr_;
    float      ambientTemp_;
    int        lastSubPageID_;
    bool       initialized_;

    // Buffers estáticos (no se allocan en runtime)
    paramsMLX90640 params_;
    uint16_t       eeData_[832];
    uint16_t       frameData_[834];

    /**
     * @brief Intenta resetear el bus I2C en caso de error.
     * @return ESP_OK si el reset fue exitoso.
     */
    esp_err_t resetI2C();
};
