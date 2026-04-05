#pragma once
#include <stdint.h>

namespace StatusLed {
    /**
     * @brief Initialize the WS2812 RGB LED on GPIO 48
     */
    void init();

    /**
     * @brief Set LED color with global brightness limit
     * @param r Red (0-255)
     * @param g Green (0-255)
     * @param b Blue (0-255)
     * @param brightness Percentage 0-100 (Default: 30 to protect eyes/OTG)
     */
    void set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 30);
}
