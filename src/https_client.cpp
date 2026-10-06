#include "https_client.h"

#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"

namespace https {
namespace {

constexpr const char* TAG = "https";
constexpr int TIMEOUT_MS = 10000;

}  // namespace

esp_err_t get(const char* url, size_t max_bytes, const ChunkHandler& on_chunk) {
  esp_http_client_config_t config = {};
  config.url = url;
  config.timeout_ms = TIMEOUT_MS;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.buffer_size = 4096;  // response headers must fit (GitHub's API sends ~2KB)
  config.user_agent = "esp-ePaper-bin-tracker";

  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_RETURN_ON_FALSE(client != nullptr, ESP_FAIL, TAG, "client init failed");

  esp_err_t err = esp_http_client_open(client, 0);
  if (err == ESP_OK) {
    esp_http_client_fetch_headers(client);
    const int status = esp_http_client_get_status_code(client);
    if (status != 200) {
      ESP_LOGW(TAG, "HTTP %d from %s", status, url);
      err = ESP_FAIL;
    }
  }

  char buf[1024];
  size_t total = 0;
  while (err == ESP_OK && total < max_bytes) {
    const int n = esp_http_client_read(client, buf, sizeof(buf));
    if (n < 0) {
      err = ESP_FAIL;
    } else if (n == 0 || !on_chunk(buf, n)) {
      break;  // end of body, or the handler has what it needs
    } else {
      total += n;
    }
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  return err;
}

}  // namespace https
