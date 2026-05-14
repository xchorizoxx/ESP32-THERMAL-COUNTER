#include "background_model.hpp"
#include <cstring>

void BackgroundModel::update(const float* frame, float* background,
                             const uint8_t* mask, int totalPixels, float alpha)
{
    const float oneMinusAlpha = 1.0f - alpha;
    const float maskedAlpha   = alpha * 0.1f;   // 10× slower adaptation under tracks
    const float oneMinusMasked = 1.0f - maskedAlpha;

    for (int i = 0; i < totalPixels; i++) {
        if (mask[i] == 0) {
            background[i] = alpha * frame[i] + oneMinusAlpha * background[i];
        } else {
            // Slow drift correction even under tracks — prevents false positives
            // when people exit a fully occupied FOV
            background[i] = maskedAlpha * frame[i] + oneMinusMasked * background[i];
        }
    }
}

void BackgroundModel::initialize(const float* frame, float* background, int totalPixels)
{
    memcpy(background, frame, totalPixels * sizeof(float));
}
