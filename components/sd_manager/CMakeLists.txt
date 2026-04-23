# components/sd_manager/CMakeLists.txt
idf_component_register(
    SRCS "src/sd_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_sdspi fatfs sdmmc freertos
)
