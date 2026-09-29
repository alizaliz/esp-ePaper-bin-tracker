#pragma once

#include <cstdint>

#include "lvgl.h"

// Images in src/assets/ are embedded into the firmware at build time (see
// src/CMakeLists.txt) and decoded by LVGL's PNG decoder when drawn.
//
// To add one: drop a PNG in src/assets/, then declare it here with
// ASSET_PNG(<file name with '.' replaced by '_'>), e.g. my_icon.png -> my_icon_png.
#define ASSET_PNG(name)                                                  \
  extern const uint8_t name##_start[] asm("_binary_" #name "_start"); \
  extern const uint8_t name##_end[] asm("_binary_" #name "_end");

ASSET_PNG(bin_png)

// Wraps embedded PNG bytes in an LVGL image descriptor for lv_image_set_src().
// The descriptor must outlive the image object that uses it.
inline lv_image_dsc_t assetImage(const uint8_t* start, const uint8_t* end) {
  lv_image_dsc_t dsc = {};
  dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc.header.cf = LV_COLOR_FORMAT_RAW;
  dsc.data = start;
  dsc.data_size = static_cast<uint32_t>(end - start);
  return dsc;
}
