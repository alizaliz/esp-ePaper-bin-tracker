#include "auckland_council_client.h"

#include "esp_log.h"

#include "council_parser.h"
#include "https_client.h"

namespace auckland_council {
namespace {

constexpr const char* TAG = "council";
constexpr const char* PAGE_URL =
    "https://www.aucklandcouncil.govt.nz/en/rubbish-recycling/rubbish-recycling-collections/"
    "rubbish-recycling-collection-days/";

// The page is about 2.7MB, but the dates are in the first ~16KB. Read until
// they've arrived, with a cap in case the layout changes.
constexpr size_t MAX_BYTES = 96 * 1024;

}  // namespace

bool fetchCollectionDays(const std::string& address_id, int reference_year,
                         std::vector<CollectionDay>& days) {
  if (address_id.empty()) {
    ESP_LOGW(TAG, "no Council address ID set; use the config page");
    return false;
  }
  const std::string url = PAGE_URL + address_id + ".html";
  std::string html;
  html.reserve(24 * 1024);

  const esp_err_t err = https::get(url.c_str(), MAX_BYTES, [&](const char* data, size_t len) {
    html.append(data, len);
    return !council_parser::hasNextDates(html);
  });
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "fetch failed: %s", esp_err_to_name(err));
    return false;
  }
  if (!council_parser::parseNextDates(html, reference_year, days)) {
    ESP_LOGW(TAG, "collection dates not found in the first %u bytes of the page",
             (unsigned)html.size());
    return false;
  }
  return true;
}

}  // namespace auckland_council
