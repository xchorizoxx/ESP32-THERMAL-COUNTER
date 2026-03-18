#include "background_model.hpp"
#include <cstring>

void BackgroundModel::update(const float* frame, float* fondo,
                             const uint8_t* mascara, int totalPixels, float alpha)
{
    const float oneMinusAlpha = 1.0f - alpha;

    for (int i = 0; i < totalPixels; i++) {
        if (mascara[i] == 0) {
            fondo[i] = alpha * frame[i] + oneMinusAlpha * fondo[i];
        }
        // Si mascara[i] == 1 (bloqueado por track), fondo[i] permanece inmutable
    }
}

void BackgroundModel::initialize(const float* frame, float* fondo, int totalPixels)
{
    memcpy(fondo, frame, totalPixels * sizeof(float));
}
