idf_component_register(
    SRCS "src/MLX90640_API.c"
         "src/mlx90640_i2c_esp.cpp"
         "src/mlx90640_sensor.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_driver_gpio
)
