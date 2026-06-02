/**
 * @file tft_driver.cpp
 * @brief ST7735S driver implementation — SPI3 init, commands, framebuffer push.
 */

#include "tft_driver.hpp"
#include "tft_config.hpp"
#include "thermal_config.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const char* TftDriver::TAG = "TFT_DRIVER";

// ────────────────────────────────────────────────────────────
//  ST7735S Gamma Correction Tables (standard values)
// ────────────────────────────────────────────────────────────

static const uint8_t GMCTRP1_DATA[] = {
    0x0F, 0x1A, 0x0F, 0x18, 0x2F, 0x28, 0x20, 0x22,
    0x1F, 0x1B, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10
};

static const uint8_t GMCTRN1_DATA[] = {
    0x0F, 0x1B, 0x0F, 0x17, 0x33, 0x2C, 0x29, 0x2E,
    0x30, 0x30, 0x39, 0x3F, 0x00, 0x07, 0x03, 0x10
};

// ────────────────────────────────────────────────────────────
//  SPI Pre-Transfer Callback (sets DC pin)
// ────────────────────────────────────────────────────────────

/*static*/ void TftDriver::spiPreTransferCallback(spi_transaction_t* t) {
    int dc = (int)(intptr_t)t->user;
    gpio_set_level((gpio_num_t)ThermalConfig::TFT_DC_PIN, dc);
}

// ────────────────────────────────────────────────────────────
//  Low-Level SPI Primitives
// ────────────────────────────────────────────────────────────

void TftDriver::sendCommand(uint8_t cmd) {
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_data[0] = cmd;
    t.user = (void*)0;  // DC = 0 (command)
    t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_polling_transmit(spi_handle_, &t);
}

void TftDriver::sendData8(uint8_t data) {
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_data[0] = data;
    t.user = (void*)1;  // DC = 1 (data)
    t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_polling_transmit(spi_handle_, &t);
}

void TftDriver::sendData(const uint8_t* data, size_t len) {
    spi_transaction_t t = {};
    t.length = len * 8;
    t.tx_buffer = data;
    t.user = (void*)1;  // DC = 1 (data)
    spi_device_polling_transmit(spi_handle_, &t);
}

// ────────────────────────────────────────────────────────────
//  GRAM Window Setup
// ────────────────────────────────────────────────────────────

void TftDriver::setWindow(int x0, int y0, int x1, int y1) {
    int xs = x0 + TftConfig::GRAM_X_OFFSET;
    int xe = x1 + TftConfig::GRAM_X_OFFSET;
    int ys = y0 + TftConfig::GRAM_Y_OFFSET;
    int ye = y1 + TftConfig::GRAM_Y_OFFSET;

    // CASET: Column Address Set
    sendCommand(0x2A);
    sendData8((uint8_t)(xs >> 8));
    sendData8((uint8_t)(xs & 0xFF));
    sendData8((uint8_t)(xe >> 8));
    sendData8((uint8_t)(xe & 0xFF));

    // RASET: Row Address Set
    sendCommand(0x2B);
    sendData8((uint8_t)(ys >> 8));
    sendData8((uint8_t)(ys & 0xFF));
    sendData8((uint8_t)(ye >> 8));
    sendData8((uint8_t)(ye & 0xFF));
}

// ────────────────────────────────────────────────────────────
//  Hardware Reset
// ────────────────────────────────────────────────────────────

void TftDriver::hardwareReset() {
    gpio_set_level((gpio_num_t)ThermalConfig::TFT_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)ThermalConfig::TFT_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)ThermalConfig::TFT_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

// ────────────────────────────────────────────────────────────
//  ST7735S Initialization Sequence (Landscape 160x128)
// ────────────────────────────────────────────────────────────

