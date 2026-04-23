#include "rtc_driver.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "RTC_DS3231";

RTCDriver::RTCDriver() 
    : bus_handle_(nullptr), dev_handle_(nullptr), available_(false) {}

RTCDriver::~RTCDriver() {
    if (dev_handle_) {
        i2c_master_bus_rm_device(dev_handle_);
    }
    if (bus_handle_) {
        i2c_del_master_bus(bus_handle_);
    }
}

esp_err_t RTCDriver::init(gpio_num_t sda, gpio_num_t scl) {
    ESP_LOGI(TAG, "Initializing DS3231 on I2C1 (SDA=%d, SCL=%d)", sda, scl);

    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_1;
    bus_cfg.sda_io_num = sda;
    bus_cfg.scl_io_num = scl;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C1 bus: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = DS3231_ADDR;
    dev_cfg.scl_speed_hz = 100000; // 100 kHz is standard and stable for RTC

    err = i2c_master_bus_add_device(bus_handle_, &dev_cfg, &dev_handle_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DS3231 NOT found: %s", esp_err_to_name(err));
        i2c_del_master_bus(bus_handle_);
        bus_handle_ = nullptr;
        return err;
    }

    // Basic ping: read seconds register
    uint8_t dummy;
    if (readReg(0x00, &dummy, 1) == ESP_OK) {
        available_ = true;
        ESP_LOGI(TAG, "DS3231 initialized successfully");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "DS3231 found but not responding to reads");
    return ESP_FAIL;
}

esp_err_t RTCDriver::readReg(uint8_t reg, uint8_t* data, size_t len) const {
    if (!dev_handle_) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit_receive(dev_handle_, &reg, 1, data, len, I2C_TIMEOUT);
}

esp_err_t RTCDriver::writeReg(uint8_t reg, uint8_t val) {
    if (!dev_handle_) return ESP_ERR_INVALID_STATE;
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(dev_handle_, buf, 2, I2C_TIMEOUT);
}

esp_err_t RTCDriver::getTime(DateTime& dt) const {
    if (!available_) return ESP_ERR_INVALID_STATE;
    uint8_t regs[7];
    esp_err_t err = readReg(0x00, regs, 7);
    if (err != ESP_OK) return err;

    dt.second = bcd2dec(regs[0] & 0x7F);
    dt.minute = bcd2dec(regs[1] & 0x7F);
    dt.hour   = bcd2dec(regs[2] & 0x3F); // 24h mode assumed
    dt.day    = bcd2dec(regs[4] & 0x3F);
    dt.month  = bcd2dec(regs[5] & 0x1F);
    dt.year   = 2000 + bcd2dec(regs[6]);

    return ESP_OK;
}

esp_err_t RTCDriver::setTime(const DateTime& dt) {
    if (!available_) return ESP_ERR_INVALID_STATE;
    uint8_t buf[8] = {
        0x00, // Start address
        dec2bcd(dt.second),
        dec2bcd(dt.minute),
        dec2bcd(dt.hour),
        0x01, // Weekday (unused)
        dec2bcd(dt.day),
        dec2bcd(dt.month),
        dec2bcd((uint8_t)(dt.year - 2000))
    };
    return i2c_master_transmit(dev_handle_, buf, 8, I2C_TIMEOUT);
}

uint64_t RTCDriver::getTimestampMs() const {
    if (available_) {
        DateTime dt;
        if (getTime(dt) == ESP_OK) {
            return (uint64_t)dt.toUnix() * 1000ULL;
        }
    }
    // Fallback to system ticks
    return (uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
}

esp_err_t RTCDriver::getChipTemperature(float& temp_c) const {
    if (!available_) return ESP_ERR_INVALID_STATE;
    uint8_t regs[2];
    esp_err_t err = readReg(0x11, regs, 2);
    if (err != ESP_OK) return err;

    int8_t msb = (int8_t)regs[0];
    float frac = (regs[1] >> 6) * 0.25f;
    temp_c = (float)msb + frac;
    return ESP_OK;
}
