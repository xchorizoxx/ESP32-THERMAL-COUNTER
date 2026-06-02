idf_component_register(
    SRCS "src/wifi_softap.cpp"
         "src/udp_transmitter.cpp"
         "src/telemetry_task.cpp"
         "src/log_writer.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif esp_event nvs_flash lwip freertos
             thermal_pipeline web_server sd_manager
    PRIV_REQUIRES tft_display
)