void TftDriver::initSequence() {
    // Software reset
    sendCommand(0x01); // SWRESET
    vTaskDelay(pdMS_TO_TICKS(150));

    // Exit sleep
    sendCommand(0x11); // SLPOUT
    vTaskDelay(pdMS_TO_TICKS(500));

    // Frame rate control
    sendCommand(0xB1); // FRMCTR1
    sendData8(0x01); sendData8(0x2C); sendData8(0x2D);

    sendCommand(0xB2); // FRMCTR2
    sendData8(0x01); sendData8(0x2C); sendData8(0x2D);

    sendCommand(0xB3); // FRMCTR3
    sendData8(0x01); sendData8(0x2C); sendData8(0x2D);
    sendData8(0x01); sendData8(0x2C); sendData8(0x2D);

    // Display inversion control
    sendCommand(0xB4); // INVCTR
    sendData8(0x07);

    // Power control
    sendCommand(0xC0); // PWCTR1
    sendData8(0xA2); sendData8(0x02); sendData8(0x84);

    sendCommand(0xC1); // PWCTR2
    sendData8(0xC5);

    sendCommand(0xC2); // PWCTR3
    sendData8(0x0A); sendData8(0x00);

    sendCommand(0xC3); // PWCTR4
    sendData8(0x8A); sendData8(0x2A);

    sendCommand(0xC4); // PWCTR5
    sendData8(0x8A); sendData8(0xEE);

    // VCOM control
    sendCommand(0xC5); // VMCTR1
    sendData8(0x0E);

    // Display inversion OFF (normal colors)
    sendCommand(0x20); // INVOFF

    // Color mode: 16-bit RGB565
    sendCommand(0x3A); // COLMOD
    sendData8(0x05);

    // Memory access control: landscape (MV=1, MX=1)
    sendCommand(0x36); // MADCTL
    sendData8(0x60);

    // Column address set (full width)
    sendCommand(0x2A); // CASET
    sendData8(0x00);
    sendData8((uint8_t)TftConfig::GRAM_X_OFFSET);
    sendData8(0x00);
    sendData8((uint8_t)(TftConfig::GRAM_X_OFFSET + TftConfig::WIDTH - 1));

    // Row address set (full height)
    sendCommand(0x2B); // RASET
    sendData8(0x00);
    sendData8((uint8_t)TftConfig::GRAM_Y_OFFSET);
    sendData8(0x00);
    sendData8((uint8_t)(TftConfig::GRAM_Y_OFFSET + TftConfig::HEIGHT - 1));

    // Gamma correction
    sendCommand(0xE0); // GMCTRP1 (positive)
    sendData(GMCTRP1_DATA, sizeof(GMCTRP1_DATA));

    sendCommand(0xE1); // GMCTRN1 (negative)
    sendData(GMCTRN1_DATA, sizeof(GMCTRN1_DATA));

    // Normal display mode
    sendCommand(0x13); // NORON
    vTaskDelay(pdMS_TO_TICKS(10));

    // Display ON
    sendCommand(0x29); // DISPON
    vTaskDelay(pdMS_TO_TICKS(100));
}

// ────────────────────────────────────────────────────────────
//  Public API — Initialization
// ────────────────────────────────────────────────────────────

