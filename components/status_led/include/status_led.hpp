#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <atomic>

/**
 * @brief Simplified non-blocking WS2812 status LED manager.
 *
 * Four states, all tick-based (no blocking delays in FSM):
 *   BOOTING     — White blink every 3s
 *   IDLE        — Breathing cyan
 *   TRACKING    — N green blinks = active track count (max 5), every 2s
 *   FATAL_ERROR — 3 fast red blinks, 5s dark
 *
 * Events (strobes):
 *   CROSS_IN  — Green 80ms flash
 *   CROSS_OUT — Blue 80ms flash
 */
class StatusLedManager {
public:
    enum class State {
        BOOTING,
        IDLE,         ///< No active tracks
        TRACKING,     ///< Active tracks detected
        FATAL_ERROR,  ///< Sensor or critical system failure
    };

    enum class Event {
        CROSS_IN,     ///< Person entered
        CROSS_OUT,    ///< Person exited
    };

    struct RgbColor {
        uint8_t r, g, b;
    };

    static StatusLedManager& getInstance();

    void init();
    void setState(State state);
    void setTrackCount(uint8_t count);
    void triggerEvent(Event event);
    void setMasterBrightness(float brightness);  ///< 0.0 to 1.0, thread-safe

private:
    StatusLedManager()  = default;
    ~StatusLedManager() = default;
    StatusLedManager(const StatusLedManager&)            = delete;
    StatusLedManager& operator=(const StatusLedManager&) = delete;

    static void taskWrapper(void* pvParameters);
    void taskLoop();

    // Low-level hardware helpers — only called from taskLoop
    void setPixel(RgbColor color, float state_brightness);
    void clearPixel();

    // Shared state (protected by m_mutex, except m_masterBrightness)
    State    m_state        = State::BOOTING;
    uint8_t  m_trackCount   = 0;
    Event    m_pendingEvent = (Event)-1;
    bool     m_hasEvent     = false;

    std::atomic<float> m_masterBrightness{0.80f};  ///< Lock-free brightness

    SemaphoreHandle_t m_mutex      = nullptr;
    TaskHandle_t      m_taskHandle = nullptr;
    bool              m_initialized = false;
};
