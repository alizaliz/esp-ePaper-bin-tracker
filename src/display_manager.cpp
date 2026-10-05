#include "display_manager.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "board_power.h"
#include "config.h"
#include "ui/mono_convert.h"

namespace {

constexpr const char* TAG = "display";
constexpr int WIDTH = EpdSsd1681::WIDTH;
constexpr int HEIGHT = EpdSsd1681::HEIGHT;

uint32_t tickMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

}  // namespace

esp_err_t DisplayManager::initLvgl() {
  if (display_ != nullptr) {
    return ESP_OK;
  }

  frame_ = static_cast<uint8_t*>(heap_caps_malloc(EpdSsd1681::FRAME_BYTES, MALLOC_CAP_DMA));
  ESP_RETURN_ON_FALSE(frame_ != nullptr, ESP_ERR_NO_MEM, TAG, "frame buffer alloc failed");

  lv_init();
  lv_tick_set_cb(tickMs);

  // LVGL renders the whole screen in 8-bit greyscale (40KB), then flush()
  // converts it to the panel's 1-bit format.
  const size_t render_bytes = WIDTH * HEIGHT;
  void* render_buf = heap_caps_malloc(render_bytes, MALLOC_CAP_DEFAULT);
  ESP_RETURN_ON_FALSE(render_buf != nullptr, ESP_ERR_NO_MEM, TAG, "render buffer alloc failed");

  display_ = lv_display_create(WIDTH, HEIGHT);
  lv_display_set_color_format(display_, LV_COLOR_FORMAT_L8);
  lv_display_set_buffers(display_, render_buf, nullptr, render_bytes,
                         LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(display_, flush);
  lv_display_set_user_data(display_, this);
  return ESP_OK;
}

void DisplayManager::flush(lv_display_t* display, const lv_area_t* area, uint8_t* px_map) {
  // Full render mode always hands over the whole screen in one call.
  LV_UNUSED(area);
  auto* self = static_cast<DisplayManager*>(lv_display_get_user_data(display));
  if (DISPLAY_DITHERING) {
    mono::dither(px_map, self->frame_, WIDTH, HEIGHT);
  } else {
    mono::threshold(px_map, self->frame_, WIDTH, HEIGHT);
  }
  lv_display_flush_ready(display);
}

esp_err_t DisplayManager::show(const BinScreenData& data) {
  ESP_RETURN_ON_ERROR(initLvgl(), TAG, "LVGL init failed");

  lv_obj_t* screen = createBinScreen(data);
  lv_screen_load(screen);
  lv_refr_now(display_);
  lv_obj_delete(screen);

  ESP_RETURN_ON_ERROR(board_power::init(), TAG, "board power init failed");
  ESP_RETURN_ON_ERROR(board_power::setEpdPower(true), TAG, "panel power on failed");

  esp_err_t err = epd_.init();
  if (err == ESP_OK) {
    err = epd_.showFrame(frame_);
  }
  // Sleep and power down the panel even if the refresh failed.
  epd_.sleep();
  board_power::setEpdPower(false);
  return err;
}
