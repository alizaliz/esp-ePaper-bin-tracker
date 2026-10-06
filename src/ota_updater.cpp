#include "ota_updater.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"

#include "https_client.h"
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
constexpr size_t MAX_RELEASE_JSON = 64 * 1024;  // the latest release is about 8KB

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

esp_err_t fetchLatestRelease(Release& release) {
  const std::string url =
      std::string("https://api.github.com/repos/") + UPDATE_REPOSITORY + "/releases/latest";
  std::string body;
  ESP_RETURN_ON_ERROR(https::get(url.c_str(), MAX_RELEASE_JSON,
                                 [&](const char* data, size_t len) {
                                   body.append(data, len);
                                   return true;
                                 }),
                      TAG, "release check failed");

  cJSON* root = cJSON_Parse(body.c_str());
  ESP_RETURN_ON_FALSE(root != nullptr, ESP_ERR_INVALID_RESPONSE, TAG, "invalid release JSON");
  const cJSON* tag = cJSON_GetObjectItem(root, "tag_name");
  if (cJSON_IsString(tag)) release.tag = tag->valuestring;
  const cJSON* asset;
  cJSON_ArrayForEach(asset, cJSON_GetObjectItem(root, "assets")) {
    const cJSON* name = cJSON_GetObjectItem(asset, "name");
    const cJSON* link = cJSON_GetObjectItem(asset, "browser_download_url");
    if (cJSON_IsString(name) && cJSON_IsString(link)) {
      const std::string n = name->valuestring;
      if (n.size() > 8 && n.compare(n.size() - 8, 8, "-app.bin") == 0) {
        release.appUrl = link->valuestring;
      }
    }
  }
  cJSON_Delete(root);
  ESP_RETURN_ON_FALSE(!release.tag.empty() && !release.appUrl.empty(), ESP_ERR_INVALID_RESPONSE,
                      TAG, "latest release has no app image");
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

esp_err_t checkAndInstall(bool force) {
  const Version current = parseVersion(currentVersion());
  if (!current.valid() && !force) {
    ESP_LOGI(TAG, "development build %s; skipping the automatic update check", currentVersion());
    return ESP_ERR_NOT_FOUND;
  }

  Release latest;
  ESP_RETURN_ON_ERROR(fetchLatestRelease(latest), TAG, "");
  const Version available = parseVersion(latest.tag.c_str());
  ESP_LOGI(TAG, "running %s, latest release %s", currentVersion(), latest.tag.c_str());

  if (!available.valid() || (current.valid() && !(available > current))) {
    return ESP_ERR_NOT_FOUND;
  }
  if (latest.tag == rolledBackVersion()) {
    ESP_LOGW(TAG, "%s was rolled back after failing; not installing it again", latest.tag.c_str());
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
  ESP_RETURN_ON_ERROR(esp_https_ota(&config), TAG, "download or install failed");
  ESP_LOGI(TAG, "%s installed; it starts after a restart", latest.tag.c_str());
  return ESP_OK;
}

}  // namespace ota
