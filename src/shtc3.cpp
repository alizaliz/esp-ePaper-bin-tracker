#include "shtc3.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_power.h"

namespace shtc3 {
namespace {

constexpr const char* TAG = "shtc3";
constexpr uint8_t I2C_ADDRESS = 0x70;
constexpr int I2C_TIMEOUT_MS = 100;

// Commands from the SHTC3 datasheet
constexpr uint16_t CMD_WAKEUP = 0x3517;
constexpr uint16_t CMD_SLEEP = 0xB098;
constexpr uint16_t CMD_MEASURE_T_FIRST = 0x7866;  // normal mode, no clock stretching

i2c_master_dev_handle_t device = nullptr;

esp_err_t sendCommand(uint16_t cmd) {
  const uint8_t buf[2] = {uint8_t(cmd >> 8), uint8_t(cmd & 0xFF)};
  return i2c_master_transmit(device, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

// CRC-8, polynomial 0x31, initial value 0xFF
uint8_t crc8(const uint8_t* data, int len) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x31) : uint8_t(crc << 1);
    }
  }
  return crc;
}

}  // namespace

esp_err_t read(float& temperature_c, float& humidity_pct) {
  if (device == nullptr) {
    i2c_master_bus_handle_t bus = board_power::i2cBus();
    ESP_RETURN_ON_FALSE(bus != nullptr, ESP_ERR_INVALID_STATE, TAG, "I2C bus not initialised");
    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = I2C_ADDRESS;
    config.scl_speed_hz = 400000;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &config, &device), TAG, "add failed");
  }

  ESP_RETURN_ON_ERROR(sendCommand(CMD_WAKEUP), TAG, "wakeup failed");
  esp_rom_delay_us(300);  // 240us max
  ESP_RETURN_ON_ERROR(sendCommand(CMD_MEASURE_T_FIRST), TAG, "measure failed");
  // 12.1ms max in normal mode. The FreeRTOS tick is 10ms and vTaskDelay can
  // return up to one tick early, so ask for 30ms to be sure of at least 20ms.
  vTaskDelay(pdMS_TO_TICKS(30));

  uint8_t data[6];
  const esp_err_t err = i2c_master_receive(device, data, sizeof(data), I2C_TIMEOUT_MS);
  sendCommand(CMD_SLEEP);
  ESP_RETURN_ON_ERROR(err, TAG, "read failed");
  ESP_RETURN_ON_FALSE(crc8(&data[0], 2) == data[2] && crc8(&data[3], 2) == data[5],
                      ESP_ERR_INVALID_CRC, TAG, "CRC mismatch");

  const uint16_t raw_t = (data[0] << 8) | data[1];
  const uint16_t raw_rh = (data[3] << 8) | data[4];
  temperature_c = -45.0f + 175.0f * raw_t / 65536.0f;
  humidity_pct = 100.0f * raw_rh / 65536.0f;
  return ESP_OK;
}

}  // namespace shtc3
