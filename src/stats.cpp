#include "stats.h"

#include <cstdio>
#include <cstring>
#include <mutex>

#include "esp_attr.h"

#include "storage.h"

namespace stats {
namespace {

constexpr int MAX_DAYS = 60;
// Flash key. Change it if DayStats' layout changes.
constexpr const char* KEY = "stats_v1";

struct DayStats {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint16_t millivolts;  // latest battery reading that day
  uint8_t percent;
  uint8_t pad;
  uint32_t wakes;
  uint32_t awakeMs;
  uint32_t wifiMs;
};

struct Log {
  uint32_t count;
  DayStats days[MAX_DAYS];  // oldest first
};

// Totals for one day not yet saved to flash. Survives deep sleep.
RTC_DATA_ATTR DayStats pending = {};

// The config page reads the log from its own task.
std::mutex lock;

bool sameDay(const DayStats& d, const Date& date) {
  return d.year == date.year && d.month == date.month && d.day == date.day;
}

bool hasData(const DayStats& d) { return d.year != 0 && (d.wakes || d.millivolts); }

// Adds the pending totals into the saved log and clears them.
void flush() {
  if (!hasData(pending)) return;
  static Log log;  // ~1.2KB; static to keep it off the task stack
  if (storage::load(KEY, &log, sizeof(log)) != ESP_OK || log.count > MAX_DAYS) {
    memset(&log, 0, sizeof(log));
  }
  DayStats* entry = nullptr;
  if (log.count > 0) {
    DayStats& last = log.days[log.count - 1];
    if (last.year == pending.year && last.month == pending.month && last.day == pending.day) {
      entry = &last;
    }
  }
  if (entry == nullptr) {
    if (log.count == MAX_DAYS) {
      memmove(&log.days[0], &log.days[1], sizeof(DayStats) * (MAX_DAYS - 1));
      log.count--;
    }
    entry = &log.days[log.count++];
    *entry = {};
    entry->year = pending.year;
    entry->month = pending.month;
    entry->day = pending.day;
  }
  if (pending.millivolts) {
    entry->millivolts = pending.millivolts;
    entry->percent = pending.percent;
  }
  entry->wakes += pending.wakes;
  entry->awakeMs += pending.awakeMs;
  entry->wifiMs += pending.wifiMs;
  storage::save(KEY, &log, sizeof(log));
  // Keep the date; the totals are now in flash.
  DayStats kept = {};
  kept.year = pending.year;
  kept.month = pending.month;
  kept.day = pending.day;
  pending = kept;
}

// Points `pending` at today, saving the previous day's totals first.
void startDay(const Date& today) {
  if (today.year < 2025) return;  // clock not set yet
  if (!sameDay(pending, today)) {
    flush();
    pending = {};
    pending.year = today.year;
    pending.month = today.month;
    pending.day = today.day;
  }
}

}  // namespace

void recordBattery(const Date& today, int millivolts, int percent) {
  std::lock_guard<std::mutex> guard(lock);
  startDay(today);
  if (!sameDay(pending, today)) return;
  pending.millivolts = millivolts;
  pending.percent = percent;
}

void recordWifi(const Date& today, int milliseconds) {
  std::lock_guard<std::mutex> guard(lock);
  startDay(today);
  if (sameDay(pending, today)) pending.wifiMs += milliseconds;
}

void endWake(const Date& today, uint32_t awake_ms, bool save) {
  std::lock_guard<std::mutex> guard(lock);
  startDay(today);
  if (!sameDay(pending, today)) return;
  pending.wakes++;
  pending.awakeMs += awake_ms;
  if (save) flush();
}

cJSON* toJson() {
  std::lock_guard<std::mutex> guard(lock);
  flush();
  static Log log;
  cJSON* days = cJSON_CreateArray();
  if (storage::load(KEY, &log, sizeof(log)) != ESP_OK || log.count > MAX_DAYS) return days;
  for (uint32_t i = 0; i < log.count; ++i) {
    const DayStats& d = log.days[i];
    char date[16];
    snprintf(date, sizeof(date), "%04u-%02u-%02u", unsigned(d.year), unsigned(d.month),
             unsigned(d.day));
    cJSON* day = cJSON_CreateObject();
    cJSON_AddStringToObject(day, "date", date);
    cJSON_AddNumberToObject(day, "mv", d.millivolts);
    cJSON_AddNumberToObject(day, "pct", d.percent);
    cJSON_AddNumberToObject(day, "wakes", d.wakes);
    cJSON_AddNumberToObject(day, "awakeMs", d.awakeMs);
    cJSON_AddNumberToObject(day, "wifiMs", d.wifiMs);
    cJSON_AddItemToArray(days, day);
  }
  return days;
}

}  // namespace stats
