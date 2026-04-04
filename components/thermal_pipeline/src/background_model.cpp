#include "background_model.hpp"
#include <cstring>

void BackgroundModel::update(const float* frame, float* background,
                             const uint8_t* mask, int totalPixels, float alpha)
{
    const float oneMinusAlpha = 1.0f - alpha;

    for (int i = 0; i < totalPixels; i++) {
        if (mask[i] == 0) {
            background[i] = alpha * frame[i] + oneMinusAlpha * background[i];
        }
        // If mask[i] == 1 (blocked by track), background[i] remains immutable
    }
}

void BackgroundModel::initialize(const float* frame, float* background, int totalPixels)
{
    memcpy(background, frame, totalPixels * sizeof(float));
}
