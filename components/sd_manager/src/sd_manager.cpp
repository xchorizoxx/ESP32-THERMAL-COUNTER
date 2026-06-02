#include "sd_manager.hpp"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "SD_MANAGER";

// Colored log macros for debug visibility
#define LOG_CYAN    "\033[0;36m"
#define LOG_RESET   "\033[0m"
#define LOG_COLOR(color, tag, format, ...) \
    printf(color "I (%lu) %s: " format LOG_RESET "\n", \
           (unsigned long)esp_log_timestamp(), tag, ##__VA_ARGS__)

SDManager::SDManager() : card_(nullptr), mounted_(false), mutex_(nullptr) {
    mutex_ = xSemaphoreCreateMutex();
}

SDManager::~SDManager() {
    unmount();
}

void SDManager::unmount() {
    lock();
    if (mounted_.load(std::memory_order_relaxed)) {
        ESP_LOGI(TAG, "Unmounting SD card...");
        esp_err_t err = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card_);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(err));
        }
        card_ = nullptr;
        mounted_.store(false, std::memory_order_relaxed);
    }
    unlock();
}

bool SDManager::checkHealth() {
    if (!mounted_.load(std::memory_order_relaxed)) {
        return false;
    }
    
    FATFS* fs;
    DWORD free_clust;
    // We try to call f_getfree as a health check.
    // If it fails with a media-related error, we mark the card as unmounted.
    FRESULT res = f_getfree("0:", &free_clust, &fs);
    if (res != FR_OK) {
        ESP_LOGE(TAG, "SD Card health check failed: f_getfree returned %d. Unmounting.", (int)res);
        unmount();
        return false;
    }
    return true;
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

    FATFS* fs;
    DWORD free_clust;
    FRESULT res = f_getfree("0:", &free_clust, &fs);
    if (res != FR_OK || !fs) {
        LOG_COLOR(LOG_CYAN, TAG, "f_getfree failed: res=%d", (int)res);
        return 0;
    }

    uint64_t bytes = (uint64_t)free_clust * fs->csize * fs->ssize;
    LOG_COLOR(LOG_CYAN, TAG, "f_getfree: free_clust=%lu csize=%u ssize=%u → free=%llu",
              (unsigned long)free_clust, (unsigned)fs->csize,
              (unsigned)fs->ssize, (unsigned long long)bytes);
    return bytes;
}

uint64_t SDManager::getTotalSpaceBytes() const {
    if (!mounted_.load(std::memory_order_relaxed) || !card_) return 0;

    // CSD capacity: for SDHC/SDXC it is in 512-byte blocks
    uint64_t bytes = (uint64_t)card_->csd.capacity * 512ULL;
    LOG_COLOR(LOG_CYAN, TAG, "CSD total: capacity=%lu sector_size=%u → total=%llu",
              (unsigned long)card_->csd.capacity,
              (unsigned)card_->csd.sector_size,
              (unsigned long long)bytes);
    return bytes;
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
        checkHealth();
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    
    if (written != len) {
        unlock();
        checkHealth();
        return ESP_FAIL;
    }
    
    unlock();
    return ESP_OK;
}

esp_err_t SDManager::appendLine(const char* rel_path, const char* line) {
    if (!mounted_.load(std::memory_order_relaxed)) return ESP_ERR_INVALID_STATE;
    lock();
    FILE* f = fopen(toFull(rel_path), "a");
    if (!f) {
        unlock();
        checkHealth();
        return ESP_FAIL;
    }
    
    int written = fprintf(f, "%s\n", line);
    fclose(f);
    
    if (written < 0) {
        unlock();
        checkHealth();
        return ESP_FAIL;
    }
    
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
    // Reserve 128 bytes for the closing bracket + worst-case last entry name
    const size_t reserve = 128;
    
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        // Stop before buffer exhaustion — guarantee valid closing bracket fits
        if (pos + reserve >= json_size) break;

        if (!first) {
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

        pos += snprintf(out_json + pos, json_size - pos, "{\"name\":\"%s\",\"size\":%lu}", entry->d_name, (unsigned long)size);
    }
    closedir(dir);
    
    // Always enough room for the closing bracket (reserved above)
    pos += snprintf(out_json + pos, json_size - pos, "]");
    
    unlock();
    return ESP_OK;
}

