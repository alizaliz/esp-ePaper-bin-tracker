#include "settings.h"

#include <cmath>
#include <cstring>

#include "esp_log.h"
#include "nvs.h"

// src/config.h is optional and git-ignored; without it, use the example's
// defaults (no Wi-Fi or address, so the board is set up from the config page).
#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif
#include "storage.h"

namespace settings {
namespace {

constexpr const char* TAG = "settings";

// Flash key. Change it when Settings' layout changes, and convert the old
// layout in load(), so saved settings aren't lost.
constexpr const char* KEY = "settings_v2";

// Layout saved by firmware before automatic updates were added.
constexpr const char* KEY_V1 = "settings_v1";
struct SettingsV1 {
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
};

Settings current;

Settings defaults() {
  Settings s = {};
  strncpy(s.wifiSsid, WIFI_SSID, sizeof(s.wifiSsid) - 1);
  strncpy(s.wifiPassword, WIFI_PASSWORD, sizeof(s.wifiPassword) - 1);
  strncpy(s.councilAddressId, COUNCIL_ADDRESS_ID, sizeof(s.councilAddressId) - 1);
  s.useOnlineWeather = USE_ONLINE_WEATHER;
  s.latitude = WEATHER_LATITUDE;
  s.longitude = WEATHER_LONGITUDE;
  s.binNightHour = BIN_NIGHT_HOUR;
  s.binNightLed = BIN_NIGHT_LED;
  s.lowBatteryPercent = LOW_BATTERY_PERCENT;
  s.displayFlip = DISPLAY_FLIP;
  s.displayDithering = DISPLAY_DITHERING;
  s.temperatureOffsetC = TEMPERATURE_OFFSET_C;
  s.refreshIntervalHours = REFRESH_INTERVAL_HOURS;
  s.stayAwakeOnUsb = STAY_AWAKE_ON_USB;
  s.autoUpdate = AUTO_UPDATE;
  return s;
}

std::string validate(const Settings& s) {
  const size_t ssid = strlen(s.wifiSsid);
  const size_t password = strlen(s.wifiPassword);
  if (ssid > 32) return "Wi-Fi name must be at most 32 characters";
  if (password != 0 && (password < 8 || password > 63)) {
    return "Wi-Fi password must be 8 to 63 characters, or empty for an open network";
  }
  for (const char* p = s.councilAddressId; *p; ++p) {
    if (*p < '0' || *p > '9') return "Council address ID must be digits only";
  }
  if (!std::isfinite(s.latitude) || s.latitude < -90 || s.latitude > 90) {
    return "Latitude must be between -90 and 90";
  }
  if (!std::isfinite(s.longitude) || s.longitude < -180 || s.longitude > 180) {
    return "Longitude must be between -180 and 180";
  }
  if (s.binNightHour < 0 || s.binNightHour > 23) return "Bin night hour must be 0 to 23";
  if (s.lowBatteryPercent < 0 || s.lowBatteryPercent > 50) {
    return "Low battery level must be 0 to 50%";
  }
  if (!std::isfinite(s.temperatureOffsetC) || std::fabs(s.temperatureOffsetC) > 20) {
    return "Temperature offset must be between -20 and 20";
  }
  if (s.refreshIntervalHours < 1 || s.refreshIntervalHours > 168) {
    return "Refresh interval must be 1 to 168 hours";
  }
  return "";
}

std::string copyString(const cJSON* item, char* dest, size_t size, const char* name) {
  if (!cJSON_IsString(item)) return std::string(name) + " must be text";
  if (strlen(item->valuestring) >= size) return std::string(name) + " is too long";
  strncpy(dest, item->valuestring, size - 1);
  dest[size - 1] = '\0';
  return "";
}

}  // namespace

void load() {
  current = defaults();
  Settings saved;
  if (storage::load(KEY, &saved, sizeof(saved)) == ESP_OK && validate(saved).empty()) {
    current = saved;
    ESP_LOGI(TAG, "using saved settings");
    return;
  }

  // Convert settings saved in the older layout, keeping the new fields at
  // their defaults, and save them in the current one.
  SettingsV1 old;
  if (storage::load(KEY_V1, &old, sizeof(old)) == ESP_OK) {
    Settings converted = defaults();
    memcpy(converted.wifiSsid, old.wifiSsid, sizeof(old.wifiSsid));
    memcpy(converted.wifiPassword, old.wifiPassword, sizeof(old.wifiPassword));
    memcpy(converted.councilAddressId, old.councilAddressId, sizeof(old.councilAddressId));
    converted.useOnlineWeather = old.useOnlineWeather;
    converted.latitude = old.latitude;
    converted.longitude = old.longitude;
    converted.binNightHour = old.binNightHour;
    converted.binNightLed = old.binNightLed;
    converted.lowBatteryPercent = old.lowBatteryPercent;
    converted.displayFlip = old.displayFlip;
    converted.displayDithering = old.displayDithering;
    converted.temperatureOffsetC = old.temperatureOffsetC;
    converted.refreshIntervalHours = old.refreshIntervalHours;
    converted.stayAwakeOnUsb = old.stayAwakeOnUsb;
    if (validate(converted).empty() && save(converted).empty()) {
      storage::erase(KEY_V1);
      ESP_LOGI(TAG, "converted saved settings to the current format");
    }
  }
}

const Settings& get() { return current; }

std::string save(const Settings& updated) {
  std::string error = validate(updated);
  if (!error.empty()) return error;
  if (storage::save(KEY, &updated, sizeof(updated)) != ESP_OK) return "Couldn't save to flash";
  current = updated;
  return "";
}

esp_err_t reset() {
  current = defaults();
  storage::erase(KEY_V1);
  return storage::erase(KEY);
}

cJSON* toJson(const Settings& s) {
  cJSON* json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "wifiSsid", s.wifiSsid);
  cJSON_AddBoolToObject(json, "wifiPasswordSet", s.wifiPassword[0] != '\0');
  cJSON_AddStringToObject(json, "councilAddressId", s.councilAddressId);
  cJSON_AddBoolToObject(json, "useOnlineWeather", s.useOnlineWeather);
  cJSON_AddNumberToObject(json, "latitude", s.latitude);
  cJSON_AddNumberToObject(json, "longitude", s.longitude);
  cJSON_AddNumberToObject(json, "binNightHour", s.binNightHour);
  cJSON_AddBoolToObject(json, "binNightLed", s.binNightLed);
  cJSON_AddNumberToObject(json, "lowBatteryPercent", s.lowBatteryPercent);
  cJSON_AddBoolToObject(json, "displayFlip", s.displayFlip);
  cJSON_AddBoolToObject(json, "displayDithering", s.displayDithering);
  cJSON_AddNumberToObject(json, "temperatureOffsetC", s.temperatureOffsetC);
  cJSON_AddNumberToObject(json, "refreshIntervalHours", s.refreshIntervalHours);
  cJSON_AddBoolToObject(json, "stayAwakeOnUsb", s.stayAwakeOnUsb);
  cJSON_AddBoolToObject(json, "autoUpdate", s.autoUpdate);
  return json;
}

