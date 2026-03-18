idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES mlx90640_driver thermal_pipeline telemetry
             nvs_flash
)
