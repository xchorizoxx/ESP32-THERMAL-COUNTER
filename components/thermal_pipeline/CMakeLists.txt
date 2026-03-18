idf_component_register(
    SRCS "src/background_model.cpp"
         "src/peak_detector.cpp"
         "src/nms_suppressor.cpp"
         "src/alpha_beta_tracker.cpp"
         "src/mask_generator.cpp"
         "src/thermal_pipeline.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mlx90640_driver freertos esp_timer
)
