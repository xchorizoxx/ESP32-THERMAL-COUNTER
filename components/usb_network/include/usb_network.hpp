#pragma once
#include <stdbool.h>

namespace UsbNetwork {
    /**
     * @brief Initiates TinyUSB in Network/ECM Mode.
     * This will enumerate as a USB Ethernet adapter on the PC.
     * @return true if successfully initialized, false otherwise.
     */
    bool init();
}
