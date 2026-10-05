#pragma once

#include <cstdint>

#include "esp_err.h"
#include "lvgl.h"

#include "epd_ssd1681.h"
#include "ui/bin_screen.h"

// Renders screens with LVGL and pushes them to the e-Paper panel.
//
// Each show call is self-contained: it powers the panel up, renders, runs one
// full refresh, then puts the panel to sleep and cuts its power. The image
// stays on screen with no power.
class DisplayManager {
 public:
  esp_err_t show(const BinScreenData& data);

 private:
  esp_err_t initLvgl();
  static void flush(lv_display_t* display, const lv_area_t* area, uint8_t* px_map);

  EpdSsd1681 epd_;
  lv_display_t* display_ = nullptr;
  uint8_t* frame_ = nullptr;  // 1-bit frame buffer sent to the panel
};
