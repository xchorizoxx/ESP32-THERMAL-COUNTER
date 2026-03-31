idf_component_register(
    SRCS "src/http_server.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server thermal_pipeline esp_wifi nvs_flash esp_app_format app_update cjson
    EMBED_FILES 
        "src/web/index.html"
        "src/web/style.css"
        "src/web/app.js"
)
