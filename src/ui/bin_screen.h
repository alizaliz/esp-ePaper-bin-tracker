#pragma once

#include "lvgl.h"

#include "../collection_types.h"

// Everything shown on the main screen.
struct BinScreenData {
  Date pickup;        // next collection day
  bool isToday = false;

  // Which bins go out on the pickup day
  bool rubbish = false;
  bool recycling = false;
  bool foodScraps = false;

  bool wifiConnected = false;  // whether this refresh got online

  bool hasBattery = false;  // false if the battery voltage couldn't be read
  int batteryPct = 0;

  bool hasClimate = false;  // false if no temperature/humidity source worked
  int temperatureC = 0;
  int humidityPct = 0;
};

// Builds the main screen for a 200x200 display. Plain LVGL with no ESP-IDF
// dependencies, so tools/preview can render it on a computer.
lv_obj_t* createBinScreen(const BinScreenData& data);
