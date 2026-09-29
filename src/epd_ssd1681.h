#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"

// Driver for the board's 1.54" 200x200 black and white e-Paper panel
// (SSD1681-style controller). It only does full refreshes using the panel's
// built-in waveform, which suits a display that updates once per wake.
//
// Frame buffer layout: 1 bit per pixel, rows of 25 bytes, most significant bit
// is the leftmost pixel, 1 = white, 0 = black.
class EpdSsd1681 {
 public:
  static constexpr int WIDTH = 200;
  static constexpr int HEIGHT = 200;
  static constexpr size_t FRAME_BYTES = WIDTH * HEIGHT / 8;

  // Sets up SPI and the control pins, then resets and configures the panel.
  // The panel must already be powered (board_power::setEpdPower).
  esp_err_t init();

  // Sends a frame and runs a full refresh (about 2 seconds). The buffer must be
  // DMA capable and FRAME_BYTES long.
  esp_err_t showFrame(const uint8_t* frame);

  // Puts the panel into deep sleep. The image stays on screen; a hardware
  // reset (init) is needed before the next refresh.
  esp_err_t sleep();

 private:
  esp_err_t command(uint8_t cmd, const uint8_t* data = nullptr, size_t len = 0);
  esp_err_t waitWhileBusy();
  void hardwareReset();

  esp_lcd_panel_io_handle_t io_ = nullptr;
};
