/**
 * @file fov_correction.hpp
 * @brief Geometric compensation for the MLX90640 fisheye lens.
 *
 * Flattens the 110°x75° (or 110°x70°) FOV into a linear ground projection 
 * mapping sub-pixels to their physically equidistant locations.
 */

#pragma once

namespace FovCorrection {

    /**
     * @brief Initialize or recompute the lookup table based on sensor height.
     * @param height_m The height of the sensor from the ground in meters.
     */
    void init(float height_m);

    /**
     * @brief Apply bilinear geometrical interpolation to a sub-pixel raw centroid.
     * @param cx_raw In/Out: Sub-pixel X coordinate (cols).
     * @param cy_raw In/Out: Sub-pixel Y coordinate (rows).
     */
    void correct(float& cx_raw, float& cy_raw);

    /**
     * @brief Calculates how many grid pixels represent 1 physical meter 
     * in the normalized ground plane.
     * @return Pixels per meter conversion factor
     */
    float getPixelsPerMeter();

} // namespace FovCorrection
