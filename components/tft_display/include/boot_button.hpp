#pragma once
/**
 * @file boot_button.hpp
 * @brief Debounced button handler for GPIO 0 (BOOT).
 *
 * Detects short press (<500ms) and long press (>1500ms).
 * Must be polled periodically (e.g., at frame rate).
 * Does NOT use interrupts — pure polling with state machine.
 */

#include <stdint.h>

class BootButton {
public:
    enum class Event {
        NONE,           ///< No event
        SHORT_PRESS,    ///< Released before LONG_PRESS threshold
        LONG_PRESS,     ///< Held beyond LONG_PRESS threshold (fires once)
    };

    /**
     * @brief Initialize GPIO 0 as input with pull-up.
     */
    void init();

    /**
     * @brief Poll the button state and return any detected event.
     *
     * Call this once per frame (~66ms at 15 FPS).
     * Returns SHORT_PRESS on release, LONG_PRESS on threshold crossing.
     *
     * @param now_ms  Current timestamp in milliseconds
     * @return Event detected, or NONE
     */
    Event poll(uint32_t now_ms);

private:
    enum class State {
        IDLE,           // Button released, waiting for press
        DEBOUNCING,     // Press detected, waiting for debounce
        PRESSED,        // Confirmed pressed, timing duration
        LONG_FIRED,     // Long press already fired, waiting for release
    };

    State    state_ = State::IDLE;
    uint32_t press_start_ms_ = 0;
    uint32_t debounce_start_ms_ = 0;

    static const char* TAG;
};
