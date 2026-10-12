#include "weather_client.h"

#include <cstdio>
#include <string>

#include "cJSON.h"
#include "esp_check.h"

#include "https_client.h"

namespace weather {
namespace {

constexpr const char* TAG = "weather";
constexpr size_t MAX_BYTES = 8192;  // the response is about 2KB
// A fetch normally takes about 1.3 seconds. Occasionally the connection to
// Germany stalls; the onboard sensor is the fallback, so don't wait long.
constexpr int TIMEOUT_MS = 5000;
constexpr int MIDDAY = 12;

}  // namespace

esp_err_t fetchMidday(double latitude, double longitude, Midday (&days)[2]) {
  char url[256];
  snprintf(url, sizeof(url),
           // Plain HTTP: Open-Meteo's servers are in Germany, and a TLS
           // handshake that far takes the board 3-10 seconds, against about 1
           // without. The data isn't sensitive. Everything else uses HTTPS.
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&hourly=temperature_2m,weather_code"
           "&timezone=Pacific%%2FAuckland&forecast_days=2",
           latitude, longitude);

  std::string body;
  ESP_RETURN_ON_ERROR(https::get(url, MAX_BYTES,
                                 [&](const char* data, size_t len) {
                                   body.append(data, len);
                                   return true;
                                 },
                                 TIMEOUT_MS),
                      TAG, "request failed");

  // {"hourly":{"time":["2026-10-12T00:00",...],"temperature_2m":[12.2,...],
  //  "weather_code":[3,...]},...}, one entry per local hour from midnight.
  cJSON* root = cJSON_Parse(body.c_str());
  ESP_RETURN_ON_FALSE(root != nullptr, ESP_ERR_INVALID_RESPONSE, TAG, "invalid JSON");
  const cJSON* hourly = cJSON_GetObjectItem(root, "hourly");
  const cJSON* times = cJSON_GetObjectItem(hourly, "time");
  const cJSON* temperatures = cJSON_GetObjectItem(hourly, "temperature_2m");
  const cJSON* codes = cJSON_GetObjectItem(hourly, "weather_code");
  for (int d = 0; d < 2; ++d) {
    days[d] = {};
    const int i = d * 24 + MIDDAY;
    const cJSON* time = cJSON_GetArrayItem(times, i);
    const cJSON* temperature = cJSON_GetArrayItem(temperatures, i);
    const cJSON* code = cJSON_GetArrayItem(codes, i);
    Midday m;
    int hour = -1;
    if (!cJSON_IsString(time) || !cJSON_IsNumber(temperature) || !cJSON_IsNumber(code) ||
        sscanf(time->valuestring, "%d-%d-%dT%d", &m.date.year, &m.date.month, &m.date.day,
               &hour) != 4 ||
        hour != MIDDAY) {
      continue;
    }
    m.valid = true;
    m.temperatureC = static_cast<float>(temperature->valuedouble);
    m.sky = skyFromWmoCode(code->valueint);
    days[d] = m;
  }
  cJSON_Delete(root);
  ESP_RETURN_ON_FALSE(days[0].valid, ESP_ERR_INVALID_RESPONSE, TAG, "missing fields");
  return ESP_OK;
}

}  // namespace weather
