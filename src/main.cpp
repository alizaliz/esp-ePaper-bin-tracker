#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <cmath>
#include <cstring>
#include <vector>

#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "auckland_council_client.h"
#include "battery.h"
#include "board_pins.h"
#include "board_power.h"
#include "config_service.h"
#include "collection_types.h"
#include "settings.h"
#include "display_manager.h"
#include "network.h"
#include "shtc3.h"
#include "storage.h"
#include "weather_client.h"

// Auckland local time, including daylight saving.
static const char* TIMEZONE = "NZST-12NZDT,M9.5.0,M4.1.0/3";

// Wake a few minutes after midnight so "TODAY" shows for the whole pickup day.
static const int WAKE_HOUR = 0;
static const int WAKE_MINUTE = 5;

// After a failed refresh (no Wi-Fi or no schedule), retry this soon, up to
// MAX_RETRIES times, before falling back to the normal schedule.
static const int RETRY_INTERVAL_S = 60 * 60;
static const int MAX_RETRIES = 3;

// A wake that runs longer than this (e.g. a hung request) is cut short and
// the board forced into deep sleep, so it can't drain the battery.
static const int MAX_AWAKE_S = 90;

static const int LED_BLINK_DURATION_S = 10;

// A schedule older than this is shown as possibly out of date.
static const int STALE_AFTER_S = 48 * 60 * 60;

// While the battery is low, wake this often to blink the LED.
static const int LOW_BATTERY_BLINK_INTERVAL_S = 10 * 60;

static const int WIFI_TIMEOUT_MS = 15000;
static const int NTP_TIMEOUT_MS = 10000;

// The last schedule fetched. Kept in RTC memory across deep sleep, and saved
// to flash so it also survives power loss and reflashing. Lets the screen (and
// the readings on it) still be refreshed when a fetch fails.
struct Schedule {
  bool valid;
  Date pickup;
  bool rubbish;
  bool recycling;
  bool foodScraps;
  time_t fetchedAt;  // when it was fetched, as a time() value
};
RTC_DATA_ATTR static Schedule last_schedule = {};

// Flash key for the schedule. Change it if Schedule's layout changes, so an
// old saved copy is ignored rather than misread.
static const char* SCHEDULE_KEY = "schedule_v1";

// When the next full refresh (Wi-Fi, fetch, redraw) is due, as a time()
// value. Low battery wakes in between only blink the LED.
RTC_DATA_ATTR static time_t next_refresh = 0;
RTC_DATA_ATTR static bool battery_low = false;
RTC_DATA_ATTR static int failed_refreshes = 0;
// Set after a successful fetch. RTC memory is cleared by power loss, so this
// is false until the first fetch after power returns.
RTC_DATA_ATTR static bool fetched_since_power_on = false;

static esp_timer_handle_t awake_cap_timer = nullptr;

struct Climate {
  bool valid;
  float temperatureC;
  float humidityPct;
};

static void log_wakeup_cause(void) {
  const uint32_t causes = esp_sleep_get_wakeup_causes();
  if (causes & BIT(ESP_SLEEP_WAKEUP_EXT1)) {
    printf("Woke by the PWR button\n");
  } else if (causes & BIT(ESP_SLEEP_WAKEUP_TIMER)) {
    printf("Woke from deep sleep (timer)\n");
  } else {
    printf("Cold boot or reset\n");
  }
}

