#include "board_power.h"

#include "driver/i2c_master.h"
#include "esp_check.h"

#include "board_pins.h"

namespace board_power {
namespace {

constexpr const char* TAG = "board_power";

// TCA9554 registers. A set bit in the config register makes the pin an input.
constexpr uint8_t REG_OUTPUT = 0x01;
constexpr uint8_t REG_CONFIG = 0x03;
constexpr int I2C_TIMEOUT_MS = 100;

i2c_master_bus_handle_t bus = nullptr;
i2c_master_dev_handle_t expander = nullptr;

esp_err_t readReg(uint8_t reg, uint8_t& value) {
  return i2c_master_transmit_receive(expander, &reg, 1, &value, 1, I2C_TIMEOUT_MS);
}

esp_err_t writeReg(uint8_t reg, uint8_t value) {
  const uint8_t buf[2] = {reg, value};
  return i2c_master_transmit(expander, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

// Drives an expander pin as an output. The registers are written directly
// rather than through a driver that resets the chip on init: the expander
// stays powered through deep sleep, and a reset would briefly release the
// battery hold pin and could cut power on battery.
esp_err_t setOutput(int pin, bool high) {
  uint8_t output = 0;
  uint8_t config = 0;
  ESP_RETURN_ON_ERROR(readReg(REG_OUTPUT, output), TAG, "read output reg failed");
  ESP_RETURN_ON_ERROR(readReg(REG_CONFIG, config), TAG, "read config reg failed");

  output = high ? (output | (1 << pin)) : (output & ~(1 << pin));
  // Set the level before switching the pin to an output so it never glitches.
  ESP_RETURN_ON_ERROR(writeReg(REG_OUTPUT, output), TAG, "write output reg failed");
  return writeReg(REG_CONFIG, config & ~(1 << pin));
}

}  // namespace

esp_err_t init() {
  if (expander != nullptr) {
    return ESP_OK;
  }

  i2c_master_bus_config_t bus_config = {};
  bus_config.i2c_port = BOARD_I2C_PORT;
  bus_config.sda_io_num = BOARD_I2C_SDA_PIN;
  bus_config.scl_io_num = BOARD_I2C_SCL_PIN;
  bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
  bus_config.glitch_ignore_cnt = 7;
  bus_config.flags.enable_internal_pullup = true;
  ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), TAG, "I2C bus init failed");

  i2c_device_config_t dev_config = {};
  dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dev_config.device_address = TCA9554_I2C_ADDRESS;
  dev_config.scl_speed_hz = 400000;
  ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_config, &expander), TAG,
                      "TCA9554 add failed");

  return setOutput(EXIO_BATTERY_HOLD, true);
}

esp_err_t setEpdPower(bool on) {
  ESP_RETURN_ON_FALSE(expander != nullptr, ESP_ERR_INVALID_STATE, TAG, "init() not called");
  return setOutput(EXIO_EPD_POWER, on);
}

}  // namespace board_power
