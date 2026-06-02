idf_component_register(
    SRCS "src/tft_driver.cpp"
         "src/tft_renderer.cpp"
         "src/tft_display_task.cpp"
         "src/tft_view_manager.cpp"
         "src/tft_heatmap_view.cpp"
         "src/tft_tracking_view.cpp"
         "src/tft_stats_view.cpp"
         "src/tft_info_view.cpp"
         "src/tft_system_snapshot.cpp"
         "src/boot_button.cpp"
         "src/tft_snapshot.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer freertos
    PRIV_REQUIRES thermal_pipeline
)
