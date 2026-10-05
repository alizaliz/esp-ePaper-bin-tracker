#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <cmath>
#include <cstring>
#include <vector>

#include "driver/usb_serial_jtag.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "auckland_council_client.h"
#include "board_power.h"
#include "collection_types.h"
#include "config.h"
#include "display_manager.h"
#include "network.h"
#include "shtc3.h"
#include "weather_client.h"

// Auckland local time, including daylight saving.
static const char* TIMEZONE = "NZST-12NZDT,M9.5.0,M4.1.0/3";

// Wake a few minutes after midnight so "TODAY" shows for the whole pickup day.
static const int WAKE_HOUR = 0;
static const int WAKE_MINUTE = 5;

static const int WIFI_TIMEOUT_MS = 15000;
static const int NTP_TIMEOUT_MS = 10000;

// The last schedule fetched, kept in RTC memory across deep sleep so the
// screen (and the readings on it) can still be refreshed when a fetch fails.
struct Schedule {
  bool valid;
  Date pickup;
  bool rubbish;
  bool recycling;
  bool foodScraps;
};
RTC_DATA_ATTR static Schedule last_schedule = {};

struct Climate {
  bool valid;
  float temperatureC;
  float humidityPct;
};

static void log_wakeup_cause(void) {
  if (esp_sleep_get_wakeup_causes() & BIT(ESP_SLEEP_WAKEUP_TIMER)) {
    printf("Woke from deep sleep (timer)\n");
  } else {
    printf("Cold boot or reset\n");
  }
}

// The system clock keeps running through deep sleep once it has been set by
// NTP. Before that it starts at 1970.
static bool clock_is_set(void) {
  return time(nullptr) > 1735689600;  // 2025-01-01
}

