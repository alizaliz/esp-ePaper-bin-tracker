#pragma once

#include "esp_err.h"

// Today's average outdoor temperature and humidity from Open-Meteo
// (open-meteo.com), a free weather API that needs no key. "Today" is the
// local day in Auckland. Needs a network connection.
namespace weather {

esp_err_t fetchDailyMean(double latitude, double longitude, float& temperature_c,
                         float& humidity_pct);

}  // namespace weather
