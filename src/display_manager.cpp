#include "display_manager.h"

#include <cstring>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "assets.h"
#include "board_power.h"
#include "config.h"

namespace {

constexpr const char* TAG = "display";
constexpr int WIDTH = EpdSsd1681::WIDTH;
constexpr int HEIGHT = EpdSsd1681::HEIGHT;

uint32_t tickMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

inline void setPixel(uint8_t* frame, int x, int y, bool white) {
  uint8_t& byte = frame[y * (WIDTH / 8) + x / 8];
  const uint8_t mask = 0x80 >> (x % 8);
  byte = white ? (byte | mask) : (byte & ~mask);
}

// Greyscale to 1-bit. A plain threshold keeps text edges crisp.
void thresholdToFrame(const uint8_t* grey, uint8_t* frame) {
  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      setPixel(frame, x, y, grey[y * WIDTH + x] >= 128);
    }
  }
}

// Greyscale to 1-bit with Floyd-Steinberg error diffusion, so photos and
// gradients show as shading instead of flat black and white.
void ditherToFrame(const uint8_t* grey, uint8_t* frame) {
  // Error for the current and next row, padded by one pixel on each side.
  static int16_t rows[2][WIDTH + 2];
  memset(rows, 0, sizeof(rows));
  int16_t* cur = rows[0];
  int16_t* next = rows[1];

  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      const int value = grey[y * WIDTH + x] + cur[x + 1];
      const bool white = value >= 128;
      const int err = value - (white ? 255 : 0);
      setPixel(frame, x, y, white);
      cur[x + 2] += err * 7 / 16;
      next[x] += err * 3 / 16;
      next[x + 1] += err * 5 / 16;
      next[x + 2] += err / 16;
    }
    int16_t* done = cur;
    cur = next;
    next = done;
    memset(next, 0, sizeof(rows[0]));
  }
}

lv_obj_t* addLabel(lv_obj_t* parent, const char* text, const lv_font_t* font) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_width(label, LV_PCT(100));
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
  return label;
}

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
    ditherToFrame(px_map, self->frame_);
  } else {
    thresholdToFrame(px_map, self->frame_);
  }
  lv_display_flush_ready(display);
}

esp_err_t DisplayManager::render(lv_obj_t* screen) {
  lv_screen_load(screen);
  lv_refr_now(display_);

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

esp_err_t DisplayManager::showCollectionDays(const AddressDetails& details,
                                             const std::vector<CollectionDay>& days) {
  ESP_RETURN_ON_ERROR(initLvgl(), TAG, "LVGL init failed");

  lv_obj_t* screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
  lv_obj_set_style_text_color(screen, lv_color_black(), 0);
  lv_obj_set_style_pad_all(screen, 8, 0);
  lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(screen, 2, 0);
  lv_obj_set_scrollable(screen, false);

  // Header: bin icon and title
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_remove_style_all(header);
  lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(header, 8, 0);

  static lv_image_dsc_t bin_icon = assetImage(bin_png_start, bin_png_end);
  lv_obj_t* icon = lv_image_create(header);
  lv_image_set_src(icon, &bin_icon);

  lv_obj_t* title = addLabel(header, "Bin day", &lv_font_montserrat_20);
  lv_obj_set_flex_grow(title, 1);

  addLabel(screen, details.displayAddress.c_str(), &lv_font_montserrat_14);

  lv_obj_t* divider = lv_obj_create(screen);
  lv_obj_remove_style_all(divider);
  lv_obj_set_size(divider, LV_PCT(100), 2);
  lv_obj_set_style_bg_color(divider, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
  lv_obj_set_style_margin_ver(divider, 4, 0);

  for (const auto& day : days) {
    addLabel(screen, day.label.c_str(), &lv_font_montserrat_14);
    lv_obj_t* date = addLabel(screen, day.date.c_str(), &lv_font_montserrat_16);
    lv_obj_set_style_margin_bottom(date, 4, 0);
  }

  const esp_err_t err = render(screen);
  lv_obj_delete(screen);
  return err;
}