static Date local_today(void) {
  const time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  return {local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
}

// Year to resolve the Council's year-less dates against: the clock's year, or
// the firmware build year if the clock hasn't been set.
static int reference_year(void) {
  if (clock_is_set()) return local_today().year;
  return atoi(__DATE__ + strlen(__DATE__) - 4);  // "Mmm dd yyyy"
}

// Seconds until the next WAKE_HOUR:WAKE_MINUTE local time.
static uint64_t seconds_until_next_wake(void) {
  const time_t now = time(nullptr);
  struct tm wake;
  localtime_r(&now, &wake);
  wake.tm_hour = WAKE_HOUR;
  wake.tm_min = WAKE_MINUTE;
  wake.tm_sec = 0;
  wake.tm_isdst = -1;
  time_t next = mktime(&wake);
  if (next <= now) {
    wake.tm_mday += 1;
    wake.tm_isdst = -1;
    next = mktime(&wake);
  }
  return (uint64_t)(next - now);
}

// Deep sleep turns off the USB port, so a sleeping board can't be flashed or
// monitored. While a computer is connected over USB, stay awake instead and
// sleep once it's unplugged. A USB charger or power bank doesn't count as
// connected, so battery behaviour is unchanged.
static void stay_awake_while_usb_connected(void) {
  if (!STAY_AWAKE_ON_USB) return;
  vTaskDelay(pdMS_TO_TICKS(200));  // give the USB host time to start polling
  if (!usb_serial_jtag_is_connected()) return;

  printf("Computer connected over USB: staying awake for flashing and logs. "
         "Unplug to sleep.\n");
  while (usb_serial_jtag_is_connected()) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static void enter_deep_sleep(void) {
  uint64_t sleep_s;
  if (clock_is_set()) {
    sleep_s = seconds_until_next_wake();
  } else {
    // Without the time of day, fall back to a fixed interval.
    sleep_s = (uint64_t)REFRESH_INTERVAL_HOURS * 60ULL * 60ULL;
  }
  printf("Deep sleeping for %llu minutes\n", sleep_s / 60);
  esp_sleep_enable_timer_wakeup(sleep_s * 1000000ULL);
  esp_deep_sleep_start();
}

// Onboard SHTC3 sensor. Read straight after waking, before Wi-Fi and the
// display warm the board.
static Climate read_sensor(void) {
  Climate climate = {};
  if (shtc3::read(climate.temperatureC, climate.humidityPct) == ESP_OK) {
    climate.valid = true;
    climate.temperatureC += TEMPERATURE_OFFSET_C;
    printf("Sensor: %.1f C, %.1f %%\n", climate.temperatureC, climate.humidityPct);
  } else {
    printf("Failed to read the temperature and humidity sensor\n");
  }
  return climate;
}

static Climate fetch_weather(void) {
  Climate climate = {};
  if (weather::fetchCurrent(WEATHER_LATITUDE, WEATHER_LONGITUDE, climate.temperatureC,
                            climate.humidityPct) == ESP_OK) {
    climate.valid = true;
    printf("Weather: %.1f C, %.1f %%\n", climate.temperatureC, climate.humidityPct);
  } else {
    printf("Failed to fetch the weather\n");
  }
  return climate;
}

// Fetches the schedule and works out the next pickup day and which bins go out.
static bool fetch_schedule(Schedule& schedule) {
  std::vector<CollectionDay> days;
  if (!auckland_council::fetchCollectionDays(COUNCIL_ADDRESS_ID, reference_year(), days)) {
    printf("Failed to fetch collection dates for address ID %s\n", COUNCIL_ADDRESS_ID);
    return false;
  }

  Date next = days.front().date;
  for (const auto& day : days) {
    if (day.date < next) next = day.date;
  }

  schedule = {};
  schedule.valid = true;
  schedule.pickup = next;
  for (const auto& day : days) {
    if (!(day.date == next)) continue;
    switch (day.type) {
      case BinType::Rubbish: schedule.rubbish = true; break;
      case BinType::Recycling: schedule.recycling = true; break;
      case BinType::FoodScraps: schedule.foodScraps = true; break;
    }
  }
  printf("Next pickup %04d-%02d-%02d: rubbish %d, recycling %d, food scraps %d\n", next.year,
         next.month, next.day, schedule.rubbish, schedule.recycling, schedule.foodScraps);
  return true;
}

static void refresh_display(bool online, const Climate& climate) {
  if (!last_schedule.valid) {
    printf("No schedule yet; leaving the display unchanged\n");
    return;
  }

  BinScreenData data;
  data.pickup = last_schedule.pickup;
  data.isToday = clock_is_set() && local_today() == last_schedule.pickup;
  data.rubbish = last_schedule.rubbish;
  data.recycling = last_schedule.recycling;
  data.foodScraps = last_schedule.foodScraps;
  data.wifiConnected = online;
  data.hasClimate = climate.valid;
  data.temperatureC = (int)lroundf(climate.temperatureC);
  data.humidityPct = (int)lroundf(climate.humidityPct);

  if (DisplayManager().show(data) != ESP_OK) {
    printf("Failed to update the display\n");
  }
}

extern "C" void app_main(void) {
  printf("ESP32-C6 ePaper tracker booting...\n");
  log_wakeup_cause();

  // Latch the battery power hold first, so the board stays on when running
  // from battery even if nothing else below succeeds.
  if (board_power::init() != ESP_OK) {
    printf("Board power init failed\n");
  }

  setenv("TZ", TIMEZONE, 1);
  tzset();

  const Climate sensor = read_sensor();
  Climate weather = {};

  // Wi-Fi is only on for as long as the fetches take.
  const bool online = network::connect(WIFI_TIMEOUT_MS) == ESP_OK;
  if (online) {
    if (network::syncTime(NTP_TIMEOUT_MS) != ESP_OK) {
      printf("Failed to sync the clock\n");
    }
    if (USE_ONLINE_WEATHER) {
      weather = fetch_weather();
    }
    Schedule schedule;
    if (fetch_schedule(schedule)) {
      last_schedule = schedule;
    } else if (last_schedule.valid) {
      printf("Using the last fetched schedule\n");
    }
    network::disconnect();
  } else {
    printf("Offline; using the last fetched schedule if there is one\n");
  }

  if (clock_is_set()) {
    const Date today = local_today();
    printf("Today is %04d-%02d-%02d\n", today.year, today.month, today.day);
  }

  refresh_display(online, weather.valid ? weather : sensor);
  stay_awake_while_usb_connected();
  enter_deep_sleep();
}
