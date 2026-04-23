#include "status_led.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include <math.h>

static const char *TAG = "StatusLed";

#define LED_GPIO 48
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

// --- MASTER BRIGHTNESS CONTROL ---
#define INITIAL_MASTER_BRIGHTNESS 0.8f

// Gamma correction for smoother visual transitions
static uint8_t gamma8(uint8_t input) { return (uint32_t)input * input / 255; }

// Resistor Color Code Palette (Optimized for visibility)
static const StatusLedManager::RgbColor kResistorPalette[] = {
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

void StatusLedManager::setMasterBrightness(float brightness) {
  if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    m_masterBrightness =
        brightness > 1.0f ? 1.0f : (brightness < 0.0f ? 0.0f : brightness);
    xSemaphoreGive(m_mutex);
  }
}

void StatusLedManager::taskWrapper(void *pvParameters) {
  static_cast<StatusLedManager *>(pvParameters)->taskLoop();
}

void StatusLedManager::flashColor(RgbColor color, float brightness, int blinks,
                                  uint32_t on_ms, uint32_t off_ms) {
  for (int i = 0; i < blinks; i++) {
    updateHardware(color, brightness);
    vTaskDelay(pdMS_TO_TICKS(on_ms));
    led_strip_clear(s_led_handle);
    vTaskDelay(pdMS_TO_TICKS(off_ms));
  }
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
      RgbColor strobeColor = (event == Event::CROSS_IN) ? RgbColor{0, 255, 0}
                                                        : RgbColor{0, 0, 255};
      flashColor(strobeColor, 0.30f, 1, 50, 50);
      continue; // Prevent interfering with state machine delays
    }

    // 3. Handle persistent states
    switch (current_state) {
    case State::BOOTING:
      updateHardware({255, 255, 255}, 1.0f);
      vTaskDelay(pdMS_TO_TICKS(50));
      break;

    case State::TRACKING:
      if (tracks > 0) {
        RgbColor color = kResistorPalette[tracks % 10];

        if (tracks >= 20) {
          flashColor({255, 0, 0}, 1.0f, 3, 150, 100);
          vTaskDelay(pdMS_TO_TICKS(800));
        } else if (tracks >= 10) {
          flashColor(color, 0.80f, 2, 150, 100);
          vTaskDelay(pdMS_TO_TICKS(800));
        } else {
          updateHardware(color, 0.80f);
          vTaskDelay(pdMS_TO_TICKS(50));
        }
        break;
      }
      // Fallthrough to IDLE if tracks == 0
      [[fallthrough]];

    case State::IDLE: {
      // Azure breathe logic
      float breathe_pct = 0.10f + 0.40f * (0.5f + 0.5f * sinf(tick * 0.05f));
      updateHardware({0, 130, 255}, breathe_pct);
      vTaskDelay(pdMS_TO_TICKS(50));
      break;
    }

    case State::FATAL_ERROR:
      flashColor({180, 0, 0}, 1.0f, 3, 150, 100);
      vTaskDelay(pdMS_TO_TICKS(5000));
      break;
    }

    tick++;
  }
}

void StatusLedManager::updateHardware(RgbColor color, float state_brightness) {
  float final_pct = state_brightness * m_masterBrightness;

  auto apply = [&](uint8_t val) -> uint8_t {
    float fval = (float)val * final_pct;
    // Prevent LEDs from turning off due to float precision at low brightness
    if (fval < 0.5f && val > 0 && final_pct > 0.01f)
      return 1;
    return gamma8((uint8_t)fval);
  };

  uint8_t rs = apply(color.r);
  uint8_t gs = apply(color.g);
  uint8_t bs = apply(color.b);

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
