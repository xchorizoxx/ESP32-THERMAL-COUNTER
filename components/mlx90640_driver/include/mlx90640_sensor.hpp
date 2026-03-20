#pragma once
/**
 * @file mlx90640_sensor.hpp
 * @brief C++ OOP Wrapper for the MLX90640 thermal sensor.
 *
 * Encapsulates the Melexis C library and the ESP-IDF I2C bus
 * in a clean class with internal static buffers.
 */

#include "driver/i2c.h"
#include "esp_err.h"
#include <cstdint>

// Forward declaration — struct is defined in MLX90640_API.h
extern "C" {
#include "MLX90640_API.h"
}

class Mlx90640Sensor {
public:
    /**
     * @brief Sensor constructor.
     * @param port   I2C Port (I2C_NUM_0 or I2C_NUM_1)
     * @param sda    GPIO Pin for SDA
     * @param scl    GPIO Pin for SCL
     * @param addr   I2C Sensor Address (default 0x33)
     */
    Mlx90640Sensor(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, int i2c_freq_hz, uint8_t addr);

    /**
     * @brief Initializes the I2C bus, reads the EEPROM, and extracts calibration parameters.
     * @return ESP_OK on success, ESP_FAIL on error.
     */
    esp_err_t init();

    /**
     * @brief Reads a full frame of temperatures (both sub-frames).
     * @param outBuffer Array of 768 floats where temperatures in °C are written.
     * @return ESP_OK on success, ESP_FAIL if I2C reading fails.
     *
     * @note This method sequentially reads sub-frames 0 and 1 to obtain
     *       all 768 pixels (chess mode).
     */
    esp_err_t readFrame(float* outBuffer);

    /**
     * @brief Configures the sensor refresh rate.
     * @param rate Refresh rate code:
     *   - 0x00 = 0.5 Hz
     *   - 0x01 = 1 Hz
     *   - 0x02 = 2 Hz
     *   - 0x03 = 4 Hz
     *   - 0x04 = 8 Hz
     *   - 0x05 = 16 Hz
     *   - 0x06 = 32 Hz
     *   - 0x07 = 64 Hz
     * @return ESP_OK on success.
     */
    esp_err_t setRefreshRate(uint8_t rate);

    /**
     * @brief Gets the ambient temperature (Ta) from the last read frame.
     * @return Ambient temperature in °C.
     */
    float getAmbientTemp();

    /**
     * @brief Gets the ID of the last read subpage (0 or 1).
     * @return Subpage ID.
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

    // Static buffers (not allocated at runtime)
    paramsMLX90640 params_;
    uint16_t       eeData_[832];
    uint16_t       frameData_[834];

    /**
     * @brief Attempts to reset the I2C bus in case of error.
     * @return ESP_OK if reset was successful.
     */
    esp_err_t resetI2C();
};
