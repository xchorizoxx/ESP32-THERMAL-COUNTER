/**
 * @file status_led.cpp
 * @brief Simplified non-blocking StatusLED FSM.
 *
 * States:
 *   BOOTING      — Single white blink every 3s (100ms on, 2900ms off)
 *   IDLE         — Breathing cyan (sinusoidal, ~4s period)
 *   TRACKING     — n quick green blinks (n = track count, max 5), then 2s pause
 *   FATAL_ERROR  — 3 fast red blinks, then 5s dark
 *
 * Events (strobes, fire-and-forget):
 *   CROSS_IN     — Single green flash (80ms)
 *   CROSS_OUT    — Single blue flash (80ms)
 *
 * All timing is tick-based (xTaskGetTickCount). No blocking vTaskDelay inside patterns.
 * The task sleeps 20ms between iterations (50 Hz service rate).
 */

#include "status_led.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include <math.h>

static const char *TAG = "StatusLed";

#define LED_GPIO              48
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)
#define TASK_PERIOD_MS        20   // service rate [ms]
#define INITIAL_BRIGHTNESS    0.8f

// Gamma correction for smoother visual transitions
static uint8_t gamma8(uint8_t v) { return (uint32_t)v * v / 255; }

static led_strip_handle_t s_led_handle = nullptr;

// =============================================================================
//  SINGLETON
// =============================================================================
StatusLedManager &StatusLedManager::getInstance() {
    static StatusLedManager instance;
    return instance;
}

// =============================================================================
//  INIT
// =============================================================================
void StatusLedManager::init() {
    if (m_initialized) return;

    ESP_LOGI(TAG, "Initializing StatusLedManager on GPIO %d", LED_GPIO);

    led_strip_config_t strip_cfg = {
        .strip_gpio_num        = LED_GPIO,
        .max_leds              = 1,
        .led_model             = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags                 = {.invert_out = false}
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = LED_STRIP_RMT_RES_HZ,
        .mem_block_symbols = 64,
        .flags             = {.with_dma = true}
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led_handle));
    led_strip_clear(s_led_handle);
    led_strip_refresh(s_led_handle);

    m_mutex = xSemaphoreCreateMutex();
    // Use atomic store — consistent with the std::atomic<float> type
    m_masterBrightness.store(INITIAL_BRIGHTNESS, std::memory_order_relaxed);

    xTaskCreatePinnedToCore(taskWrapper, "status_led", 3072, this,
                            configMAX_PRIORITIES - 3, &m_taskHandle, 0);
    m_initialized = true;
}

// =============================================================================
//  PUBLIC API — thread-safe setters
// =============================================================================
void StatusLedManager::setState(State state) {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        m_state = state;
        xSemaphoreGive(m_mutex);
    }
}

void StatusLedManager::setTrackCount(uint8_t count) {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        m_trackCount = count > 20 ? 20 : count;
        xSemaphoreGive(m_mutex);
    }
}

void StatusLedManager::triggerEvent(Event event) {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        m_pendingEvent = event;
        m_hasEvent     = true;
        xSemaphoreGive(m_mutex);
    }
}

void StatusLedManager::setMasterBrightness(float brightness) {
    float b = brightness > 1.0f ? 1.0f : (brightness < 0.0f ? 0.0f : brightness);
    m_masterBrightness.store(b, std::memory_order_relaxed);
}

// =============================================================================
//  TASK
// =============================================================================
void StatusLedManager::taskWrapper(void *pvParameters) {
    static_cast<StatusLedManager *>(pvParameters)->taskLoop();
}

