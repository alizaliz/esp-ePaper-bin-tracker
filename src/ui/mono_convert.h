#pragma once

#include <cstdint>

// Converts an 8-bit greyscale image (1 byte per pixel, rows packed) to the
// panel's 1-bit frame format: rows of width/8 bytes, MSB is the leftmost
// pixel, 1 = white.
namespace mono {

// Plain threshold: keeps text and line icons crisp.
void threshold(const uint8_t* grey, uint8_t* frame, int width, int height);

// Floyd-Steinberg error diffusion: shows greys as shading.
void dither(const uint8_t* grey, uint8_t* frame, int width, int height);

}  // namespace mono
