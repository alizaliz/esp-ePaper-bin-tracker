#pragma once

#include "esp_err.h"

// Current outdoor temperature and humidity from Open-Meteo (open-meteo.com),
// a free weather API that needs no key. Needs a network connection.
namespace weather {

esp_err_t fetchCurrent(double latitude, double longitude, float& temperature_c,
                       float& humidity_pct);

}  // namespace weather
