#include "config_service.h"

#include <cstdio>
#include <string>

#include "cJSON.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ota_updater.h"
#include "settings.h"

namespace config_service {
namespace {

constexpr const char* TAG = "config";
constexpr size_t MAX_LINE = 1024;
bool running = false;
volatile bool update_requested = false;

void reply(cJSON* json) {
  char* text = cJSON_PrintUnformatted(json);
  // One printf call per reply, so log output from other tasks can't land in
  // the middle of it. The leading newline ends any log line that another task
  // has only partly written, so the reply starts on a line of its own.
  printf("\n@@CFG %s\n", text);
  fflush(stdout);
  cJSON_free(text);
  cJSON_Delete(json);
}

void replyError(const std::string& error) {
  cJSON* json = cJSON_CreateObject();
  cJSON_AddBoolToObject(json, "ok", false);
  cJSON_AddStringToObject(json, "error", error.c_str());
  reply(json);
}

void replyOk(cJSON* json = nullptr) {
  if (json == nullptr) json = cJSON_CreateObject();
  cJSON_AddBoolToObject(json, "ok", true);
  reply(json);
}

void handle(const std::string& line) {
  cJSON* request = cJSON_Parse(line.c_str());
  if (request == nullptr) {
    replyError("invalid JSON");
    return;
  }
  const cJSON* cmd = cJSON_GetObjectItem(request, "cmd");
  const std::string command = cJSON_IsString(cmd) ? cmd->valuestring : "";

  if (command == "hello") {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "device", "esp-ePaper-bin-tracker");
    cJSON_AddStringToObject(json, "version", ota::currentVersion());
    replyOk(json);
  } else if (command == "get") {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "settings", settings::toJson(settings::get()));
    replyOk(json);
  } else if (command == "set") {
    Settings updated = settings::get();
    std::string error = settings::applyJson(cJSON_GetObjectItem(request, "settings"), updated);
    if (error.empty()) error = settings::save(updated);
    if (error.empty()) {
      ESP_LOGI(TAG, "settings saved");
      replyOk();
    } else {
      replyError(error);
    }
  } else if (command == "reset") {
    if (settings::reset() == ESP_OK) {
      ESP_LOGI(TAG, "settings reset to defaults");
      replyOk();
    } else {
      replyError("Couldn't clear the saved settings");
    }
  } else if (command == "restart") {
    replyOk();
    vTaskDelay(pdMS_TO_TICKS(200));  // let the reply go out
    esp_restart();
  } else if (command == "update") {
    update_requested = true;
    replyOk();
  } else {
    replyError("unknown command");
  }
  cJSON_Delete(request);
}

void listen(void*) {
  std::string line;
  uint8_t buf[64];
  for (;;) {
    const int n = usb_serial_jtag_read_bytes(buf, sizeof(buf), portMAX_DELAY);
    for (int i = 0; i < n; ++i) {
      const char c = static_cast<char>(buf[i]);
      if (c == '\n' || c == '\r') {
        if (!line.empty() && line[0] == '{') handle(line);
        line.clear();
      } else if (line.size() < MAX_LINE) {
        line += c;
      }
    }
  }
}

}  // namespace

void start() {
  if (running) return;
  vTaskDelay(pdMS_TO_TICKS(100));  // give the USB host time to start polling
  if (!usb_serial_jtag_is_connected()) return;

  usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  config.rx_buffer_size = 1024;
  config.tx_buffer_size = 1024;
  if (usb_serial_jtag_driver_install(&config) != ESP_OK) {
    ESP_LOGW(TAG, "USB serial driver install failed");
    return;
  }
  usb_serial_jtag_vfs_use_driver();
  xTaskCreate(listen, "config", 6 * 1024, nullptr, 3, nullptr);
  running = true;
  ESP_LOGI(TAG, "config page can connect over USB");
}

bool takeUpdateRequest() {
  if (!update_requested) return false;
  update_requested = false;
  return true;
}

void stop() {
  if (!running) return;
  usb_serial_jtag_vfs_use_nonblocking();
}

}  // namespace config_service
