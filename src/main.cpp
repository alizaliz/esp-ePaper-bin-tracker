#include <stdio.h>
#include <string>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "auckland_council_client.h"
#include "collection_types.h"
#include "config.h"
#include "display_manager.h"

static void configure_wifi(void) {
  printf("Wi-Fi SSID: %s\n", WIFI_SSID);
  printf("Using stored address details and a resolved address ID from Auckland Council\n");
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

  configure_wifi();

  while (1) {
    refresh_collection_schedule();
    printf("Waiting 7 days before checking again...\n");
    vTaskDelay(pdMS_TO_TICKS(7UL * 24UL * 60UL * 60UL * 1000UL));
  }
}
