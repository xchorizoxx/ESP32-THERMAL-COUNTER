#include "status_led.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include <math.h>

static const char *TAG = "StatusLed";

#define LED_GPIO 48
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

// --- MASTER BRIGHTNESS CONTROL ---
#define INITIAL_MASTER_BRIGHTNESS 0.8f // <--- MAX BRIGHTNESS

// Gamma correction for smoother breathing
static uint8_t gamma8(uint8_t input) { return (uint32_t)input * input / 255; }

struct RgbColor {
  uint8_t r, g, b;
};

// Resistor Color Code Palette (Optimized for low brightness WS2812)
static const RgbColor kResistorPalette[] = {
    {40, 40, 40},   // 0: Black/Grey (Smoke)
    {80, 40, 10},   // 1: Brown
    {180, 0, 0},    // 2: Red
    {180, 60, 0},   // 3: Orange
    {150, 150, 0},  // 4: Yellow
    {0, 180, 0},    // 5: Green
    {0, 0, 180},    // 6: Blue
    {120, 0, 180},  // 7: Violet
    {40, 40, 40},   // 8: Grey
    {150, 150, 150} // 9: White
};

static led_strip_handle_t s_led_handle = nullptr;

StatusLedManager &StatusLedManager::getInstance() {
  static StatusLedManager instance;
  return instance;
}

void StatusLedManager::init() {
  if (m_initialized)
    return;

  ESP_LOGI(TAG, "Initializing StatusLedManager on GPIO %d", LED_GPIO);

  led_strip_config_t strip_config = {.strip_gpio_num = LED_GPIO,
                                     .max_leds = 1,
                                     .led_model = LED_MODEL_WS2812,
                                     .color_component_format =
                                         LED_STRIP_COLOR_COMPONENT_FMT_GRB,
                                     .flags = {.invert_out = false}};

  led_strip_rmt_config_t rmt_config = {.clk_src = RMT_CLK_SRC_DEFAULT,
                                       .resolution_hz = LED_STRIP_RMT_RES_HZ,
                                       .mem_block_symbols = 64,
                                       .flags = {.with_dma = false}};

  ESP_ERROR_CHECK(
      led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_handle));
  led_strip_clear(s_led_handle);

  m_mutex = xSemaphoreCreateMutex();
  m_masterBrightness = INITIAL_MASTER_BRIGHTNESS;

  xTaskCreatePinnedToCore(taskWrapper, "status_led_task", 3072, this,
                          configMAX_PRIORITIES - 3, &m_taskHandle, 0);

  m_initialized = true;
}

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
    m_hasEvent = true;
    xSemaphoreGive(m_mutex);
  }
}

void StatusLedManager::taskWrapper(void *pvParameters) {
  static_cast<StatusLedManager *>(pvParameters)->taskLoop();
}

void StatusLedManager::taskLoop() {
  uint32_t tick = 0;

  while (true) {
    State current_state = State::IDLE;
    uint8_t tracks = 0;
    Event event = (Event)-1;
    bool has_event = false;

    // 1. Capture state under mutex
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
      current_state = m_state;
      tracks = m_trackCount;
      if (m_hasEvent) {
        event = m_pendingEvent;
        has_event = true;
        m_hasEvent = false;
      }
      xSemaphoreGive(m_mutex);
    }

    // 2. Handle high-priority events (Strobe)
    if (has_event) {
      if (event == Event::CROSS_IN)
        updateHardware(0, 255, 0, 0.30f); // Green Strobe
      else
        updateHardware(0, 0, 255, 0.30f); // Blue Strobe
      vTaskDelay(pdMS_TO_TICKS(50));
      led_strip_clear(s_led_handle);
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 3. Handle persistent states
    RgbColor color = {0, 0, 0};
    float brightness = 0.40f;
    bool blink_pattern = false;
    int blink_count = 1;

    switch (current_state) {
    case State::BOOTING:
      color = {255, 255, 255}; // Bright White for boot
      brightness = 1.0f;
      break;

    case State::TRACKING:
      if (tracks > 0) {
        uint8_t digit = tracks % 10;
        color = kResistorPalette[digit];
        brightness = 0.80f;

        if (tracks >= 10 && tracks < 20) {
          blink_pattern = true;
          blink_count = 2;
        } else if (tracks >= 20) {
          color = {255, 0, 0};
          blink_pattern = true;
          blink_count = 3;
          brightness = 1.0f;
        }
        break;
      }
      // If tracks == 0, fall through to IDLE/Breathe logic
      [[fallthrough]];

    case State::FATAL_ERROR:
      color = {255, 0, 0}; // Pure Red
      brightness = 1.0f;
      blink_pattern = true;
      blink_count = 3;
      break;

    case State::IDLE:
      // Breathing logic for IDLE
      color = {0, 130, 255}; // Azure Blue
      brightness =
          0.10f +
          0.40f * (0.5f + 0.5f * sinf(tick * 0.05f)); // Standard breathe (6s)
      break;
    }

    // Apply Blinking or Solid
    if (blink_pattern) {
      for (int i = 0; i < blink_count; i++) {
        updateHardware(color.r, color.g, color.b, brightness);
        vTaskDelay(pdMS_TO_TICKS(150));
        led_strip_clear(s_led_handle);
        vTaskDelay(pdMS_TO_TICKS(100));
      }
      // Adjust pause to complete the requested cycle
      // 3 blinks = 750ms. To reach 2000ms cycle, we need 1250ms pause.
      uint32_t pause = (current_state == State::FATAL_ERROR) ? 1250 : 800;
      vTaskDelay(pdMS_TO_TICKS(pause));
    } else {
      updateHardware(color.r, color.g, color.b, brightness);
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    tick++;
  }
}

void StatusLedManager::updateHardware(uint8_t r, uint8_t g, uint8_t b,
                                      float brightness_pct) {
  float final_pct = brightness_pct * m_masterBrightness;

  auto apply = [&](uint8_t val) -> uint8_t {
    float fval = (float)val * final_pct;
    if (fval < 0.5f && val > 0 && final_pct > 0.01f)
      return 1;
    return gamma8((uint8_t)fval);
  };

  uint8_t rs = apply(r);
  uint8_t gs = apply(g);
  uint8_t bs = apply(b);

  // Debug log to verify hardware activity during first 10 updates
  static uint32_t log_count = 0;
  if (log_count < 10) {
    log_count++;
    ESP_LOGI(TAG, "LED Update #%lu: R=%d G=%d B=%d (Master: %.2f)",
             (unsigned long)log_count, rs, gs, bs, m_masterBrightness);
  }

  led_strip_set_pixel(s_led_handle, 0, rs, gs, bs);
  esp_err_t err = led_strip_refresh(s_led_handle);
  if (err != ESP_OK && log_count < 20) {
    ESP_LOGE(TAG, "Failed to refresh LED: %s", esp_err_to_name(err));
  }
}
