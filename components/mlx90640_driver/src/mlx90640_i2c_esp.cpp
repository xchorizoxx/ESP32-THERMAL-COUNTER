/**
 * @file mlx90640_i2c_esp.cpp
 * @brief Implementación del driver I2C para MLX90640 usando ESP-IDF.
 *
 * Implementa las 5 funciones declaradas en MLX90640_I2C_Driver.h
 * usando el driver I2C nativo de ESP-IDF (driver/i2c.h).
 *
 * Los parámetros del bus (puerto, pines, dirección) se configuran
 * desde Mlx90640Sensor::init() a través de las funciones de
 * configuración expuestas aquí.
 */

#include "driver/i2c.h"
#include "esp_log.h"
#include <cstring>

extern "C" {
#include "MLX90640_I2C_Driver.h"
}

static const char* TAG = "MLX_I2C";

// --- Estado global del módulo (configurado por Mlx90640Sensor::init()) ---
static i2c_port_t s_i2c_port  = I2C_NUM_0;
static gpio_num_t s_sda_pin   = GPIO_NUM_8;
static gpio_num_t s_scl_pin   = GPIO_NUM_9;
static int        s_freq_hz   = 1000000; // 1 MHz por defecto
static bool       s_initialized = false;

/**
 * @brief Configura los parámetros del bus I2C antes de inicializar.
 * Llamada desde Mlx90640Sensor antes de MLX90640_I2CInit().
 */
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
        ESP_LOGE(TAG, "i2c_param_config falló: %s", esp_err_to_name(err));
        return;
    }

    err = i2c_driver_install(s_i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install falló: %s", esp_err_to_name(err));
        return;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "I2C inicializado: port=%d, SDA=%d, SCL=%d, freq=%d Hz",
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
        ESP_LOGW(TAG, "I2C General Reset falló: %s", esp_err_to_name(err));
        return -1;
    }
    return 0;
}

extern "C" int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress,
                                 uint16_t nMemAddressRead, uint16_t *data)
{
    // Tamaño del buffer temporal para los bytes crudos
    int bytesToRead = nMemAddressRead * 2;
    uint8_t* tempBuf = (uint8_t*)malloc(bytesToRead);
    if (tempBuf == nullptr) {
        ESP_LOGE(TAG, "malloc falló para I2C read buffer (%d bytes)", bytesToRead);
        return -1;
    }

    // Fase Write: enviar dirección de lectura (2 bytes, MSB primero)
    uint8_t addrBuf[2];
    addrBuf[0] = (startAddress >> 8) & 0xFF;
    addrBuf[1] = startAddress & 0xFF;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slaveAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, addrBuf, 2, true);

    // Fase Read: leer los datos (repeated start)
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slaveAddr << 1) | I2C_MASTER_READ, true);
    if (bytesToRead > 1) {
        i2c_master_read(cmd, tempBuf, bytesToRead - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &tempBuf[bytesToRead - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2CRead falló (addr=0x%04X, n=%d): %s",
                 startAddress, nMemAddressRead, esp_err_to_name(err));
        free(tempBuf);
        return -1;
    }

    // Convertir bytes a uint16_t (big-endian del sensor)
    for (int i = 0; i < nMemAddressRead; i++) {
        data[i] = ((uint16_t)tempBuf[2 * i] << 8) | tempBuf[2 * i + 1];
    }

    free(tempBuf);
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
        ESP_LOGE(TAG, "I2CWrite falló (addr=0x%04X, data=0x%04X): %s",
                 writeAddress, data, esp_err_to_name(err));
        return -1;
    }
    return 0;
}

extern "C" void MLX90640_I2CFreqSet(int freq)
{
    s_freq_hz = freq;

    if (s_initialized) {
        // Reconfigurar sin reinstalar el driver
        i2c_config_t conf = {};
        conf.mode             = I2C_MODE_MASTER;
        conf.sda_io_num       = s_sda_pin;
        conf.scl_io_num       = s_scl_pin;
        conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = freq;
        i2c_param_config(s_i2c_port, &conf);
        ESP_LOGI(TAG, "I2C frecuencia cambiada a %d Hz", freq);
    }
}
