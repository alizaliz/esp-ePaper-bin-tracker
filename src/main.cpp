#include <stdio.h>
#include <string>
#include <vector>

#include "esp_sleep.h"

#include "auckland_council_client.h"
#include "collection_types.h"
#include "config.h"
#include "display_manager.h"

static void configure_wifi(void) {
  printf("Wi-Fi SSID: %s\n", WIFI_SSID);
  printf("Using stored address details and a resolved address ID from Auckland Council\n");
}

static void log_wakeup_cause(void) {
  if (esp_sleep_get_wakeup_causes() & BIT(ESP_SLEEP_WAKEUP_TIMER)) {
    printf("Woke from deep sleep (timer)\n");
  } else {
    printf("Cold boot or reset\n");
  }
}

static void enter_deep_sleep(void) {
  const uint64_t sleep_us = (uint64_t)REFRESH_INTERVAL_HOURS * 60ULL * 60ULL * 1000000ULL;
  printf("Deep sleeping for %d hours\n", REFRESH_INTERVAL_HOURS);
  esp_sleep_enable_timer_wakeup(sleep_us);
  esp_deep_sleep_start();
}

static void refresh_collection_schedule(void) {
  AddressDetails details;
  std::vector<CollectionDay> days;

  const bool resolved = auckland_council::resolveAddress(ADDRESS_SEARCH_QUERY, details);
  if (!resolved) {
    printf("Failed to resolve address\n");
    return;
  }

  const bool fetched = auckland_council::fetchCollectionDays(details.addressId, days);
  if (!fetched) {
    printf("Failed to fetch collection dates for address ID %s\n", details.addressId.c_str());
    return;
  }

  printf("Resolved address: %s\n", details.displayAddress.c_str());
  printf("Collection schedule:\n");
  for (const auto& day : days) {
    printf("- %s: %s\n", day.type.c_str(), day.date.c_str());
  }
}

extern "C" void app_main(void) {
  printf("ESP32-C6 ePaper tracker booting...\n");
  printf("Official site: https://www.aucklandcouncil.govt.nz/en/rubbish-recycling/rubbish-recycling-collections/rubbish-recycling-collection-days.html\n");
  printf("Runtime flow: resolve address -> fetch collection data -> refresh ePaper display\n");

  log_wakeup_cause();

  DisplayManager display;
  display.init();

  configure_wifi();
  refresh_collection_schedule();

  // The ePaper panel keeps its image without power, so put it into its own
  // deep sleep before the ESP32-C6 powers down.
  display.sleep();
  enter_deep_sleep();
}
