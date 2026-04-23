#include "sd_manager.hpp"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "SD_MANAGER";

SDManager::SDManager() : card_(nullptr), mounted_(false) {}

SDManager::~SDManager() {
    if (mounted_) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card_);
    }
}

const char* SDManager::toFull(const char* rel_path) {
    snprintf(full_path_, sizeof(full_path_), "%s/%s", MOUNT_POINT, rel_path);
    return full_path_;
}

esp_err_t SDManager::init(gpio_num_t mosi, gpio_num_t miso, gpio_num_t sck, gpio_num_t cs) {
    ESP_LOGI(TAG, "Initializing SD card (SPI: MOSI=%d MISO=%d SCK=%d CS=%d)",
             mosi, miso, sck, cs);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST; 

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num     = mosi;
    bus_cfg.miso_io_num     = miso;
    bus_cfg.sclk_io_num     = sck;
    bus_cfg.quadwp_io_num   = GPIO_NUM_NC;
    bus_cfg.quadhd_io_num   = GPIO_NUM_NC;
    bus_cfg.max_transfer_sz = 4096;

    esp_err_t err = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(err));
        return err;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = cs;
    slot_config.host_id = (spi_host_device_t)host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false
    };

    err = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to mount SD card (%s). Card might not be inserted.", esp_err_to_name(err));
        spi_bus_free((spi_host_device_t)host.slot);
        return err;
    }

    mounted_ = true;
    ESP_LOGI(TAG, "SD Card mounted successfully at %s", MOUNT_POINT);
    
    // Create base directories
    mkdir("logs");
    mkdir("clips");

    return ESP_OK;
}

uint64_t SDManager::getFreeSpaceBytes() const {
    if (!mounted_) return 0;
    struct statvfs vfs;
    if (statvfs(MOUNT_POINT, &vfs) != 0) return 0;
    return (uint64_t)vfs.f_bfree * vfs.f_bsize;
}

uint64_t SDManager::getTotalSpaceBytes() const {
    if (!mounted_) return 0;
    struct statvfs vfs;
    if (statvfs(MOUNT_POINT, &vfs) != 0) return 0;
    return (uint64_t)vfs.f_blocks * vfs.f_bsize;
}

esp_err_t SDManager::mkdir(const char* rel_path) {
    if (!mounted_) return ESP_ERR_INVALID_STATE;
    const char* fp = toFull(rel_path);
    struct stat st;
    if (stat(fp, &st) == 0) return ESP_OK; // Already exists
    
    if (::mkdir(fp, 0755) != 0) {
        ESP_LOGE(TAG, "Failed to create directory: %s", fp);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t SDManager::writeFile(const char* rel_path, const uint8_t* data, size_t len, bool append) {
    if (!mounted_) return ESP_ERR_INVALID_STATE;
    FILE* f = fopen(toFull(rel_path), append ? "ab" : "wb");
    if (!f) return ESP_FAIL;

    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    
    return (written == len) ? ESP_OK : ESP_FAIL;
}

esp_err_t SDManager::appendLine(const char* rel_path, const char* line) {
    if (!mounted_) return ESP_ERR_INVALID_STATE;
    FILE* f = fopen(toFull(rel_path), "a");
    if (!f) return ESP_FAIL;
    
    fprintf(f, "%s\n", line);
    fclose(f);
    return ESP_OK;
}

bool SDManager::fileExists(const char* rel_path) const {
    if (!mounted_) return false;
    struct stat st;
    return stat(const_cast<SDManager*>(this)->toFull(rel_path), &st) == 0;
}

size_t SDManager::fileSize(const char* rel_path) const {
    if (!mounted_) return 0;
    struct stat st;
    if (stat(const_cast<SDManager*>(this)->toFull(rel_path), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

esp_err_t SDManager::deleteFile(const char* rel_path) {
    if (!mounted_) return ESP_ERR_INVALID_STATE;
    if (unlink(toFull(rel_path)) != 0) return ESP_FAIL;
    return ESP_OK;
}
