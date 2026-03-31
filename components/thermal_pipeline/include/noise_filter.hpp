#pragma once
/**
 * @file noise_filter.hpp
 * @brief Per-pixel 1D Kalman filter for thermal noise reduction.
 *
 * Applies an independent scalar Kalman filter to each of the
 * ThermalConfig::TOTAL_PIXELS (768) pixels of the MLX90640 sensor.
 *
 * Assumed model:
 *   - Process:     temperature is approximately constant between frames
 *   - Process noise Q: variance of the actual thermal change per frame
 *   - Measurement noise R: sensor variance (NETD²)
 *
 * Default values (MLX90640BAA at 16 FPS):
 *   NETD typical ≈ 0.5 K  →  R = 0.5² = 0.25
 *   Real scene change ≈ 0.1 K/frame  →  Q = 0.01
 *
 * Result: smoother image without significant latency increase (~1 frame lag).
 *
 * Memory: 768 × 2 floats = 6 KB (class member, no PSRAM required).
 */

#include <cstring>
#include "thermal_config.hpp"

class NoiseFilter {
public:
    static constexpr float DEFAULT_Q = 0.01f;   ///< Process noise variance
    static constexpr float DEFAULT_R = 0.25f;   ///< Measurement noise variance (NETD² @ 16 Hz)
    static constexpr float INIT_P    = 1.0f;    ///< Initial covariance (high uncertainty)

    NoiseFilter() : q_(DEFAULT_Q), r_(DEFAULT_R), initialized_(false)
    {
        memset(x_, 0, sizeof(x_));
        for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
            p_[i] = INIT_P;
        }
    }

    /**
     * @brief Adjusts the filter parameters.
     *
     * Call before the first apply() invocation if values other than
     * the defaults are needed.
     *
     * @param q  Process noise variance (lower Q = stronger smoothing)
     * @param r  Measurement noise variance (higher R = more trust in estimate)
     */
    void setParameters(float q, float r) { q_ = q; r_ = r; }

    /**
     * @brief Seeds the filter state with the first composed frame.
     *
     * Sets x_[i] = frame[i] and p_[i] = INIT_P for all pixels.
     * Called automatically from apply() if not yet initialized.
     *
     * @param frame First valid composed frame [TOTAL_PIXELS floats]
     */
    void init(const float* frame)
    {
        memcpy(x_, frame, ThermalConfig::TOTAL_PIXELS * sizeof(float));
        for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
            p_[i] = INIT_P;
        }
        initialized_ = true;
    }

    /**
     * @brief Applies the Kalman filter to the input frame.
     *
     * Scalar Kalman equations per pixel:
     *   Predict:  p_pred = p[i] + Q
     *   Gain:     k = p_pred / (p_pred + R)
     *   Update:   x[i] = x[i] + k * (z - x[i])
     *             p[i] = (1 - k) * p_pred
     *
     * On first call (uninitialized), calls init() and returns frame_in unchanged.
     *
     * @param frame_in  Composed input frame  [TOTAL_PIXELS floats]
     * @param frame_out Filtered output frame [TOTAL_PIXELS floats]
     *                  (may alias frame_in)
     */
    void apply(const float* frame_in, float* frame_out)
    {
        if (!initialized_) {
            init(frame_in);
            memcpy(frame_out, frame_in, ThermalConfig::TOTAL_PIXELS * sizeof(float));
            return;
        }

        for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
            // Predict
            const float p_pred = p_[i] + q_;

            // Kalman gain
            const float k = p_pred / (p_pred + r_);

            // Update state and covariance
            x_[i] = x_[i] + k * (frame_in[i] - x_[i]);
            p_[i] = (1.0f - k) * p_pred;

            frame_out[i] = x_[i];
        }
    }

    /**
     * @brief Resets the filter to the uninitialized state.
     *
     * Call after sensor re-initialization (ConfigCmdType::RETRY_SENSOR)
     * so the first post-reset frame is used as a fresh initial condition.
     */
    void reset()
    {
        initialized_ = false;
        for (int i = 0; i < ThermalConfig::TOTAL_PIXELS; i++) {
            p_[i] = INIT_P;
        }
    }

    bool isInitialized() const { return initialized_; }

private:
    float x_[ThermalConfig::TOTAL_PIXELS];  ///< Estimated state (temperature per pixel)
    float p_[ThermalConfig::TOTAL_PIXELS];  ///< Error covariance estimate
    float q_;                               ///< Process noise
    float r_;                               ///< Measurement noise
    bool  initialized_;
};
