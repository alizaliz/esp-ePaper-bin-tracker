#pragma once

#include "lvgl.h"

#include "../collection_types.h"

// Everything shown on the main screen.
struct BinScreenData {
  Date pickup;        // next collection day
  bool isToday = false;
  bool isTonight = false;  // evening before the pickup: put the bins out
  bool isStale = false;    // the schedule may be out of date (old or past)

  // Which bins go out on the pickup day
  bool rubbish = false;
  bool recycling = false;
  bool foodScraps = false;

  bool wifiConnected = false;  // whether this refresh got online
  bool batteryLow = false;

  bool hasClimate = false;  // false if no temperature/humidity source worked
  int temperatureC = 0;
  int humidityPct = 0;

  // Changes once a day (e.g. days since 1970), to vary the mascot's face on
  // ordinary days. 0 if the date isn't known.
  int dayNumber = 0;
};

// Builds the main screen for a 200x200 display. Plain LVGL with no ESP-IDF
// dependencies, so tools/preview can render it on a computer.
lv_obj_t* createBinScreen(const BinScreenData& data);
