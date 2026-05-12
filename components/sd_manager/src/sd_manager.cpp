#include "sd_manager.hpp"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "SD_MANAGER";

SDManager::SDManager() : card_(nullptr), mounted_(false), mutex_(nullptr) {
    mutex_ = xSemaphoreCreateMutex();
}

SDManager::~SDManager() {
    if (mounted_.load(std::memory_order_relaxed)) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card_);
    }
}

const char* SDManager::toFull(const char* rel_path) {
    snprintf(full_path_, sizeof(full_path_), "%s/%s", MOUNT_POINT, rel_path);
    return full_path_;
}

void SDManager::lock() const {
    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
}

void SDManager::unlock() const {
    if (mutex_) xSemaphoreGive(mutex_);
}

esp_err_t SDManager::init(gpio_num_t mosi, gpio_num_t miso, gpio_num_t sck, gpio_num_t cs) {
    lock();

    if (mounted_.load(std::memory_order_relaxed)) {
        ESP_LOGI(TAG, "SD Card is already mounted. Ignoring init.");
        unlock();
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SD card (SPI: MOSI=%d MISO=%d SCK=%d CS=%d)",
             mosi, miso, sck, cs);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 10000; 

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num     = mosi;
    bus_cfg.miso_io_num     = miso;
    bus_cfg.sclk_io_num     = sck;
    bus_cfg.quadwp_io_num   = GPIO_NUM_NC;
    bus_cfg.quadhd_io_num   = GPIO_NUM_NC;
    bus_cfg.max_transfer_sz = 4096;

    esp_err_t err = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(err));
        unlock();
        return err;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = cs;
    slot_config.host_id = (spi_host_device_t)host.slot;

    gpio_set_pull_mode(cs, GPIO_PULLUP_ONLY);

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
        unlock();
        return err;
    }

    mounted_.store(true, std::memory_order_relaxed);
    ESP_LOGI(TAG, "SD Card mounted successfully at %s", MOUNT_POINT);
    
    // Create base directories (still holding lock to prevent concurrent access)
    mkdirInternal("logs");
    mkdirInternal("clips");

    unlock();
    return ESP_OK;
}

esp_err_t SDManager::mkdirInternal(const char* rel_path) {
    const char* fp = toFull(rel_path);
    struct stat st;
    if (stat(fp, &st) == 0) {
        return ESP_OK;
    }
    if (::mkdir(fp, 0755) != 0) {
        ESP_LOGE(TAG, "Failed to create directory: %s", fp);
        return ESP_FAIL;
    }
    return ESP_OK;
}

uint64_t SDManager::getFreeSpaceBytes() const {
    if (!mounted_.load(std::memory_order_relaxed)) return 0;
    struct statvfs vfs;
    if (statvfs(MOUNT_POINT, &vfs) != 0) return 0;
    return (uint64_t)vfs.f_bfree * vfs.f_bsize;
}

uint64_t SDManager::getTotalSpaceBytes() const {
    if (!mounted_.load(std::memory_order_relaxed)) return 0;
    struct statvfs vfs;
    if (statvfs(MOUNT_POINT, &vfs) != 0) return 0;
    return (uint64_t)vfs.f_blocks * vfs.f_bsize;
}

esp_err_t SDManager::mkdir(const char* rel_path) {
    if (!mounted_.load(std::memory_order_relaxed)) return ESP_ERR_INVALID_STATE;
    lock();
    const char* fp = toFull(rel_path);
    struct stat st;
    if (stat(fp, &st) == 0) {
        unlock();
        return ESP_OK; // Already exists
    }
    
    if (::mkdir(fp, 0755) != 0) {
        ESP_LOGE(TAG, "Failed to create directory: %s", fp);
        unlock();
        return ESP_FAIL;
    }
    unlock();
    return ESP_OK;
}

esp_err_t SDManager::writeFile(const char* rel_path, const uint8_t* data, size_t len, bool append) {
    if (!mounted_.load(std::memory_order_relaxed)) return ESP_ERR_INVALID_STATE;
    lock();
    FILE* f = fopen(toFull(rel_path), append ? "ab" : "wb");
    if (!f) {
        unlock();
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    
    unlock();
    return (written == len) ? ESP_OK : ESP_FAIL;
}

esp_err_t SDManager::appendLine(const char* rel_path, const char* line) {
    if (!mounted_.load(std::memory_order_relaxed)) return ESP_ERR_INVALID_STATE;
    lock();
    FILE* f = fopen(toFull(rel_path), "a");
    if (!f) {
        unlock();
        return ESP_FAIL;
    }
    
    fprintf(f, "%s\n", line);
    fclose(f);
    unlock();
    return ESP_OK;
}

bool SDManager::fileExists(const char* rel_path) const {
    if (!mounted_.load(std::memory_order_relaxed)) return false;
    struct stat st;
    return stat(const_cast<SDManager*>(this)->toFull(rel_path), &st) == 0;
}

size_t SDManager::fileSize(const char* rel_path) const {
    if (!mounted_.load(std::memory_order_relaxed)) return 0;
    struct stat st;
    if (stat(const_cast<SDManager*>(this)->toFull(rel_path), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

esp_err_t SDManager::deleteFile(const char* rel_path) {
    if (!mounted_.load(std::memory_order_relaxed)) return ESP_ERR_INVALID_STATE;
    if (unlink(toFull(rel_path)) != 0) return ESP_FAIL;
    return ESP_OK;
}

esp_err_t SDManager::listDirectory(const char* rel_path, char* out_json, size_t json_size) const {
    if (!mounted_.load(std::memory_order_relaxed) || !out_json || json_size == 0) {
        if (out_json && json_size > 0) snprintf(out_json, json_size, "[]");
        return ESP_ERR_INVALID_STATE;
    }
    
    lock();
    DIR* dir = opendir(const_cast<SDManager*>(this)->toFull(rel_path));
    if (!dir) {
        unlock();
        snprintf(out_json, json_size, "[]");
        return ESP_FAIL;
    }

    size_t pos = 0;
    pos += snprintf(out_json + pos, json_size - pos, "[");
    bool first = true;
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        if (!first && pos < json_size) {
            pos += snprintf(out_json + pos, json_size - pos, ",");
        }
        first = false;

        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", const_cast<SDManager*>(this)->toFull(rel_path), entry->d_name);
        
        struct stat st;
        size_t size = 0;
        if (stat(file_path, &st) == 0) {
            size = st.st_size;
        }

        if (pos < json_size) {
            pos += snprintf(out_json + pos, json_size - pos, "{\"name\":\"%s\",\"size\":%lu}", entry->d_name, (unsigned long)size);
        }
    }
    closedir(dir);
    
    if (pos < json_size) {
        pos += snprintf(out_json + pos, json_size - pos, "]");
    } else {
        out_json[json_size - 1] = '\0';
        if (json_size > 2) out_json[json_size - 2] = ']';
    }
    
    unlock();
    return ESP_OK;
}

