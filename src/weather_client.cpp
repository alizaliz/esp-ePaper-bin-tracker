#include "weather_client.h"

#include <cstdio>
#include <string>

#include "cJSON.h"
#include "esp_check.h"

#include "https_client.h"

namespace weather {
namespace {

constexpr const char* TAG = "weather";
constexpr size_t MAX_BYTES = 4096;  // the response is about 400 bytes
// A fetch normally takes about 1.3 seconds. Occasionally the connection to
// Germany stalls; the onboard sensor is the fallback, so don't wait long.
constexpr int TIMEOUT_MS = 5000;

}  // namespace

esp_err_t fetchDailyMean(double latitude, double longitude, float& temperature_c,
                         float& humidity_pct) {
  char url[256];
  snprintf(url, sizeof(url),
           // Plain HTTP: Open-Meteo's servers are in Germany, and a TLS
           // handshake that far takes the board 3-10 seconds, against about 1
           // without. The data isn't sensitive. Everything else uses HTTPS.
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&daily=temperature_2m_mean,relative_humidity_2m_mean"
           "&timezone=Pacific%%2FAuckland&forecast_days=1",
           latitude, longitude);

  std::string body;
  ESP_RETURN_ON_ERROR(https::get(url, MAX_BYTES,
                                 [&](const char* data, size_t len) {
                                   body.append(data, len);
                                   return true;
                                 },
                                 TIMEOUT_MS),
                      TAG, "request failed");

  // {"daily":{"time":["2026-10-05"],"temperature_2m_mean":[12.2],
  //  "relative_humidity_2m_mean":[74]},...}
  cJSON* root = cJSON_Parse(body.c_str());
  ESP_RETURN_ON_FALSE(root != nullptr, ESP_ERR_INVALID_RESPONSE, TAG, "invalid JSON");
  const cJSON* daily = cJSON_GetObjectItem(root, "daily");
  const cJSON* temperature =
      cJSON_GetArrayItem(cJSON_GetObjectItem(daily, "temperature_2m_mean"), 0);
  const cJSON* humidity =
      cJSON_GetArrayItem(cJSON_GetObjectItem(daily, "relative_humidity_2m_mean"), 0);
  const bool ok = cJSON_IsNumber(temperature) && cJSON_IsNumber(humidity);
  if (ok) {
    temperature_c = static_cast<float>(temperature->valuedouble);
    humidity_pct = static_cast<float>(humidity->valuedouble);
  }
  cJSON_Delete(root);
  ESP_RETURN_ON_FALSE(ok, ESP_ERR_INVALID_RESPONSE, TAG, "missing fields");
  return ESP_OK;
}

}  // namespace weather
