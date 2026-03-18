idf_component_register(
    SRCS "src/http_server.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server thermal_pipeline esp_wifi json nvs_flash esp_app_format app_update
)
