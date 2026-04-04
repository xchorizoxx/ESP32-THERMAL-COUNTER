#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

extern "C" {
#include "MLX90640_I2C_Driver.h"
}

static const char* TAG = "MLX_I2C";

// --- Module global state (configured by Mlx90640Sensor::init()) ---
static i2c_master_bus_handle_t  s_bus_handle = NULL;
static i2c_master_dev_handle_t  s_dev_handle = NULL;
static i2c_port_t s_i2c_port   = I2C_NUM_0;
static gpio_num_t s_sda_pin    = GPIO_NUM_8;
static gpio_num_t s_scl_pin    = GPIO_NUM_9;
static int        s_freq_hz    = 400000; // 400 kHz exactos (Melexis standard)
static uint8_t    s_slave_addr = 0x33;
static bool       s_initialized = false;

// --- Static buffer aligned for DMA (1664 bytes for frame + overhead) ---
static uint8_t s_i2c_buffer[1700] __attribute__((aligned(4)));

extern "C" void MLX90640_I2CSetConfig(int port, int sda, int scl, int freq)
{
    s_i2c_port = (i2c_port_t)port;
    s_sda_pin  = (gpio_num_t)sda;
    s_scl_pin  = (gpio_num_t)scl;
    s_freq_hz  = freq;
}

extern "C" int MLX90640_I2CInit(void)
{
    if (s_initialized) return 0;

    // 1. Configure the Master Bus
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = s_i2c_port;
    bus_cfg.sda_io_num = s_sda_pin;
    bus_cfg.scl_io_num = s_scl_pin;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.intr_priority = 0;
    bus_cfg.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Bus Init Failed: %s", esp_err_to_name(err));
        return -1;
    }

    // 2. Add the Sensor Device to the Bus
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = s_slave_addr;
    dev_cfg.scl_speed_hz = s_freq_hz;

    err = i2c_master_bus_add_device(s_bus_handle, &dev_cfg, &s_dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Device Add Failed: %s", esp_err_to_name(err));
        i2c_del_master_bus(s_bus_handle);
        s_bus_handle = NULL;
        return -1;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "I2C Master Driver initialized (DMA enabled, non-blocking)");
    return 0;
}

extern "C" int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data)
{
    if (!s_initialized || !s_dev_handle) return -1;

    // Address is Big-Endian
    uint8_t addr_buf[2];
    addr_buf[0] = (uint8_t)(startAddress >> 8);
    addr_buf[1] = (uint8_t)(startAddress & 0xFF);

    size_t bytes_to_read = nMemAddressRead * 2;
    if (bytes_to_read > sizeof(s_i2c_buffer)) return -1;

    // Combined transmit/receive for atomicity
    esp_err_t err = i2c_master_transmit_receive(s_dev_handle, addr_buf, 2, s_i2c_buffer, bytes_to_read, 500);

    if (err == ESP_ERR_INVALID_RESPONSE) {
        ESP_LOGW(TAG, "I2C NACK at [0x%04X] (Bus Busy or No Response)", startAddress);
        return -1;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Read Failed [0x%04X]: %s", startAddress, esp_err_to_name(err));
        return -1;
    }

    // Convert result buffer (Big-Endian from sensor) to Little-Endian words
    for (int i = 0; i < nMemAddressRead; i++) {
        data[i] = ((uint16_t)s_i2c_buffer[2 * i] << 8) | s_i2c_buffer[2 * i + 1];
    }

    return 0;
}

extern "C" int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data)
{
    if (!s_initialized || !s_dev_handle) return -1;

    uint8_t buf[4];
    buf[0] = (uint8_t)(writeAddress >> 8);
    buf[1] = (uint8_t)(writeAddress & 0xFF);
    buf[2] = (uint8_t)(data >> 8);
    buf[3] = (uint8_t)(data & 0xFF);

    esp_err_t err = i2c_master_transmit(s_dev_handle, buf, 4, 100);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Write Failed [0x%04X]: %s", writeAddress, esp_err_to_name(err));
        return -1;
    }
    return 0;
}

extern "C" void MLX90640_I2CFreqSet(int freq)
{
    s_freq_hz = freq;
}

extern "C" void MLX90640_I2CDeinit(void)
{
    if (s_dev_handle) {
        i2c_master_bus_rm_device(s_dev_handle);
        s_dev_handle = nullptr;
    }
    if (s_bus_handle) {
        i2c_del_master_bus(s_bus_handle);
        s_bus_handle = nullptr;
    }
    s_initialized = false;
    ESP_LOGI(TAG, "I2C Driver deinitialized");
}
