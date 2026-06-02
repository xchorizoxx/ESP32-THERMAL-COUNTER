#pragma once

#include "thermal_types.hpp"
#include <stdint.h>

class ITftView {
public:
    virtual ~ITftView() = default;

    virtual void onEnter() = 0;

    virtual void onExit() = 0;

    virtual void render(const TftSnapshot& snap,
                        uint16_t* fb, int width, int height) = 0;

    virtual const char* name() const = 0;
};
