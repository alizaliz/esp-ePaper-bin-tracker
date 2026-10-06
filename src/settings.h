#pragma once

#include <string>

#include "cJSON.h"
#include "esp_err.h"

// Device settings. config.h supplies the defaults; anything saved from the
// config page (over USB) is kept in flash and overrides them, so settings
// can change without rebuilding the firmware.
struct Settings {
  char wifiSsid[33];
  char wifiPassword[65];
  char councilAddressId[24];
  bool useOnlineWeather;
  double latitude;
  double longitude;
  int binNightHour;
  bool binNightLed;
  int lowBatteryPercent;
  bool displayFlip;
  bool displayDithering;
  float temperatureOffsetC;
  int refreshIntervalHours;
  bool stayAwakeOnUsb;
  bool autoUpdate;
};

namespace settings {

// Loads the defaults, then any saved settings. Call once at boot, after
// storage::init().
void load();

const Settings& get();

// Checks and saves new settings. Returns an error message, or "" on success.
// They take effect after a restart.
std::string save(const Settings& updated);

// Forgets the saved settings, going back to the config.h defaults.
esp_err_t reset();

// JSON for the config page. The Wi-Fi password is never included, only
// whether one is set.
cJSON* toJson(const Settings& s);

// Applies the fields present in `json` on top of `s`. Missing fields keep
// their current value, so the password can be left unchanged. Returns an
// error message, or "" on success.
std::string applyJson(const cJSON* json, Settings& s);

}  // namespace settings
