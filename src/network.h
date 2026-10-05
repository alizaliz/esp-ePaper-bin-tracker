#pragma once

#include "esp_err.h"

// Wi-Fi station connection and clock sync. Wi-Fi is only on for the few
// seconds needed to fetch data, then switched off to save battery.
namespace network {

// Connects to WIFI_SSID and waits for an IP address, up to timeout_ms.
esp_err_t connect(int timeout_ms);

// Sets the system clock from NTP, up to timeout_ms. Needs a connection.
// The clock then keeps running through deep sleep.
esp_err_t syncTime(int timeout_ms);

// Disconnects and powers the radio down.
void disconnect();

}  // namespace network
