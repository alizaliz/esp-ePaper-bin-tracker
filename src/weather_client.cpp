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

}  // namespace

esp_err_t fetchCurrent(double latitude, double longitude, float& temperature_c,
                       float& humidity_pct) {
  char url[192];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m",
           latitude, longitude);

  std::string body;
  ESP_RETURN_ON_ERROR(https::get(url, MAX_BYTES,
                                 [&](const char* data, size_t len) {
                                   body.append(data, len);
                                   return true;
                                 }),
                      TAG, "request failed");

  // {"current":{"temperature_2m":14.8,"relative_humidity_2m":60,...},...}
  cJSON* root = cJSON_Parse(body.c_str());
  ESP_RETURN_ON_FALSE(root != nullptr, ESP_ERR_INVALID_RESPONSE, TAG, "invalid JSON");
  const cJSON* current = cJSON_GetObjectItem(root, "current");
  const cJSON* temperature = cJSON_GetObjectItem(current, "temperature_2m");
  const cJSON* humidity = cJSON_GetObjectItem(current, "relative_humidity_2m");
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
