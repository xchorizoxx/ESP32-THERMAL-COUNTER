/**
 * @file boot_button.cpp
 * @brief Debounced button FSM for GPIO 0 (BOOT button).
 */

#include "boot_button.hpp"
#include "tft_config.hpp"
#include "driver/gpio.h"
#include "esp_log.h"

const char* BootButton::TAG = "BOOT_BTN";

void BootButton::init() {
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = (1ULL << TftConfig::BUTTON_GPIO);
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;       // No ISR — polling only
    gpio_config(&cfg);
    ESP_LOGI(TAG, "Boot button initialized on GPIO %d (polling mode)", TftConfig::BUTTON_GPIO);
}

BootButton::Event BootButton::poll(uint32_t now_ms) {
    bool pressed = (gpio_get_level((gpio_num_t)TftConfig::BUTTON_GPIO) == 0);

    switch (state_) {
        case State::IDLE:
            if (pressed) {
                debounce_start_ms_ = now_ms;
                state_ = State::DEBOUNCING;
            }
            break;

        case State::DEBOUNCING:
            if (!pressed) {
                // Released during debounce — noise, ignore
                state_ = State::IDLE;
            } else if (now_ms - debounce_start_ms_ >= TftConfig::DEBOUNCE_MS) {
                // Debounce confirmed — start timing
                press_start_ms_ = debounce_start_ms_;
                state_ = State::PRESSED;
            }
            break;

        case State::PRESSED:
            if (!pressed) {
                // Released — short press
                state_ = State::IDLE;
                ESP_LOGI(TAG, "Short press (%lu ms)",
                         (unsigned long)(now_ms - press_start_ms_));
                return Event::SHORT_PRESS;
            } else if (now_ms - press_start_ms_ >= TftConfig::LONG_PRESS_MS) {
                // Held long enough — fire long press
                state_ = State::LONG_FIRED;
                ESP_LOGI(TAG, "Long press detected");
                return Event::LONG_PRESS;
            }
            break;

        case State::LONG_FIRED:
            if (!pressed) {
                // Released after long press — return to idle
                state_ = State::IDLE;
            }
            break;
    }

    return Event::NONE;
}