static bool pwr_button_pressed(void) {
  return gpio_get_level(PWR_BUTTON_PIN) == 0;
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

// The next WAKE_HOUR:WAKE_MINUTE local time after now.
static time_t next_wake_time(void) {
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
  return next;
}

// Local time on a date. mktime normalises out-of-range days, so
// {y, m, d - 1} gives the day before.
static time_t local_time_on(const Date& date, int hour, int minute) {
  struct tm t = {};
  t.tm_year = date.year - 1900;
  t.tm_mon = date.month - 1;
  t.tm_mday = date.day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_isdst = -1;
  return mktime(&t);
}

// BIN_NIGHT_HOUR on the evening before the next pickup.
static time_t bin_night_start(void) {
  const Date& p = last_schedule.pickup;
  return local_time_on({p.year, p.month, p.day - 1}, settings::get().binNightHour, 0);
}

// From BIN_NIGHT_HOUR the evening before a pickup until midnight.
static bool is_bin_night(void) {
  if (!clock_is_set() || !last_schedule.valid) return false;
  const time_t now = time(nullptr);
  return now >= bin_night_start() && now < local_time_on(last_schedule.pickup, 0, 0);
}

// When the refresh after this one should happen: just after midnight, or the
// bin night reminder if that comes first. After a failed refresh, retry
// sooner, a limited number of times.
static time_t schedule_next_refresh(bool refresh_ok) {
  const time_t now = time(nullptr);
  time_t next;
  if (clock_is_set()) {
    next = next_wake_time();
    if (last_schedule.valid) {
      const time_t bin_night = bin_night_start();
      if (bin_night > now && bin_night < next) next = bin_night;
    }
  } else {
    // Without the time of day, fall back to a fixed interval. time() still
    // counts up through deep sleep even when it hasn't been set.
    next = now + (time_t)settings::get().refreshIntervalHours * 60 * 60;
  }

  if (!refresh_ok && failed_refreshes <= MAX_RETRIES) {
    const time_t retry = now + RETRY_INTERVAL_S;
    if (retry < next) {
      printf("Refresh failed (%d in a row); retrying in %d minutes\n", failed_refreshes,
             RETRY_INTERVAL_S / 60);
      next = retry;
    }
  }
  return next;
}

// Runs if a wake goes on too long. Sleeps as if the refresh had failed, so a
// retry is due when the board next wakes.
static void awake_cap_expired(void*) {
  printf("Awake for more than %d s: forcing deep sleep\n", MAX_AWAKE_S);
  config_service::stop();
  failed_refreshes++;
  next_refresh = time(nullptr) + RETRY_INTERVAL_S;
  esp_sleep_enable_ext1_wakeup_io(1ULL << PWR_BUTTON_PIN, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_sleep_enable_timer_wakeup((uint64_t)RETRY_INTERVAL_S * 1000000ULL);
  esp_deep_sleep_start();
}

static void start_awake_cap(void) {
  esp_timer_create_args_t args = {};
  args.callback = awake_cap_expired;
  args.name = "awake_cap";
  if (esp_timer_create(&args, &awake_cap_timer) == ESP_OK) {
    esp_timer_start_once(awake_cap_timer, (uint64_t)MAX_AWAKE_S * 1000000ULL);
  }
}

// Deep sleep turns off the USB port, so a sleeping board can't be flashed or
// monitored. While a computer is connected over USB, stay awake instead and
// sleep once it's unplugged. A USB charger or power bank doesn't count as
// connected, so battery behaviour is unchanged.
static void stay_awake_while_usb_connected(void) {
  if (!settings::get().stayAwakeOnUsb) return;
  vTaskDelay(pdMS_TO_TICKS(200));  // give the USB host time to start polling
  if (!usb_serial_jtag_is_connected()) return;

  // Staying awake is intended here, so the awake time cap no longer applies.
  if (awake_cap_timer != nullptr) esp_timer_stop(awake_cap_timer);

  printf("Computer connected over USB: staying awake for flashing, logs and the config "
         "page. Unplug to sleep, or press PWR to restart.\n");
  // Only act on a press that starts while awake, not one still held from
  // waking the board.
  bool armed = !pwr_button_pressed();
  while (usb_serial_jtag_is_connected()) {
    if (!pwr_button_pressed()) {
      armed = true;
    } else if (armed) {
      printf("PWR pressed: restarting\n");
      esp_restart();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

static void enter_deep_sleep(void) {
  const time_t now = time(nullptr);
  int64_t sleep_s = next_refresh > now ? next_refresh - now : 60;
  if (battery_low && sleep_s > LOW_BATTERY_BLINK_INTERVAL_S) {
    sleep_s = LOW_BATTERY_BLINK_INTERVAL_S;
  }
  // A press of PWR wakes the board, which restarts the firmware and runs a
  // full refresh. Wait for release first so a held button doesn't wake it
  // straight back up.
  while (pwr_button_pressed()) {
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  esp_sleep_enable_ext1_wakeup_io(1ULL << PWR_BUTTON_PIN, ESP_EXT1_WAKEUP_ANY_LOW);

  printf("Deep sleeping for %lld minutes\n", sleep_s / 60);
  config_service::stop();
  esp_sleep_enable_timer_wakeup((uint64_t)sleep_s * 1000000ULL);
  esp_deep_sleep_start();
}

// Reads the battery and updates battery_low, with 5% hysteresis so it doesn't
// flip back and forth around the threshold. Returns the charge, or -1.
static int check_battery(void) {
  float volts = 0;
  if (battery::readVoltage(volts) != ESP_OK) {
    printf("Failed to read the battery voltage\n");
    return -1;
  }
  const int percent = battery::percentFromVoltage(volts);
  if (percent < settings::get().lowBatteryPercent) {
    battery_low = true;
  } else if (percent > settings::get().lowBatteryPercent + 5) {
    battery_low = false;
  }
  printf("Battery: %.2f V, %d %%%s\n", volts, percent, battery_low ? " (low)" : "");
  return percent;
}

// One second of LED blinking: on/off steps with their lengths. The board only
// has one LED the firmware can drive (the red one belongs to the charger), so
// the two reminders are told apart by pattern.
struct BlinkStep {
  bool on;
  int ms;
};
// Bin night: a slow, even blink.
static const BlinkStep BIN_NIGHT_BLINK[] = {{true, 500}, {false, 500}};
// Low battery: two quick flashes, then a pause.
static const BlinkStep LOW_BATTERY_BLINK[] = {
    {true, 150}, {false, 150}, {true, 150}, {false, 550}};

// Blinks the green LED with a one-second pattern, repeated for
// LED_BLINK_DURATION_S. Light sleep between steps keeps the cost of these
// wakes down; the expander holds the LED state meanwhile. Light sleep drops
// USB, so plain delays are used while a computer is connected.
template <size_t N>
static void blink_led(const char* reason, const BlinkStep (&pattern)[N]) {
  printf("%s: blinking the LED\n", reason);
  const bool usb = usb_serial_jtag_is_connected();
  for (int second = 0; second < LED_BLINK_DURATION_S; ++second) {
    for (const BlinkStep& step : pattern) {
      board_power::setLed(step.on);
      if (usb) {
        vTaskDelay(pdMS_TO_TICKS(step.ms));
      } else {
        esp_sleep_enable_timer_wakeup((uint64_t)step.ms * 1000);
        esp_light_sleep_start();
      }
    }
  }
  board_power::setLed(false);
}

// Onboard SHTC3 sensor. Read straight after waking, before Wi-Fi and the
// display warm the board.
static Climate read_sensor(void) {
  Climate climate = {};
  if (shtc3::read(climate.temperatureC, climate.humidityPct) == ESP_OK) {
    climate.valid = true;
    climate.temperatureC += settings::get().temperatureOffsetC;
    printf("Sensor: %.1f C, %.1f %%\n", climate.temperatureC, climate.humidityPct);
  } else {
    printf("Failed to read the temperature and humidity sensor\n");
  }
  return climate;
}

static Climate fetch_weather(void) {
  Climate climate = {};
  if (weather::fetchDailyMean(settings::get().latitude, settings::get().longitude, climate.temperatureC,
                              climate.humidityPct) == ESP_OK) {
    climate.valid = true;
    printf("Weather (today's mean): %.1f C, %.1f %%\n", climate.temperatureC,
           climate.humidityPct);
  } else {
    printf("Failed to fetch the weather\n");
  }
  return climate;
}

// Fetches the schedule and works out the next pickup day and which bins go out.
static bool fetch_schedule(Schedule& schedule) {
  std::vector<CollectionDay> days;
  if (!auckland_council::fetchCollectionDays(settings::get().councilAddressId, reference_year(), days)) {
    printf("Failed to fetch collection dates for address ID %s\n", settings::get().councilAddressId);
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

// Whether the schedule shown may be out of date: the pickup day has passed,
// or the last successful fetch was more than STALE_AFTER_S ago. Without a set
// clock neither can be checked, so it counts as stale unless it was fetched
// since power on.
static bool schedule_is_stale(void) {
  if (!last_schedule.valid) return false;
  if (!clock_is_set()) return !fetched_since_power_on;
  if (last_schedule.pickup < local_today()) return true;
  // A fetch made before the clock was set has no usable timestamp; rely on
  // the pickup date check above.
  const time_t fetched = last_schedule.fetchedAt;
  return fetched > 1735689600 && time(nullptr) - fetched > STALE_AFTER_S;
}

// Redraws the screen. Returns true if it shows the bin night reminder.
static bool refresh_display(bool online, const Climate& climate, int battery_pct) {
  if (!last_schedule.valid) {
    printf("No schedule yet; leaving the display unchanged\n");
    return false;
  }

  BinScreenData data;
  data.pickup = last_schedule.pickup;
  // TODAY and TONIGHT can't be trusted from out-of-date data.
  data.isStale = schedule_is_stale();
  data.isToday = !data.isStale && clock_is_set() && local_today() == last_schedule.pickup;
  data.isTonight = !data.isStale && !data.isToday && is_bin_night();
  if (data.isStale) printf("Schedule may be out of date\n");
  data.rubbish = last_schedule.rubbish;
  data.recycling = last_schedule.recycling;
  data.foodScraps = last_schedule.foodScraps;
  data.wifiConnected = online;
  data.hasBattery = battery_pct >= 0;
  data.batteryPct = battery_pct;
  data.hasClimate = climate.valid;
  data.temperatureC = (int)lroundf(climate.temperatureC);
  data.humidityPct = (int)lroundf(climate.humidityPct);

  if (DisplayManager().show(data) != ESP_OK) {
    printf("Failed to update the display\n");
  }
  return data.isTonight;
}

// Reads the sensor, goes online for the clock, weather and schedule, and
// redraws the screen. Returns true if it shows the bin night reminder.
static bool full_refresh(int battery_pct) {
  const Climate sensor = read_sensor();
  Climate weather = {};

  // Wi-Fi is only on for as long as the fetches take.
  const bool online = network::connect(WIFI_TIMEOUT_MS) == ESP_OK;
  bool fetched = false;
  if (online) {
    if (network::syncTime(NTP_TIMEOUT_MS) != ESP_OK) {
      printf("Failed to sync the clock\n");
    }
    if (settings::get().useOnlineWeather) {
      weather = fetch_weather();
    }
    Schedule schedule;
    fetched = fetch_schedule(schedule);
    if (fetched) {
      schedule.fetchedAt = time(nullptr);
      last_schedule = schedule;
      fetched_since_power_on = true;
      if (storage::save(SCHEDULE_KEY, &last_schedule, sizeof(last_schedule)) != ESP_OK) {
        printf("Failed to save the schedule to flash\n");
      }
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

  const bool tonight = refresh_display(online, weather.valid ? weather : sensor, battery_pct);

  failed_refreshes = fetched ? 0 : failed_refreshes + 1;
  next_refresh = schedule_next_refresh(fetched);
  struct tm next;
  localtime_r(&next_refresh, &next);
  printf("Next refresh at %04d-%02d-%02d %02d:%02d\n", next.tm_year + 1900, next.tm_mon + 1,
         next.tm_mday, next.tm_hour, next.tm_min);
  return tonight;
}

extern "C" void app_main(void) {
  // The version comes from `git describe` at build time: the release tag
  // (e.g. v1.0.0) for release builds, otherwise a commit hash.
  printf("ESP32-C6 ePaper tracker %s booting...\n", esp_app_get_description()->version);
  log_wakeup_cause();
  start_awake_cap();

  // Latch the battery power hold first, so the board stays on when running
  // from battery even if nothing else below succeeds.
  if (board_power::init() != ESP_OK) {
    printf("Board power init failed\n");
  }

  setenv("TZ", TIMEZONE, 1);
  tzset();

  if (storage::init() != ESP_OK) {
    printf("Flash storage init failed\n");
  }
  settings::load();
  config_service::start();
  // RTC memory is cleared by power loss and reflashing; fall back to the copy
  // in flash.
  if (!last_schedule.valid) {
    Schedule saved;
    if (storage::load(SCHEDULE_KEY, &saved, sizeof(saved)) == ESP_OK && saved.valid) {
      last_schedule = saved;
      printf("Loaded the last schedule from flash (pickup %04d-%02d-%02d)\n", saved.pickup.year,
             saved.pickup.month, saved.pickup.day);
    }
  }

  gpio_config_t button = {};
  button.pin_bit_mask = 1ULL << PWR_BUTTON_PIN;
  button.mode = GPIO_MODE_INPUT;  // the board has its own pull-up
  gpio_config(&button);

  const int battery_pct = check_battery();

  // A timer wake before the refresh is due only blinks the low battery LED.
  // Any other wake (PWR button, power on, reset, flashing) always refreshes.
  const bool timer_wake = esp_sleep_get_wakeup_causes() & BIT(ESP_SLEEP_WAKEUP_TIMER);
  bool tonight = false;
  if (!timer_wake || time(nullptr) >= next_refresh) {
    tonight = full_refresh(battery_pct);
  }

  if (battery_low) {
    blink_led("Battery low", LOW_BATTERY_BLINK);
  } else if (tonight && settings::get().binNightLed) {
    blink_led("Bin night", BIN_NIGHT_BLINK);
  }

  stay_awake_while_usb_connected();
  enter_deep_sleep();
}
