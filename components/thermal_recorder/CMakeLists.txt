idf_component_register(
    SRCS "src/thermal_recorder.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer nvs_flash
)
