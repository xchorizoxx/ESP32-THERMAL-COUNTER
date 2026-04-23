#pragma once
/**
 * @file sd_manager.hpp
 * @brief Gestor de MicroSD via SPI para ESP-IDF 6.0.
 */
#include "esp_err.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
#include <cstdint>
#include <cstddef>

class SDManager {
public:
    static constexpr const char* MOUNT_POINT = "/sdcard";
    static constexpr size_t      WRITE_CHUNK  = 4096;

    SDManager();
    ~SDManager();

    /**
     * @brief Inicializa SPI y monta la tarjeta SD.
     */
    esp_err_t init(gpio_num_t mosi, gpio_num_t miso, gpio_num_t sck, gpio_num_t cs);

    bool     isMounted() const { return mounted_; }
    uint64_t getFreeSpaceBytes() const;
    uint64_t getTotalSpaceBytes() const;

    // Operaciones de archivo
    esp_err_t mkdir(const char* rel_path);
    esp_err_t writeFile(const char* rel_path, const uint8_t* data, size_t len, bool append = false);
    esp_err_t appendLine(const char* rel_path, const char* line);

    bool   fileExists(const char* rel_path) const;
    size_t fileSize(const char* rel_path) const;
    esp_err_t deleteFile(const char* rel_path);

private:
    sdmmc_card_t* card_;
    bool mounted_;
    char full_path_[256];

    const char* toFull(const char* rel_path);
};
