#include "network.h"

#include <cstring>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

#include "config.h"

namespace network {
namespace {

constexpr const char* TAG = "network";
constexpr const char* NTP_SERVER = "nz.pool.ntp.org";

constexpr EventBits_t CONNECTED_BIT = BIT0;
EventGroupHandle_t events = nullptr;
bool started = false;

void onEvent(void*, esp_event_base_t base, int32_t id, void*) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    // Keep retrying until connect() times out.
    xEventGroupClearBits(events, CONNECTED_BIT);
    esp_wifi_connect();
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(events, CONNECTED_BIT);
  }
}

esp_err_t initNvs() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS erase failed");
    err = nvs_flash_init();
  }
  return err;
}

}  // namespace

esp_err_t connect(int timeout_ms) {
  if (!started) {
    ESP_RETURN_ON_ERROR(initNvs(), TAG, "NVS init failed");  // Wi-Fi stores calibration in NVS
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

    wifi_config_t wifi_config = {};
    strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), WIFI_SSID,
            sizeof(wifi_config.sta.ssid));
    strncpy(reinterpret_cast<char*>(wifi_config.sta.password), WIFI_PASSWORD,
            sizeof(wifi_config.sta.password));
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "");
    started = true;
  }

  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed");
  const EventBits_t bits =
      xEventGroupWaitBits(events, CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms));
  if (!(bits & CONNECTED_BIT)) {
    ESP_LOGW(TAG, "no connection to \"%s\" within %d ms", WIFI_SSID, timeout_ms);
    disconnect();
    return ESP_ERR_TIMEOUT;
  }
  ESP_LOGI(TAG, "connected to \"%s\"", WIFI_SSID);
  return ESP_OK;
}

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
