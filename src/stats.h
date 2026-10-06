#pragma once

#include <cstdint>

#include "cJSON.h"

#include "collection_types.h"

// Daily battery and activity log, for working out real battery life: one
// entry per day for the last 60 days, with the battery level and how long the
// board was awake. Totals build up in RTC memory and are saved to flash once
// a day, so flash writes stay rare.
namespace stats {

// Battery reading for the current wake.
void recordBattery(const Date& today, int millivolts, int percent);

// Time spent connecting to Wi-Fi during the current wake.
void recordWifi(const Date& today, int milliseconds);

// Ends the current wake's record: counts the wake and its working time (not
// time spent staying awake for a computer). Saves to flash when the day
// changes, or when `save` is set. Pass save for full refreshes (a few a day),
// so a restart loses little; the low battery blink wakes, every 10 minutes,
// don't save.
void endWake(const Date& today, uint32_t awake_ms, bool save);

// Saves today's totals so far and returns the log, oldest first:
// [{"date":"2026-10-07","mv":3950,"pct":72,"wakes":2,"awakeMs":9400,"wifiMs":1400}, ...]
cJSON* toJson();

}  // namespace stats
