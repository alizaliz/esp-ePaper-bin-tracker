#pragma once

#include "esp_err.h"

// Sensirion SHTC3 temperature and humidity sensor on the board's I2C bus.
namespace shtc3 {

// Takes one measurement. board_power::init() must have been called first so
// the I2C bus exists. The sensor is put back to sleep afterwards.
esp_err_t read(float& temperature_c, float& humidity_pct);

}  // namespace shtc3