esp_err_t TftDriver::init() {
    if (initialized_) return ESP_OK;

    // Configure DC, RST, and BL pins as GPIO outputs
    gpio_config_t io_cfg = {};
    io_cfg.intr_type    = GPIO_INTR_DISABLE;
    io_cfg.mode         = GPIO_MODE_OUTPUT;
    io_cfg.pin_bit_mask = (1ULL << ThermalConfig::TFT_DC_PIN) |
                          (1ULL << ThermalConfig::TFT_RST_PIN) |
                          (1ULL << ThermalConfig::TFT_BL_PIN);
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpio_config(&io_cfg);

    // Drive RST high and BL low initially
    gpio_set_level((gpio_num_t)ThermalConfig::TFT_RST_PIN, 1);
    gpio_set_level((gpio_num_t)ThermalConfig::TFT_BL_PIN, 0);

    // Initialize SPI3 bus (write-only, no MISO)
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num     = ThermalConfig::TFT_MOSI_PIN;
    bus_cfg.miso_io_num     = -1;  // TFT is write-only
    bus_cfg.sclk_io_num     = ThermalConfig::TFT_SCK_PIN;
    bus_cfg.quadwp_io_num   = -1;
    bus_cfg.quadhd_io_num   = -1;
    bus_cfg.max_transfer_sz = TftConfig::FB_SIZE_BYTES;

    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI3 bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Add ST7735S device to SPI3 bus
    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = ThermalConfig::TFT_SPI_FREQ_HZ; // 26 MHz
    dev_cfg.mode           = 0;
    dev_cfg.spics_io_num   = ThermalConfig::TFT_CS_PIN;
    dev_cfg.queue_size     = 7;
    dev_cfg.pre_cb         = spiPreTransferCallback;

    ret = spi_bus_add_device(SPI3_HOST, &dev_cfg, &spi_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ST7735S device add failed: %s", esp_err_to_name(ret));
        spi_bus_free(SPI3_HOST);
        return ret;
    }

    // Hardware reset + init sequence
    hardwareReset();
    initSequence();

    // Turn on backlight
    gpio_set_level((gpio_num_t)ThermalConfig::TFT_BL_PIN, 1);

    initialized_ = true;
    ESP_LOGI(TAG, "ST7735S initialized (SPI3, %dx%d landscape)",
             TftConfig::WIDTH, TftConfig::HEIGHT);
    return ESP_OK;
}

// ────────────────────────────────────────────────────────────
//  Public API — Fill Screen (solid color, byte-swapped for SPI)
// ────────────────────────────────────────────────────────────

void TftDriver::fillScreen(uint16_t color_rgb565) {
    if (!initialized_) return;

    // ST7735S expects big-endian RGB565; ESP32 is little-endian
    uint16_t swapped = (color_rgb565 >> 8) | (color_rgb565 << 8);

    // Fill one row buffer with the color, then blast 128 rows
    static uint16_t row_buf[160]; // 320 bytes = one row
    for (int i = 0; i < 160; i++) {
        row_buf[i] = swapped;
    }

    setWindow(0, 0, TftConfig::WIDTH - 1, TftConfig::HEIGHT - 1);
    sendCommand(0x2C); // RAMWR

    spi_transaction_t t = {};
    t.tx_buffer = row_buf;
    t.length    = 160 * 2 * 8; // 160 pixels × 16 bits
    t.user      = (void*)1;    // DC = 1 (data)

    for (int row = 0; row < TftConfig::HEIGHT; row++) {
        spi_device_polling_transmit(spi_handle_, &t);
    }
}

// ────────────────────────────────────────────────────────────
//  Public API — Push Full Framebuffer
// ────────────────────────────────────────────────────────────

void TftDriver::pushPixels(const uint16_t* fb) {
    if (!initialized_) return;

    setWindow(0, 0, TftConfig::WIDTH - 1, TftConfig::HEIGHT - 1);
    sendCommand(0x2C); // RAMWR

    spi_transaction_t t = {};
    t.length    = TftConfig::FB_SIZE_BYTES * 8; // 40960 bytes × 8 = 327680 bits
    t.tx_buffer = fb;
    t.user      = (void*)1; // DC = 1 (data)
    spi_device_polling_transmit(spi_handle_, &t);
}

// ────────────────────────────────────────────────────────────
//  Public API — Sleep / Wake
// ────────────────────────────────────────────────────────────

void TftDriver::sleep() {
    if (!initialized_ || sleeping_) return;

    sendCommand(0x10); // SLPIN
    gpio_set_level((gpio_num_t)ThermalConfig::TFT_BL_PIN, 0);
    sleeping_ = true;
    ESP_LOGI(TAG, "Display sleep: backlight off, SLPIN sent");
}

void TftDriver::wakeup() {
    if (!initialized_ || !sleeping_) return;

    sendCommand(0x11); // SLPOUT
    vTaskDelay(pdMS_TO_TICKS(120));
    gpio_set_level((gpio_num_t)ThermalConfig::TFT_BL_PIN, 1);
    sleeping_ = false;
    ESP_LOGI(TAG, "Display wake: backlight on, SLPOUT sent");
}