void StatusLedManager::taskLoop() {
    uint32_t  tick         = 0;           // increments every 20ms
    State     lastState    = State::BOOTING;
    int       blinkPhase   = 0;           // generic blink-phase counter
    TickType_t nextTick    = 0;           // when to advance blink phase
    uint8_t   blinksLeft   = 0;          // remaining blinks in TRACKING burst

    while (true) {
        // --- 1. Snapshot state under mutex ---
        State   cur     = State::IDLE;
        uint8_t tracks  = 0;
        Event   event   = (Event)-1;
        bool    hasEv   = false;

        if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
            cur    = m_state;
            tracks = m_trackCount;
            if (m_hasEvent) {
                event   = m_pendingEvent;
                hasEv   = true;
                m_hasEvent = false;
            }
            xSemaphoreGive(m_mutex);
        }

        // --- 2. High-priority events — single short flash, non-blocking ---
        if (hasEv) {
            // We do a single quick flash and immediately return to the loop.
            // 80ms on is short enough that no state transition is missed.
            RgbColor c = (event == Event::CROSS_IN) ? RgbColor{0, 220, 0}
                                                    : RgbColor{0, 60, 255};
            setPixel(c, 0.40f);
            vTaskDelay(pdMS_TO_TICKS(80));
            clearPixel();
            tick++;
            continue;
        }

        // --- 3. Reset FSM when state changes ---
        if (cur != lastState) {
            blinkPhase = 0;
            blinksLeft = 0;
            nextTick   = xTaskGetTickCount();
            lastState  = cur;
        }

        TickType_t now = xTaskGetTickCount();

        // --- 4. State machine ---
        switch (cur) {

        // ---- BOOTING: single white blink every 3s (100ms on, 2900ms off) ----
        case State::BOOTING: {
            if (now >= nextTick) {
                if (blinkPhase == 0) {
                    setPixel({255, 255, 255}, 0.8f);
                    nextTick   = now + pdMS_TO_TICKS(100);
                    blinkPhase = 1;
                } else {
                    clearPixel();
                    nextTick   = now + pdMS_TO_TICKS(2900);
                    blinkPhase = 0;
                }
            }
            break;
        }

        // ---- IDLE: breathing cyan ----
        case State::IDLE: {
            // Period ~4s at 20ms tick: 4000/20 = 200 ticks/cycle
            float breathe = 0.08f + 0.35f * (0.5f + 0.5f * sinf(tick * 0.0314f));
            setPixel({0, 130, 255}, breathe);
            break;
        }

        // ---- TRACKING: n quick green blinks (n=tracks, max 5), then 2s pause ----
        case State::TRACKING: {
            if (tracks == 0) {
                // No tracks → fallback to breathing cyan
                float breathe = 0.08f + 0.35f * (0.5f + 0.5f * sinf(tick * 0.0314f));
                setPixel({0, 130, 255}, breathe);
                break;
            }

            if (now >= nextTick) {
                if (blinkPhase == 0) {
                    // Start a new burst — clamp to 5 blinks max for readability
                    blinksLeft = (tracks > 5) ? 5 : tracks;
                    blinkPhase = 1;
                    nextTick   = now; // process immediately
                } else if (blinkPhase % 2 == 1) {
                    // Blink ON
                    setPixel({0, 200, 0}, 0.90f);
                    nextTick   = now + pdMS_TO_TICKS(120);
                    blinkPhase++;
                } else {
                    // Blink OFF
                    clearPixel();
                    blinksLeft--;
                    if (blinksLeft > 0) {
                        nextTick   = now + pdMS_TO_TICKS(130);
                        blinkPhase++;  // back to ON
                    } else {
                        // Burst done — 2s pause
                        nextTick   = now + pdMS_TO_TICKS(2000);
                        blinkPhase = 0;
                    }
                }
            }
            break;
        }

        // ---- FATAL_ERROR: 3 fast red blinks, then 5s dark ----
        case State::FATAL_ERROR: {
            if (now >= nextTick) {
                if (blinkPhase < 6) {
                    if (blinkPhase % 2 == 0) {
                        setPixel({200, 0, 0}, 1.0f);
                        nextTick = now + pdMS_TO_TICKS(150);
                    } else {
                        clearPixel();
                        nextTick = now + pdMS_TO_TICKS(100);
                    }
                    blinkPhase++;
                } else {
                    clearPixel();
                    nextTick   = now + pdMS_TO_TICKS(5000);
                    blinkPhase = 0;
                }
            }
            break;
        }

        default:
            clearPixel();
            break;
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(TASK_PERIOD_MS));
    }
}

// =============================================================================
//  HARDWARE HELPERS
// =============================================================================
void StatusLedManager::setPixel(RgbColor color, float state_brightness) {
    float master     = m_masterBrightness.load(std::memory_order_relaxed);
    float final_pct  = state_brightness * master;

    auto apply = [&](uint8_t val) -> uint8_t {
        float fv = (float)val * final_pct;
        if (fv < 0.5f && val > 0 && final_pct > 0.01f) return 1;
        return gamma8((uint8_t)fv);
    };

    led_strip_set_pixel(s_led_handle, 0, apply(color.r), apply(color.g), apply(color.b));
    led_strip_refresh(s_led_handle);
}

void StatusLedManager::clearPixel() {
    led_strip_clear(s_led_handle);
    led_strip_refresh(s_led_handle);
}
