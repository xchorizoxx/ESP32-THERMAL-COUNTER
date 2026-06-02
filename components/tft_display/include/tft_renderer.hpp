#pragma once

#include <stdint.h>

class TftRenderer {
public:
    TftRenderer();
    ~TftRenderer();

    bool init();
    void renderHeatmap(const int16_t* srcPixels,
                       uint16_t* fb,
                       int srcW, int srcH,
                       int dstW, int dstH);

private:
    bool m_initialized;
};
