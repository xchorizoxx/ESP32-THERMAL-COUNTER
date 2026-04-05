idf_component_register(SRCS "src/usb_network.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_tinyusb esp_netif)
