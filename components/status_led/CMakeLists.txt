idf_component_register(SRCS "src/status_led.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES led_strip esp_driver_gpio)
