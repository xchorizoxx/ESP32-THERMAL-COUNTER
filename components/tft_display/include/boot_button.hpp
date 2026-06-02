#pragma once

#include <stdint.h>

class BootButton {
public:
    BootButton();
    ~BootButton();

    bool init();
    bool poll();

private:
    bool m_initialized;
    int m_lastState;
    uint32_t m_pressStartMs;
};
