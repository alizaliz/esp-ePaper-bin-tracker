#pragma once

#include "esp_err.h"

#include "collection_types.h"
#include "weather_types.h"

// The midday forecast from Open-Meteo (open-meteo.com), a free weather API
// that needs no key, for today and tomorrow in Auckland time. Needs a
// network connection.
namespace weather {

struct Midday {
  bool valid = false;
  Date date;
  float temperatureC = 0;
  Sky sky = Sky::Unknown;
};

// Fills days[0] with today's 12:00 forecast and days[1] with tomorrow's.
esp_err_t fetchMidday(double latitude, double longitude, Midday (&days)[2]);

}  // namespace weather
