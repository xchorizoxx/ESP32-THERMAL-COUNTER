#pragma once
/**
 * @file rtc_driver.hpp
 * @brief Driver para DS3231 RTC usando ESP-IDF 6.0 I2C master API.
 * 
 * Funciona en el bus I2C1 (GPIO 1/2) para total independencia del sensor térmico.
 */
#include "driver/i2c_master.h"
#include "esp_err.h"
#include <cstdint>
#include <ctime>

class RTCDriver {
public:
    static constexpr uint8_t DS3231_ADDR   = 0x68;
    static constexpr uint32_t I2C_TIMEOUT  = 50; // ms

    struct DateTime {
        uint16_t year;    // 2000..2099
        uint8_t  month;   // 1..12
        uint8_t  day;     // 1..31
        uint8_t  hour;    // 0..23
        uint8_t  minute;  // 0..59
        uint8_t  second;  // 0..59

        void toISO(char* buf, size_t len) const {
            snprintf(buf, len, "%04d-%02d-%02dT%02d:%02d:%02d",
                     year, month, day, hour, minute, second);
        }
        
        uint32_t toUnix() const {
            struct tm t = {};
            t.tm_year = year - 1900;
            t.tm_mon  = month - 1;
            t.tm_mday = day;
            t.tm_hour = hour;
            t.tm_min  = minute;
            t.tm_sec  = second;
            return (uint32_t)mktime(&t);
        }
    };

    RTCDriver();
    ~RTCDriver();

    /**
     * @brief Inicializa el bus I2C1 y añade el DS3231.
     */
    esp_err_t init(gpio_num_t sda, gpio_num_t scl);

    bool isAvailable() const { return available_; }

    esp_err_t getTime(DateTime& dt) const;
    esp_err_t setTime(const DateTime& dt);

    /**
     * @brief Retorna ms desde Unix Epoch si RTC disponible, 
     * o ms desde boot como fallback.
     */
    uint64_t getTimestampMs() const;

    esp_err_t getChipTemperature(float& temp_c) const;

private:
    i2c_master_bus_handle_t bus_handle_;
    i2c_master_dev_handle_t dev_handle_;
    bool available_;

    esp_err_t readReg(uint8_t reg, uint8_t* data, size_t len) const;
    esp_err_t writeReg(uint8_t reg, uint8_t val);

    static uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
    static uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }
};
