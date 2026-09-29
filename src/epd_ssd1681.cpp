#include "epd_ssd1681.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_io_spi.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_pins.h"

namespace {

constexpr const char* TAG = "epd";

// SSD1681 commands
constexpr uint8_t CMD_DRIVER_OUTPUT = 0x01;
constexpr uint8_t CMD_DEEP_SLEEP = 0x10;
constexpr uint8_t CMD_DATA_ENTRY_MODE = 0x11;
constexpr uint8_t CMD_SW_RESET = 0x12;
constexpr uint8_t CMD_TEMP_SENSOR = 0x18;
constexpr uint8_t CMD_MASTER_ACTIVATION = 0x20;
constexpr uint8_t CMD_UPDATE_CONTROL_2 = 0x22;
constexpr uint8_t CMD_WRITE_RAM_BW = 0x24;
constexpr uint8_t CMD_WRITE_RAM_PREVIOUS = 0x26;
constexpr uint8_t CMD_BORDER_WAVEFORM = 0x3C;
constexpr uint8_t CMD_RAM_X_RANGE = 0x44;
constexpr uint8_t CMD_RAM_Y_RANGE = 0x45;
constexpr uint8_t CMD_RAM_X_COUNTER = 0x4E;
constexpr uint8_t CMD_RAM_Y_COUNTER = 0x4F;

// Full update sequence using the waveform stored in the panel's OTP memory.
constexpr uint8_t UPDATE_FULL = 0xF7;

constexpr int SPI_CLOCK_HZ = 20 * 1000 * 1000;  // SSD1681 write limit
constexpr int64_t BUSY_TIMEOUT_US = 10 * 1000 * 1000;

}  // namespace

esp_err_t EpdSsd1681::init() {
  if (io_ == nullptr) {
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = EPD_MOSI_PIN;
    bus_config.sclk_io_num = EPD_SCK_PIN;
    bus_config.miso_io_num = -1;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = FRAME_BYTES;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(EPD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO), TAG,
                        "SPI bus init failed");

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = EPD_CS_PIN;
    io_config.dc_gpio_num = EPD_DC_PIN;
    io_config.pclk_hz = SPI_CLOCK_HZ;
    io_config.spi_mode = 0;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.trans_queue_depth = 4;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EPD_SPI_HOST, &io_config, &io_),
        TAG, "panel IO init failed");

    gpio_config_t rst_config = {};
    rst_config.pin_bit_mask = 1ULL << EPD_RST_PIN;
    rst_config.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&rst_config), TAG, "RST pin config failed");

    gpio_config_t busy_config = {};
    busy_config.pin_bit_mask = 1ULL << EPD_BUSY_PIN;
    busy_config.mode = GPIO_MODE_INPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&busy_config), TAG, "BUSY pin config failed");
  }

  hardwareReset();
  ESP_RETURN_ON_ERROR(waitWhileBusy(), TAG, "panel did not come out of reset");
  ESP_RETURN_ON_ERROR(command(CMD_SW_RESET), TAG, "SW reset failed");
  ESP_RETURN_ON_ERROR(waitWhileBusy(), TAG, "SW reset timed out");

  // Register values match Waveshare's driver for this board, which scans
  // gates in reverse and fills RAM with Y decrementing so the image is upright.
  const uint8_t driver_output[] = {(HEIGHT - 1) & 0xFF, (HEIGHT - 1) >> 8, 0x01};
  const uint8_t data_entry[] = {0x01};  // X increment, Y decrement
  const uint8_t x_range[] = {0x00, (WIDTH / 8) - 1};
  const uint8_t y_range[] = {(HEIGHT - 1) & 0xFF, (HEIGHT - 1) >> 8, 0x00, 0x00};
  const uint8_t border[] = {0x01};
  const uint8_t temp_sensor[] = {0x80};  // internal sensor
  const uint8_t x_counter[] = {0x00};
  const uint8_t y_counter[] = {(HEIGHT - 1) & 0xFF, (HEIGHT - 1) >> 8};

  ESP_RETURN_ON_ERROR(command(CMD_DRIVER_OUTPUT, driver_output, sizeof(driver_output)), TAG, "");
  ESP_RETURN_ON_ERROR(command(CMD_DATA_ENTRY_MODE, data_entry, sizeof(data_entry)), TAG, "");
  ESP_RETURN_ON_ERROR(command(CMD_RAM_X_RANGE, x_range, sizeof(x_range)), TAG, "");
  ESP_RETURN_ON_ERROR(command(CMD_RAM_Y_RANGE, y_range, sizeof(y_range)), TAG, "");
  ESP_RETURN_ON_ERROR(command(CMD_BORDER_WAVEFORM, border, sizeof(border)), TAG, "");
  ESP_RETURN_ON_ERROR(command(CMD_TEMP_SENSOR, temp_sensor, sizeof(temp_sensor)), TAG, "");
  ESP_RETURN_ON_ERROR(command(CMD_RAM_X_COUNTER, x_counter, sizeof(x_counter)), TAG, "");
  ESP_RETURN_ON_ERROR(command(CMD_RAM_Y_COUNTER, y_counter, sizeof(y_counter)), TAG, "");
  return waitWhileBusy();
}

esp_err_t EpdSsd1681::showFrame(const uint8_t* frame) {
  // Write both RAM banks so a later partial refresh starts from this image.
  ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io_, CMD_WRITE_RAM_BW, frame, FRAME_BYTES), TAG,
                      "RAM write failed");
  ESP_RETURN_ON_ERROR(
      esp_lcd_panel_io_tx_color(io_, CMD_WRITE_RAM_PREVIOUS, frame, FRAME_BYTES), TAG,
      "RAM write failed");

  const uint8_t update[] = {UPDATE_FULL};
  ESP_RETURN_ON_ERROR(command(CMD_UPDATE_CONTROL_2, update, sizeof(update)), TAG, "");
  ESP_RETURN_ON_ERROR(command(CMD_MASTER_ACTIVATION), TAG, "");
  return waitWhileBusy();
}

esp_err_t EpdSsd1681::sleep() {
  if (io_ == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  const uint8_t mode[] = {0x01};
  return command(CMD_DEEP_SLEEP, mode, sizeof(mode));
}

esp_err_t EpdSsd1681::command(uint8_t cmd, const uint8_t* data, size_t len) {
  return esp_lcd_panel_io_tx_param(io_, cmd, data, len);
}

esp_err_t EpdSsd1681::waitWhileBusy() {
  // BUSY is high while the controller is working. Time out rather than spin
  // forever and drain the battery if the panel is missing or unpowered.
  const int64_t start = esp_timer_get_time();
  while (gpio_get_level(EPD_BUSY_PIN) == 1) {
    if (esp_timer_get_time() - start > BUSY_TIMEOUT_US) {
      ESP_LOGE(TAG, "timed out waiting for BUSY");
      return ESP_ERR_TIMEOUT;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  return ESP_OK;
}

void EpdSsd1681::hardwareReset() {
  gpio_set_level(EPD_RST_PIN, 1);
  vTaskDelay(pdMS_TO_TICKS(20));
  gpio_set_level(EPD_RST_PIN, 0);
  vTaskDelay(pdMS_TO_TICKS(10));
  gpio_set_level(EPD_RST_PIN, 1);
  vTaskDelay(pdMS_TO_TICKS(20));
}
