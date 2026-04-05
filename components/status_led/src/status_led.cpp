#include "status_led.hpp"
#include "driver/gpio.h"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "StatusLed";
#define LED_STRIP_BLINK_GPIO  48
#define LED_STRIP_LED_NUMBERS 1
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

static led_strip_handle_t led_strip_handle;
static bool initialized = false;

namespace StatusLed {

void init() {
    ESP_LOGI(TAG, "Initializing RGB LED on GPIO %d", LED_STRIP_BLINK_GPIO);
    
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,
        .max_leds = LED_STRIP_LED_NUMBERS,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        }
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_handle);
    if (err == ESP_OK) {
        led_strip_clear(led_strip_handle);
        initialized = true;
    } else {
        ESP_LOGE(TAG, "Failed to initialize LED strip: %s", esp_err_to_name(err));
    }
}

void set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    if (!initialized) return;
    
    // Scale colors by brightness percentage (0-100)
    uint32_t scaled_r = (r * brightness) / 100;
    uint32_t scaled_g = (g * brightness) / 100;
    uint32_t scaled_b = (b * brightness) / 100;

    led_strip_set_pixel(led_strip_handle, 0, scaled_r, scaled_g, scaled_b);
    led_strip_refresh(led_strip_handle);
}

} // namespace StatusLed
