#include "storage.h"

#include "esp_check.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace storage {
namespace {

constexpr const char* TAG = "storage";
constexpr const char* NAMESPACE = "bintracker";

}  // namespace

esp_err_t init() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS erase failed");
    err = nvs_flash_init();
  }
  return err;
}

esp_err_t save(const char* key, const void* data, size_t size) {
  nvs_handle_t handle;
  ESP_RETURN_ON_ERROR(nvs_open(NAMESPACE, NVS_READWRITE, &handle), TAG, "open failed");
  esp_err_t err = nvs_set_blob(handle, key, data, size);
  if (err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  return err;
}

esp_err_t load(const char* key, void* data, size_t size) {
  nvs_handle_t handle;
  // Opening read-only fails with ESP_ERR_NVS_NOT_FOUND until something has
  // been saved. That's expected on first boot, so it isn't logged.
  esp_err_t err = nvs_open(NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) return err;
  size_t stored = 0;
  err = nvs_get_blob(handle, key, nullptr, &stored);
  if (err == ESP_OK && stored != size) err = ESP_ERR_INVALID_SIZE;
  if (err == ESP_OK) err = nvs_get_blob(handle, key, data, &size);
  nvs_close(handle);
  return err;
}

}  // namespace storage
