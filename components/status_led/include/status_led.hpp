#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/**
 * @brief Manager for the system status LED (WS2812).
 * 
 * Handles background animations (breathing, blinking) and 
 * high-priority event strobes (IN/OUT crossings).
 */
class StatusLedManager {
public:
    enum class State {
        BOOTING,
        IDLE,           ///< 0 tracks: Breathing Cyan/White
        TRACKING,       ///< 1-20 tracks: Resistor code + blink pattern
        FATAL_ERROR,    ///< Sensor/I2C Fail: Breathing Red
    };

    enum class Event {
        CROSS_IN,       ///< Green Strobe
        CROSS_OUT,      ///< Blue Strobe
    };

    /**
     * @brief Singleton instance access.
     */
    static StatusLedManager& getInstance();

    /**
     * @brief Initialize hardware and start background task.
     */
    void init();

    /**
     * @brief Set the global system state.
     */
    void setState(State state);

    /**
     * @brief Update the number of active tracks to display.
     * @param count 0 to 20
     */
    void setTrackCount(uint8_t count);

    /**
     * @brief Trigger a high-priority visual event (strobe).
     */
    void triggerEvent(Event event);

    /**
     * @brief Set the global master brightness multiplier.
     * @param brightness 0.0f to 1.0f
     */
    void setMasterBrightness(float brightness);

private:
    StatusLedManager() = default;
    ~StatusLedManager() = default;
    StatusLedManager(const StatusLedManager&) = delete;
    StatusLedManager& operator=(const StatusLedManager&) = delete;

    static void taskWrapper(void* pvParameters);
    void taskLoop();

    void updateHardware(uint8_t r, uint8_t g, uint8_t b, float brightness_pct);

    State    m_state = State::BOOTING;
    uint8_t  m_trackCount = 0;
    Event    m_pendingEvent = (Event)-1;
    bool     m_hasEvent = false;
    float    m_masterBrightness = 0.50f; // Default 50%
    
    SemaphoreHandle_t m_mutex = nullptr;
    TaskHandle_t      m_taskHandle = nullptr;
    bool              m_initialized = false;
};
