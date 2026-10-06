#include "network.h"

#include <cstring>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "settings.h"
#include "storage.h"

namespace network {
namespace {

constexpr const char* TAG = "network";
constexpr const char* NTP_SERVER = "nz.pool.ntp.org";

constexpr EventBits_t CONNECTED_BIT = BIT0;
EventGroupHandle_t events = nullptr;
bool started = false;

// Timings for the log: when the radio started, when it joined the network,
// and when it got an IP address.
int64_t start_us = 0;
int64_t associated_us = 0;
int last_connect_ms = 0;

// The access point last connected to. Connecting straight to it skips the
// scan across all channels. Saved in flash, so it survives restarts too.
struct AccessPoint {
  char ssid[33];
  uint8_t bssid[6];
  uint8_t channel;
};
constexpr const char* AP_KEY = "wifi_ap_v1";

// How long to try the remembered access point before scanning for others,
// e.g. if the router has changed channel.
constexpr int REMEMBERED_AP_TIMEOUT_MS = 6000;

wifi_config_t baseConfig() {
  wifi_config_t config = {};
  strncpy(reinterpret_cast<char*>(config.sta.ssid), settings::get().wifiSsid,
          sizeof(config.sta.ssid));
  strncpy(reinterpret_cast<char*>(config.sta.password), settings::get().wifiPassword,
          sizeof(config.sta.password));
  return config;
}

// Uses the remembered access point if it's for the configured network.
bool useRememberedAp(wifi_config_t& config) {
  AccessPoint ap;
  if (storage::load(AP_KEY, &ap, sizeof(ap)) != ESP_OK) return false;
  if (strncmp(ap.ssid, settings::get().wifiSsid, sizeof(ap.ssid)) != 0) return false;
  memcpy(config.sta.bssid, ap.bssid, sizeof(ap.bssid));
  config.sta.bssid_set = true;
  config.sta.channel = ap.channel;
  return true;
}

// Saves the access point just connected to, if it's changed.
void rememberAp() {
  wifi_ap_record_t info;
  if (esp_wifi_sta_get_ap_info(&info) != ESP_OK) return;
  AccessPoint ap = {};
  strncpy(ap.ssid, settings::get().wifiSsid, sizeof(ap.ssid) - 1);
  memcpy(ap.bssid, info.bssid, sizeof(ap.bssid));
  ap.channel = info.primary;
  AccessPoint saved;
  if (storage::load(AP_KEY, &saved, sizeof(saved)) == ESP_OK && memcmp(&saved, &ap, sizeof(ap)) == 0) {
    return;
  }
  storage::save(AP_KEY, &ap, sizeof(ap));
}

void onEvent(void*, esp_event_base_t base, int32_t id, void*) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
    associated_us = esp_timer_get_time();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    // Keep retrying until connect() times out.
    xEventGroupClearBits(events, CONNECTED_BIT);
    esp_wifi_connect();
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(events, CONNECTED_BIT);
  }
}

}  // namespace

esp_err_t connect(int timeout_ms) {
  if (settings::get().wifiSsid[0] == '\0') {
    ESP_LOGW(TAG, "no Wi-Fi network set; use the config page");
    return ESP_ERR_INVALID_STATE;
  }
  if (!started) {
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop failed");
    esp_netif_create_default_wifi_sta();
    events = xEventGroupCreate();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "Wi-Fi init failed");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onEvent, nullptr), TAG, "");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, onEvent, nullptr), TAG, "");

    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "");
    started = true;
  }

  wifi_config_t wifi_config = baseConfig();
  bool remembered = useRememberedAp(wifi_config);
  ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "");

  start_us = esp_timer_get_time();
  associated_us = 0;
  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed");
  EventBits_t bits = xEventGroupWaitBits(
      events, CONNECTED_BIT, pdFALSE, pdTRUE,
      pdMS_TO_TICKS(remembered ? REMEMBERED_AP_TIMEOUT_MS : timeout_ms));
  if (!(bits & CONNECTED_BIT) && remembered && timeout_ms > REMEMBERED_AP_TIMEOUT_MS) {
    // The remembered access point didn't answer: forget it and scan.
    ESP_LOGW(TAG, "remembered access point didn't answer; scanning");
    storage::erase(AP_KEY);
    remembered = false;
    // Restart the radio with the plain config; it connects (and scans) when
    // it starts. Changing the config mid-connection isn't reliable.
    esp_wifi_stop();
    wifi_config = baseConfig();
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    bits = xEventGroupWaitBits(events, CONNECTED_BIT, pdFALSE, pdTRUE,
                               pdMS_TO_TICKS(timeout_ms - REMEMBERED_AP_TIMEOUT_MS));
  }
  if (!(bits & CONNECTED_BIT)) {
    ESP_LOGW(TAG, "no connection to \"%s\" within %d ms", settings::get().wifiSsid, timeout_ms);
    disconnect();
    return ESP_ERR_TIMEOUT;
  }
  const int64_t now_us = esp_timer_get_time();
  last_connect_ms = static_cast<int>((now_us - start_us) / 1000);
  ESP_LOGI(TAG, "connected to \"%s\"%s: joined after %lld ms, IP address after %lld ms",
           settings::get().wifiSsid, remembered ? " (remembered access point)" : "",
           (associated_us - start_us) / 1000, (now_us - start_us) / 1000);
  rememberAp();
  return ESP_OK;
}

int lastConnectMs() { return last_connect_ms; }

esp_err_t syncTime(int timeout_ms) {
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
  ESP_RETURN_ON_ERROR(esp_netif_sntp_init(&config), TAG, "SNTP init failed");
  const esp_err_t err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeout_ms));
  esp_netif_sntp_deinit();
  return err;
}

void disconnect() {
  if (!started) return;
  esp_wifi_disconnect();
  esp_wifi_stop();
}

}  // namespace network