std::string applyJson(const cJSON* json, Settings& s) {
  if (!cJSON_IsObject(json)) return "settings must be an object";
  std::string error;
  const cJSON* item;

  struct TextField {
    const char* name;
    char* dest;
    size_t size;
  };
  const TextField text[] = {
      {"wifiSsid", s.wifiSsid, sizeof(s.wifiSsid)},
      {"wifiPassword", s.wifiPassword, sizeof(s.wifiPassword)},
      {"councilAddressId", s.councilAddressId, sizeof(s.councilAddressId)},
  };
  for (const TextField& f : text) {
    if ((item = cJSON_GetObjectItem(json, f.name)) != nullptr) {
      if (!(error = copyString(item, f.dest, f.size, f.name)).empty()) return error;
    }
  }

  struct BoolField {
    const char* name;
    bool* dest;
  };
  const BoolField bools[] = {
      {"useOnlineWeather", &s.useOnlineWeather}, {"binNightLed", &s.binNightLed},
      {"displayFlip", &s.displayFlip},           {"displayDithering", &s.displayDithering},
      {"stayAwakeOnUsb", &s.stayAwakeOnUsb},     {"autoUpdate", &s.autoUpdate},
  };
  for (const BoolField& f : bools) {
    if ((item = cJSON_GetObjectItem(json, f.name)) != nullptr) {
      if (!cJSON_IsBool(item)) return std::string(f.name) + " must be true or false";
      *f.dest = cJSON_IsTrue(item);
    }
  }

  struct NumberField {
    const char* name;
    double* asDouble;
    float* asFloat;
    int* asInt;
  };
  const NumberField numbers[] = {
      {"latitude", &s.latitude, nullptr, nullptr},
      {"longitude", &s.longitude, nullptr, nullptr},
      {"temperatureOffsetC", nullptr, &s.temperatureOffsetC, nullptr},
      {"binNightHour", nullptr, nullptr, &s.binNightHour},
      {"lowBatteryPercent", nullptr, nullptr, &s.lowBatteryPercent},
      {"refreshIntervalHours", nullptr, nullptr, &s.refreshIntervalHours},
  };
  for (const NumberField& f : numbers) {
    if ((item = cJSON_GetObjectItem(json, f.name)) != nullptr) {
      if (!cJSON_IsNumber(item)) return std::string(f.name) + " must be a number";
      if (f.asDouble) *f.asDouble = item->valuedouble;
      if (f.asFloat) *f.asFloat = static_cast<float>(item->valuedouble);
      if (f.asInt) *f.asInt = static_cast<int>(lround(item->valuedouble));
    }
  }
  return "";
}

}  // namespace settings
