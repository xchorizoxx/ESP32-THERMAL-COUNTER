idf_component_register(
    SRCS "src/background_model.cpp"
         "src/peak_detector.cpp"
         "src/nms_suppressor.cpp"
         "src/tracklet_tracker.cpp"    # A2: replaces alpha_beta_tracker
         "src/mask_generator.cpp"
         "src/thermal_pipeline.cpp"
         # NOTE: alpha_beta_tracker.cpp is intentionally excluded from SRCS.
         # The file is kept on disk for reference. Remove after A3 is stable.
    INCLUDE_DIRS "include"
    REQUIRES mlx90640_driver freertos esp_timer
)
