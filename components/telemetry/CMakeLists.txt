idf_component_register(
    SRCS "src/wifi_softap.cpp"
         "src/udp_transmitter.cpp"
         "src/telemetry_task.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif esp_event nvs_flash lwip freertos
             thermal_pipeline web_server
)
