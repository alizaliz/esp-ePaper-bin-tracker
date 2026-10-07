// Renders the device screens on a computer and writes them as PNGs, using the
// same LVGL layout code and black and white conversion as the firmware.
// Build and run with tools/preview/run.sh.

#include <zlib.h>

#include <cstdio>
#include <string>
#include <vector>

#include "lvgl.h"

#include "mascot.h"
#include "ui/bin_screen.h"
#include "ui/mono_convert.h"

namespace {

constexpr int WIDTH = 200;
constexpr int HEIGHT = 200;

std::vector<uint8_t> grey(WIDTH * HEIGHT);

void flush(lv_display_t* display, const lv_area_t*, uint8_t* px_map) {
  std::copy(px_map, px_map + grey.size(), grey.begin());
  lv_display_flush_ready(display);
}

void writeChunk(FILE* f, const char* type, const std::vector<uint8_t>& data) {
  const uint8_t len[4] = {uint8_t(data.size() >> 24), uint8_t(data.size() >> 16),
                          uint8_t(data.size() >> 8), uint8_t(data.size())};
  fwrite(len, 1, 4, f);
  fwrite(type, 1, 4, f);
  fwrite(data.data(), 1, data.size(), f);
  uLong crc = crc32(0, reinterpret_cast<const Bytef*>(type), 4);
  crc = crc32(crc, data.data(), data.size());
  const uint8_t c[4] = {uint8_t(crc >> 24), uint8_t(crc >> 16), uint8_t(crc >> 8), uint8_t(crc)};
  fwrite(c, 1, 4, f);
}

// Writes the 1-bit frame as a greyscale PNG, scaled up so it's easy to inspect.
void writePng(const std::string& path, const std::vector<uint8_t>& frame, int scale) {
  const int w = WIDTH * scale;
  const int h = HEIGHT * scale;
  std::vector<uint8_t> raw;
  raw.reserve((w + 1) * h);
  for (int y = 0; y < h; ++y) {
    raw.push_back(0);  // no filter
    for (int x = 0; x < w; ++x) {
      const int sx = x / scale;
      const int sy = y / scale;
      const bool white = frame[sy * (WIDTH / 8) + sx / 8] & (0x80 >> (sx % 8));
      raw.push_back(white ? 255 : 0);
    }
  }
  uLongf size = compressBound(raw.size());
  std::vector<uint8_t> idat(size);
  compress(idat.data(), &size, raw.data(), raw.size());
  idat.resize(size);

  std::vector<uint8_t> ihdr = {uint8_t(w >> 24), uint8_t(w >> 16), uint8_t(w >> 8), uint8_t(w),
                               uint8_t(h >> 24), uint8_t(h >> 16), uint8_t(h >> 8), uint8_t(h),
                               8, 0, 0, 0, 0};
  FILE* f = fopen(path.c_str(), "wb");
  const uint8_t sig[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  fwrite(sig, 1, sizeof(sig), f);
  writeChunk(f, "IHDR", ihdr);
  writeChunk(f, "IDAT", idat);
  writeChunk(f, "IEND", {});
  fclose(f);
}

void render(lv_display_t* display, const BinScreenData& data, const std::string& path) {
  lv_obj_t* screen = createBinScreen(data);
  lv_screen_load(screen);
  lv_refr_now(display);

  // Warn about anything drawn outside the screen.
  for (uint32_t i = 0; i < lv_obj_get_child_count(screen); ++i) {
    lv_area_t a;
    lv_obj_get_coords(lv_obj_get_child(screen, i), &a);
    if (a.x1 < 0 || a.y1 < 0 || a.x2 >= WIDTH || a.y2 >= HEIGHT) {
      printf("WARNING: %s: element %u at (%d,%d)-(%d,%d) is off screen\n", path.c_str(),
             (unsigned)i, (int)a.x1, (int)a.y1, (int)a.x2, (int)a.y2);
    }
  }

  std::vector<uint8_t> frame(WIDTH * HEIGHT / 8);
  mono::threshold(grey.data(), frame.data(), WIDTH, HEIGHT);
  writePng(path, frame, 2);
  printf("wrote %s\n", path.c_str());
}

}  // namespace

int main(int argc, char** argv) {
  const std::string out = argc > 1 ? argv[1] : ".";

  lv_init();
  lv_display_t* display = lv_display_create(WIDTH, HEIGHT);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_L8);
  static std::vector<uint8_t> buf(WIDTH * HEIGHT);
  lv_display_set_buffers(display, buf.data(), nullptr, buf.size(), LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(display, flush);

  BinScreenData upcoming;
  upcoming.pickup = {2026, 10, 8};
  upcoming.rubbish = true;
  upcoming.foodScraps = true;
  upcoming.wifiConnected = true;
  upcoming.hasClimate = true;
  upcoming.temperatureC = 14;
  upcoming.humidityPct = 72;
  render(display, upcoming, out + "/upcoming.png");

  BinScreenData next_day = upcoming;  // the daily face change
  next_day.dayNumber = 1;
  render(display, next_day, out + "/upcoming_next_day.png");

  BinScreenData tonight = upcoming;
  tonight.isTonight = true;
  tonight.recycling = true;
  render(display, tonight, out + "/tonight.png");

  BinScreenData today = tonight;
  today.isTonight = false;
  today.isToday = true;
  render(display, today, out + "/today.png");

  BinScreenData widest = upcoming;  // widest date text and readings
  widest.pickup = {2026, 9, 30};
  widest.recycling = true;
  widest.temperatureC = -9;
  widest.humidityPct = 99;
  render(display, widest, out + "/widest.png");

  BinScreenData stale = upcoming;  // schedule out of date
  stale.isStale = true;
  render(display, stale, out + "/stale.png");

  BinScreenData low = upcoming;  // low battery
  low.batteryLow = true;
  render(display, low, out + "/low_battery.png");

  BinScreenData offline = upcoming;  // every problem at once
  offline.wifiConnected = false;
  offline.batteryLow = true;
  offline.isStale = true;
  offline.hasClimate = false;
  render(display, offline, out + "/offline.png");
  // Each of the mascot's faces on its own, at the top left of a blank
  // screen, for the README diagram.
  const struct {
    const char* name;
    const lv_image_dsc_t* image;
  } faces[] = {{"happy", &mascot_emoji_happy},   {"wink", &mascot_emoji_wink},
               {"excited", &mascot_emoji_excited}, {"proud", &mascot_emoji_proud},
               {"worried", &mascot_emoji_worried}, {"sleepy", &mascot_emoji_sleepy}};
  for (const auto& face : faces) {
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_t* image = lv_image_create(screen);
    lv_image_set_src(image, face.image);
    lv_obj_set_pos(image, 0, 0);
    lv_screen_load(screen);
    lv_refr_now(display);
    std::vector<uint8_t> frame(WIDTH * HEIGHT / 8);
    mono::threshold(grey.data(), frame.data(), WIDTH, HEIGHT);
    writePng(out + "/face_" + face.name + ".png", frame, 2);
    lv_obj_delete(screen);
  }
  return 0;
}
