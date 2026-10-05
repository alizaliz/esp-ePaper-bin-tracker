#pragma once

// Copy this file to src/config.h and fill in the device's real values before
// flashing. src/config.h is git-ignored so credentials are never committed.

// Wi-Fi network (2.4GHz) used to fetch the schedule and weather and set the clock.
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Auckland Council address ID. Look your address up at
// https://www.aucklandcouncil.govt.nz/en/rubbish-recycling/rubbish-recycling-collections/rubbish-recycling-collection-days.html
// The results page URL ends in /<address ID>.html.
#define COUNCIL_ADDRESS_ID "12342478585"

// 1: show the outdoor temperature and humidity from Open-Meteo, falling back
// to the onboard sensor if Wi-Fi or the request fails. 0: always use the
// onboard sensor (indoor readings).
#define USE_ONLINE_WEATHER 1

// Location for the online weather, in decimal degrees.
#define WEATHER_LATITUDE -36.85
#define WEATHER_LONGITUDE 174.76

// The device wakes just after midnight each day once its clock has been set.
// If the clock has never been set (no Wi-Fi time sync yet), it sleeps for this
// many hours instead.
#define REFRESH_INTERVAL_HOURS 24

// 0: convert the screen to black and white with a plain threshold (crisp text).
// 1: use Floyd-Steinberg dithering so greyscale images show as shading.
#define DISPLAY_DITHERING 0

// Added to the onboard sensor's temperature to correct for the board warming
// it. The device is only awake for a few seconds per wake, so this is usually
// 0. Waveshare's examples use -6 for a board that runs continuously.
#define TEMPERATURE_OFFSET_C 0.0f

// 1: stay awake while a computer is connected over USB, so the board can be
// flashed and monitored without entering download mode. Set to 0 to test real
// deep sleep while plugged in. Chargers and power banks never keep it awake.
#define STAY_AWAKE_ON_USB 1
