#pragma once

#include <cstddef>
#include <functional>

#include "esp_err.h"

namespace https {

// Called with each chunk of the response body as it arrives. Return false to
// stop reading early (e.g. once the needed data has been found).
using ChunkHandler = std::function<bool(const char* data, size_t len)>;

// GETs a URL over HTTPS, verifying the server against ESP-IDF's certificate
// bundle, and streams the body to on_chunk without buffering it all in memory.
// Stops after max_bytes. Fails unless the server returns 200.
esp_err_t get(const char* url, size_t max_bytes, const ChunkHandler& on_chunk);

}  // namespace https
