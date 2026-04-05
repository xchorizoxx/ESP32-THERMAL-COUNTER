#include "usb_network.hpp"
#include "esp_log.h"
#include "esp_netif.h"
#include "tinyusb.h"
#include "tinyusb_net.h"

static const char *TAG = "UsbNetwork";
static bool is_initialized = false;

namespace UsbNetwork {

// We must export this directly for the TinyUSB stack linker.
extern "C" {
    uint8_t tud_network_mac_address[6] = {0x02, 0x02, 0x84, 0x6A, 0x96, 0x00};
}

// Dummy packet consumer to avoid memory leaks or crashes when the PC sends data.
static esp_err_t dummy_rx_cb(void *buffer, uint16_t len, void *ctx) {
    return ESP_OK;
}
static void dummy_tx_free_cb(void *eb, void *ctx) {
}

bool init() {
    if (is_initialized) return true;

    ESP_LOGI(TAG, "Starting TinyUSB ECM/RNDIS Network interface...");

    // 1. Initialize the TinyUSB Network bridge (links tinyusb_net.c object)
    tinyusb_net_config_t net_cfg = {};
    memcpy(net_cfg.mac_addr, tud_network_mac_address, 6);
    net_cfg.on_recv_callback = dummy_rx_cb;
    net_cfg.free_tx_buffer = dummy_tx_free_cb;
    
    // We ignore error in case it's somehow already initialized
    tinyusb_net_init(&net_cfg);

    // 2. Install the raw USB driver
    const tinyusb_config_t tusb_cfg = {};
    esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TinyUSB driver: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "TinyUSB networking initialized (IP typically 192.168.4.1/192.168.7.1 depending on netif)");
    is_initialized = true;
    return true;
}

} // namespace UsbNetwork
