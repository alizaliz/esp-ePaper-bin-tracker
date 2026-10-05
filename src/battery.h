#pragma once

#include "esp_err.h"

// Lithium battery voltage, read through the board's 1:2 divider on GPIO0.
namespace battery {

esp_err_t readVoltage(float& volts);

// Approximate state of charge (0-100) from a resting single-cell lithium
// voltage. Readings while charging run high.
int percentFromVoltage(float volts);

}  // namespace battery
