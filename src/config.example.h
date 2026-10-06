#pragma once

// Default settings, built into the firmware. Settings saved from the config
// page (docs/config/index.html, over USB) override these and are kept in
// flash, so you normally don't need to edit this file.
//
// Optional: to build your own values in as defaults, copy this file to
// src/config.h and edit it. src/config.h is git-ignored, so credentials are
// never committed. Without it, the build uses this file.

// Wi-Fi network (2.4GHz) used to fetch the schedule and weather and set the clock.
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Auckland Council address ID. Look your address up at
// https://www.aucklandcouncil.govt.nz/en/rubbish-recycling/rubbish-recycling-collections/rubbish-recycling-collection-days.html
// The results page URL ends in /<address ID>.html.
#define COUNCIL_ADDRESS_ID ""

// 1: show today's average outdoor temperature and humidity from Open-Meteo,
// falling back to the onboard sensor if Wi-Fi or the request fails. 0: always use the
// onboard sensor (indoor readings).
#define USE_ONLINE_WEATHER 1

// Location for the online weather, in decimal degrees.
#define WEATHER_LATITUDE -36.85
#define WEATHER_LONGITUDE 174.76

// The evening before a pickup, the device wakes at this hour (24h, local
// time) and shows TONIGHT as a reminder to put the bins out.
#define BIN_NIGHT_HOUR 18

// 1: also blink the green LED for 10 seconds at the bin night wake.
#define BIN_NIGHT_LED 1

// The device wakes just after midnight each day once its clock has been set.
// If the clock has never been set (no Wi-Fi time sync yet), it sleeps for this
// many hours instead.
#define REFRESH_INTERVAL_HOURS 24

// 0: convert the screen to black and white with a plain threshold (crisp text).
// 1: use Floyd-Steinberg dithering so greyscale images show as shading.
#define DISPLAY_DITHERING 0

// 1: rotate the image 180 degrees so the USB-C and LED end of the board is at
// the top. 0: the panel's native orientation, with that end at the bottom.
#define DISPLAY_FLIP 1

// Added to the onboard sensor's temperature to correct for the board warming
// it. The device is only awake for a few seconds per wake, so this is usually
// 0. Waveshare's examples use -6 for a board that runs continuously.
#define TEMPERATURE_OFFSET_C 0.0f

// 1: stay awake while a computer is connected over USB, so the board can be
// flashed and monitored without entering download mode. Set to 0 to test real
// deep sleep while plugged in. Chargers and power banks never keep it awake.
#define STAY_AWAKE_ON_USB 1

// Below this charge (%), the green LED blinks for 10 seconds every 10 minutes
// as a reminder to charge. It stops once the charge is back above
// LOW_BATTERY_PERCENT + 5.
#define LOW_BATTERY_PERCENT 10
