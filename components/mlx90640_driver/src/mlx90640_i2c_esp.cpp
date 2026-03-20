/**
 * @file mlx90640_i2c_esp.cpp
 * @brief I2C driver implementation for MLX90640 using ESP-IDF.
 *
 * Implements the 5 functions declared in MLX90640_I2C_Driver.h
 * using the native ESP-IDF I2C driver (driver/i2c.h).
 *
 * Bus parameters (port, pins, address) are configured
 * from Mlx90640Sensor::init() through the configuration
 * functions exposed here.
 */

#include "driver/i2c.h"
#include "esp_log.h"
#include <cstring>

extern "C" {
#include "MLX90640_I2C_Driver.h"
}

static const char* TAG = "MLX_I2C";

// --- Module global state (configured by Mlx90640Sensor::init()) ---
static i2c_port_t s_i2c_port  = I2C_NUM_0;
static gpio_num_t s_sda_pin   = GPIO_NUM_8;
static gpio_num_t s_scl_pin   = GPIO_NUM_9;
static int        s_freq_hz   = 1000000; // 1 MHz by default
static bool       s_initialized = false;

/**
 * @brief Configures I2C bus parameters before initialization.
 * Called from Mlx90640Sensor before MLX90640_I2CInit().
 */
// --- Static buffer to avoid fragmentation (768 pixels + 64 aux + overhead) ---
static uint8_t s_i2c_buffer[1700];

extern "C" void MLX90640_I2CSetConfig(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, int freqHz)
{
    s_i2c_port = port;
    s_sda_pin  = sda;
    s_scl_pin  = scl;
    s_freq_hz  = freqHz;
}

extern "C" void MLX90640_I2CInit(void)
{
    if (s_initialized) {
        return;
    }

    i2c_config_t conf = {};
    conf.mode             = I2C_MODE_MASTER;
    conf.sda_io_num       = s_sda_pin;
    conf.scl_io_num       = s_scl_pin;
    conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = s_freq_hz;

    esp_err_t err = i2c_param_config(s_i2c_port, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
        return;
    }

    err = i2c_driver_install(s_i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
        return;
    }

    // --- IMPROVEMENT: Hardware Glitch Filter (Crucial for 1MHz on S3) ---
    // Filter noise up to 7 APB cycles (ideal for preventing I2C bit-flips)
    i2c_filter_enable(s_i2c_port, 7);

    s_initialized = true;
    ESP_LOGI(TAG, "I2C initialized: port=%d, SDA=%d, SCL=%d, freq=%d Hz (HW Filter ON)",
             s_i2c_port, s_sda_pin, s_scl_pin, s_freq_hz);
}

extern "C" int MLX90640_I2CGeneralReset(void)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x00, true); // General Call address
    i2c_master_write_byte(cmd, 0x06, true); // Reset command
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C General Reset failed: %s", esp_err_to_name(err));
        return -1;
    }
    return 0;
}

extern "C" int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress,
                                 uint16_t nMemAddressRead, uint16_t *data)
{
    int bytesToRead = nMemAddressRead * 2;
    
    // Static buffer overflow check
    if (bytesToRead > sizeof(s_i2c_buffer)) {
         ESP_LOGE(TAG, "I2CRead: Insufficient buffer (%d > %zu)", bytesToRead, sizeof(s_i2c_buffer));
         return -1;
    }

    // Write Phase: send read address (2 bytes, MSB first)
    uint8_t addrBuf[2];
    addrBuf[0] = (startAddress >> 8) & 0xFF;
    addrBuf[1] = startAddress & 0xFF;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slaveAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, addrBuf, 2, true);

    // Read Phase: read data (repeated start)
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slaveAddr << 1) | I2C_MASTER_READ, true);
    if (bytesToRead > 1) {
        i2c_master_read(cmd, s_i2c_buffer, bytesToRead - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &s_i2c_buffer[bytesToRead - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    // 100ms timeout is safe for 400kHz even when reading full EEPROM (15ms @ 1MHz, ~40ms @ 400kHz)
    esp_err_t err = i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        // Log only in Debug or Warn to avoid flooding console at 16Hz
        ESP_LOGW(TAG, "I2CRead failed at 0x%04X: %s", startAddress, esp_err_to_name(err));
        return -1;
    }

    // Convert bytes to uint16_t (sensor big-endian)
    for (int i = 0; i < nMemAddressRead; i++) {
        data[i] = ((uint16_t)s_i2c_buffer[2 * i] << 8) | s_i2c_buffer[2 * i + 1];
    }

    return 0;
}

extern "C" int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data)
{
    uint8_t buf[4];
    buf[0] = (writeAddress >> 8) & 0xFF;
    buf[1] = writeAddress & 0xFF;
    buf[2] = (data >> 8) & 0xFF;
    buf[3] = data & 0xFF;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slaveAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, 4, true);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2CWrite failed (addr=0x%04X, data=0x%04X): %s",
                 writeAddress, data, esp_err_to_name(err));
        return -1;
    }
    return 0;
}

extern "C" void MLX90640_I2CFreqSet(int freq)
{
    s_freq_hz = freq;

    if (s_initialized) {
        // Reconfigure without reinstalling driver
        i2c_config_t conf = {};
        conf.mode             = I2C_MODE_MASTER;
        conf.sda_io_num       = s_sda_pin;
        conf.scl_io_num       = s_scl_pin;
        conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = freq;
        i2c_param_config(s_i2c_port, &conf);
        ESP_LOGI(TAG, "I2C frequency changed to %d Hz", freq);
    }
}
