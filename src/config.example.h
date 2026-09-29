#pragma once

// Copy this file to src/config.h and fill in the device's real values before
// flashing. src/config.h is git-ignored so credentials are never committed.
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Official Auckland Council collection source:
// https://www.aucklandcouncil.govt.nz/en/rubbish-recycling/rubbish-recycling-collections/rubbish-recycling-collection-days.html
//
// The app should resolve the house/address once during boot, then store the
// resulting address ID in NVS so it doesn't need to re-query the API every time.
#define ADDRESS_SEARCH_QUERY "500 Queen Street"
#define DEFAULT_ADDRESS_ID "12342478585"

// Address details needed for the display and the local device configuration.
#define ADDRESS_LINE_1 "500 Queen Street"
#define ADDRESS_LINE_2 "Auckland Central"

// How long the device deep sleeps between schedule refreshes. Daily keeps the
// "next collection" dates current without costing much battery.
#define REFRESH_INTERVAL_HOURS 24
