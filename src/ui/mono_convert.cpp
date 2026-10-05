#include "mono_convert.h"

#include <cstring>
#include <vector>

namespace mono {
namespace {

inline void setPixel(uint8_t* frame, int width, int x, int y, bool white) {
  uint8_t& byte = frame[y * (width / 8) + x / 8];
  const uint8_t mask = 0x80 >> (x % 8);
  byte = white ? (byte | mask) : (byte & ~mask);
}

}  // namespace

void threshold(const uint8_t* grey, uint8_t* frame, int width, int height) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      setPixel(frame, width, x, y, grey[y * width + x] >= 128);
    }
  }
}

void dither(const uint8_t* grey, uint8_t* frame, int width, int height) {
  // Error for the current and next row, padded by one pixel on each side.
  std::vector<int16_t> cur(width + 2, 0);
  std::vector<int16_t> next(width + 2, 0);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int value = grey[y * width + x] + cur[x + 1];
      const bool white = value >= 128;
      const int err = value - (white ? 255 : 0);
      setPixel(frame, width, x, y, white);
      cur[x + 2] += err * 7 / 16;
      next[x] += err * 3 / 16;
      next[x + 1] += err * 5 / 16;
      next[x + 2] += err / 16;
    }
    cur.swap(next);
    std::fill(next.begin(), next.end(), 0);
  }
}

}  // namespace mono
