#pragma once

#include <cstddef>
#include <functional>

#include "esp_err.h"

namespace https {

// Called with each chunk of the response body as it arrives. Return false to
// stop reading early (e.g. once the needed data has been found).
using ChunkHandler = std::function<bool(const char* data, size_t len)>;

// GETs a URL, streaming the body to on_chunk without buffering it all in
// memory. https:// URLs are verified against ESP-IDF's certificate bundle;
// http:// URLs are plain HTTP.
// Stops after max_bytes. Fails unless the server returns 200. timeout_ms
// applies to connecting and to each read.
esp_err_t get(const char* url, size_t max_bytes, const ChunkHandler& on_chunk,
              int timeout_ms = 10000);

}  // namespace https
