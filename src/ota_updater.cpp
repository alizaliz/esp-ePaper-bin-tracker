#include "ota_updater.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <strings.h>

#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"

#include "settings.h"

// UPDATE_REPOSITORY comes from config.h, or the example's default without it.
#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

namespace ota {
namespace {

constexpr const char* TAG = "ota";

struct Version {
  int major = -1;
  int minor = 0;
  int patch = 0;
  bool valid() const { return major >= 0; }
  bool operator>(const Version& o) const {
    if (major != o.major) return major > o.major;
    if (minor != o.minor) return minor > o.minor;
    return patch > o.patch;
  }
};

// Parses a release version, exactly "v1.2.3". Development builds are
// described like "v1.2.3-4-gabc1234-dirty" (commits since the tag) and don't
// count as releases.
Version parseVersion(const char* text) {
  Version v;
  int major, minor, patch, end = 0;
  if (text && sscanf(text, "v%d.%d.%d%n", &major, &minor, &patch, &end) == 3 &&
      text[end] == '\0') {
    v.major = major;
    v.minor = minor;
    v.patch = patch;
  }
  return v;
}

struct Release {
  std::string tag;
  std::string appUrl;
};

esp_err_t onHttpEvent(esp_http_client_event_t* event) {
  if (event->event_id == HTTP_EVENT_ON_HEADER && strcasecmp(event->header_key, "Location") == 0) {
    *static_cast<std::string*>(event->user_data) = event->header_value;
  }
  return ESP_OK;
}

// Finds the latest release from where github.com/<repo>/releases/latest
// redirects to (.../releases/tag/<tag>). This avoids GitHub's API, which
// allows only 60 requests an hour per network without a login.
esp_err_t fetchLatestRelease(Release& release) {
  const std::string base = std::string("https://github.com/") + UPDATE_REPOSITORY + "/releases";
  const std::string url = base + "/latest";
  std::string location;

  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.method = HTTP_METHOD_HEAD;
  config.disable_auto_redirect = true;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.timeout_ms = 10000;
  config.buffer_size = 8192;  // github.com sends about 5KB of headers
  config.user_agent = "esp-ePaper-bin-tracker";
  config.event_handler = onHttpEvent;
  config.user_data = &location;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_RETURN_ON_FALSE(client != nullptr, ESP_FAIL, TAG, "client init failed");
  const esp_err_t err = esp_http_client_perform(client);
  const int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  ESP_RETURN_ON_ERROR(err, TAG, "release check failed");
  ESP_RETURN_ON_FALSE(status == 302, ESP_ERR_INVALID_RESPONSE, TAG, "release check: HTTP %d",
                      status);

  const std::string marker = "/releases/tag/";
  const size_t at = location.find(marker);
  ESP_RETURN_ON_FALSE(at != std::string::npos, ESP_ERR_NOT_FOUND, TAG, "no releases yet");
  release.tag = location.substr(at + marker.size());
  // Asset name set by .github/workflows/release.yml.
  release.appUrl = base + "/download/" + release.tag + "/bin-tracker-" + release.tag + "-app.bin";
  return ESP_OK;
}

// The version that was installed and then rolled back, if any.
std::string rolledBackVersion() {
  const esp_partition_t* invalid = esp_ota_get_last_invalid_partition();
  esp_app_desc_t desc;
  if (invalid && esp_ota_get_partition_description(invalid, &desc) == ESP_OK) {
    return desc.version;
  }
  return "";
}

}  // namespace

const char* currentVersion() { return esp_app_get_description()->version; }

bool isPendingVerify() {
  esp_ota_img_states_t state;
  return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
         state == ESP_OTA_IMG_PENDING_VERIFY;
}

void markHealthy() {
  if (!isPendingVerify()) return;
  if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
    ESP_LOGI(TAG, "firmware %s confirmed", currentVersion());
  }
}

esp_err_t checkAndInstall(bool force, const Progress& progress) {
  auto report = [&](const char* state, const char* version, int percent = 0) {
    if (progress) progress(state, version, percent);
  };
  const Version current = parseVersion(currentVersion());
  if (!current.valid() && !force) {
    ESP_LOGI(TAG, "development build %s; skipping the automatic update check", currentVersion());
    return ESP_ERR_NOT_FOUND;
  }

  Release latest;
  if (fetchLatestRelease(latest) != ESP_OK) {
    report("failed", "couldn't reach GitHub");
    return ESP_FAIL;
  }
  const Version available = parseVersion(latest.tag.c_str());
  ESP_LOGI(TAG, "running %s, latest release %s", currentVersion(), latest.tag.c_str());

  if (!available.valid() || (current.valid() && !(available > current))) {
    report("uptodate", currentVersion());
    return ESP_ERR_NOT_FOUND;
  }
  if (latest.tag == rolledBackVersion()) {
    ESP_LOGW(TAG, "%s was rolled back after failing; not installing it again", latest.tag.c_str());
    report("uptodate", currentVersion());
    return ESP_ERR_NOT_FOUND;
  }

  ESP_LOGI(TAG, "installing %s", latest.tag.c_str());
  esp_http_client_config_t http = {};
  http.url = latest.appUrl.c_str();
  http.crt_bundle_attach = esp_crt_bundle_attach;
  http.timeout_ms = 20000;
  http.keep_alive_enable = true;
  // GitHub redirects downloads to a CDN address about 1KB long.
  http.buffer_size = 4096;
  http.buffer_size_tx = 2048;
  http.user_agent = "esp-ePaper-bin-tracker";

  esp_https_ota_config_t config = {};
  config.http_config = &http;

  // Download in steps, so progress can be reported.
  esp_https_ota_handle_t ota = nullptr;
  esp_err_t err = esp_https_ota_begin(&config, &ota);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "download failed to start: %s", esp_err_to_name(err));
    report("failed", "download failed");
    return err;
  }
  const char* version = latest.tag.c_str();
  int last_percent = -1;
  report("downloading", version, 0);
  while ((err = esp_https_ota_perform(ota)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
    const int size = esp_https_ota_get_image_size(ota);
    const int percent = size > 0 ? (int)(100LL * esp_https_ota_get_image_len_read(ota) / size) : 0;
    if (percent >= last_percent + 5) {  // every 5% is plenty
      last_percent = percent;
      report("downloading", version, percent);
    }
  }
  if (err != ESP_OK || !esp_https_ota_is_complete_data_received(ota)) {
    ESP_LOGE(TAG, "download failed: %s", esp_err_to_name(err));
    esp_https_ota_abort(ota);
    report("failed", "download failed");
    return err != ESP_OK ? err : ESP_FAIL;
  }
  err = esp_https_ota_finish(ota);  // checks the image and switches to it
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "install failed: %s", esp_err_to_name(err));
    report("failed", "the downloaded firmware didn't verify");
    return err;
  }
  ESP_LOGI(TAG, "%s installed; it starts after a restart", version);
  report("installed", version, 100);
  return ESP_OK;
}

}  // namespace ota
