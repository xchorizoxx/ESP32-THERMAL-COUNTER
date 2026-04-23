# components/rtc_driver/CMakeLists.txt
idf_component_register(
    SRCS "src/rtc_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c freertos
)
