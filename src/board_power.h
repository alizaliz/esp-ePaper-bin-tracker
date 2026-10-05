#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

// Power switching on the Waveshare board, done through the TCA9554 I/O
// expander on the I2C bus.
namespace board_power {

// Brings up the I2C bus and drives the battery power hold high so the board
// stays on when running from a battery.
esp_err_t init();

// Switches the e-Paper panel's supply on or off.
esp_err_t setEpdPower(bool on);

// The board's shared I2C bus, or nullptr before init().
i2c_master_bus_handle_t i2cBus();

}  // namespace board_power
