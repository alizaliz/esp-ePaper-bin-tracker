#pragma once

#include <cstddef>

#include "esp_err.h"

// Small values kept in the board's flash (NVS), so they survive power loss
// and reflashing, unlike RTC memory, which only survives deep sleep.
namespace storage {

// Must be called once at boot, before Wi-Fi (which also uses NVS).
esp_err_t init();

esp_err_t save(const char* key, const void* data, size_t size);

// Fails if the key is missing or the stored size doesn't match.
esp_err_t load(const char* key, void* data, size_t size);

}  // namespace storage
