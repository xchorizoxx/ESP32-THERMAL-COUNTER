#pragma once

#include "thermal_config.hpp"

namespace TftConfig {

constexpr int WIDTH  = ThermalConfig::TFT_WIDTH;
constexpr int HEIGHT = ThermalConfig::TFT_HEIGHT;
constexpr int PIXEL_COUNT = WIDTH * HEIGHT;

constexpr int FB_SIZE_BYTES = PIXEL_COUNT * 2;

constexpr int SRC_COLS = ThermalConfig::MLX_COLS;
constexpr int SRC_ROWS = ThermalConfig::MLX_ROWS;

constexpr int TARGET_FPS = 15;
constexpr int FRAME_PERIOD_MS = 1000 / TARGET_FPS;

constexpr int PALETTE_SIZE = 256;
constexpr float TEMP_RANGE_MIN = 15.0f;
constexpr float TEMP_RANGE_MAX = 45.0f;

constexpr int BUTTON_GPIO = 0;
constexpr int DEBOUNCE_MS = 50;
constexpr int LONG_PRESS_MS = 1500;

constexpr int TASK_STACK_BYTES = 4096;
constexpr int TASK_PRIORITY = tskIDLE_PRIORITY + 1;
constexpr int TASK_CORE = 0;

constexpr int GRAM_X_OFFSET = 0;
constexpr int GRAM_Y_OFFSET = 0;

} // namespace TftConfig
