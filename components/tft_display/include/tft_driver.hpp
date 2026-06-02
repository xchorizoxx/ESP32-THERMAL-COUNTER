#pragma once
/**
 * @file tft_driver.hpp
 * @brief ST7735S TFT display driver (SPI3, 160x128 landscape).
 *
 * Hardware abstraction layer for the 1.8" ST7735S module.
 * Handles SPI bus init, command sequences, framebuffer push,
 * and sleep/wake control.
 */

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>

class TftDriver {
public:
    /**
     * @brief Initialize SPI3 bus, register ST7735S device, run init sequence.
     * @return ESP_OK on success, error code if SPI or display init fails.
     *
     * On failure: logs warning and returns error. Caller decides whether to
     * start the display task or not.
     */
    esp_err_t init();

    /**
     * @brief Fill entire screen with a solid RGB565 color.
     * @param color_rgb565 ESP32-native RGB565 value (little-endian).
     *   Internally byte-swapped for ST7735S big-endian SPI protocol.
     */
    void fillScreen(uint16_t color_rgb565);

    /**
     * @brief Push a full framebuffer to the display via DMA.
     * @param fb Pointer to 160x128 RGB565 pixel array (40960 bytes).
     *   MUST be in DMA-capable memory (internal SRAM, DMA_ATTR).
     *   Pixels MUST be pre-swapped for ST7735S big-endian SPI.
     *
     * This call blocks until the DMA transfer completes.
     */
    void pushPixels(const uint16_t* fb);

    /**
     * @brief Enter sleep mode (SLPIN + backlight off).
     * Display retains GRAM contents; wakeup restores the image.
     */
    void sleep();

    /**
     * @brief Exit sleep mode (SLPOUT + backlight on).
     * Includes 120ms stabilization delay.
     */
    void wakeup();

    /// @return true if init() succeeded
    bool isInitialized() const { return initialized_; }

private:
    void sendCommand(uint8_t cmd);
    void sendData8(uint8_t data);
    void sendData(const uint8_t* data, size_t len);
    void setWindow(int x0, int y0, int x1, int y1);
    void hardwareReset();
    void initSequence();

    static void spiPreTransferCallback(spi_transaction_t* t);

    spi_device_handle_t spi_handle_ = nullptr;
    bool initialized_ = false;
    bool sleeping_ = false;

    static const char* TAG;
};
