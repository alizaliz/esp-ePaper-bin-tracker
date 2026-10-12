#pragma once

// What the sky is forecast to do, as drawn on the screen. Shared by the
// weather client and the screen layout.
enum class Sky {
  Unknown,  // no forecast
  Clear,
  PartlyCloudy,
  Cloudy,
  Fog,
  Drizzle,
  Rain,
  Snow,
  Thunder,
};

// Open-Meteo's weather_code (WMO 4677 codes) to an icon.
inline Sky skyFromWmoCode(int code) {
  switch (code) {
    case 0: case 1: return Sky::Clear;  // clear, mainly clear
    case 2: return Sky::PartlyCloudy;
    case 3: return Sky::Cloudy;
    case 45: case 48: return Sky::Fog;
    case 51: case 53: case 55: case 56: case 57: return Sky::Drizzle;
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82: return Sky::Rain;  // rain and showers
    case 71: case 73: case 75: case 77: case 85: case 86: return Sky::Snow;
    case 95: case 96: case 99: return Sky::Thunder;
    default: return Sky::Unknown;
  }
}

inline const char* skyName(Sky sky) {
  switch (sky) {
    case Sky::Clear: return "clear";
    case Sky::PartlyCloudy: return "partly cloudy";
    case Sky::Cloudy: return "cloudy";
    case Sky::Fog: return "fog";
    case Sky::Drizzle: return "drizzle";
    case Sky::Rain: return "rain";
    case Sky::Snow: return "snow";
    case Sky::Thunder: return "thunder";
    default: return "unknown";
  }
}
