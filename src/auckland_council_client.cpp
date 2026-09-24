#include "auckland_council_client.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace auckland_council {

bool resolveAddress(const std::string& query, AddressDetails& details) {
  // This is a placeholder implementation: the real device should call the official
  // Auckland Council API directly over HTTPS, parse the returned JSON, and store
  // the resolved address data once per install.
  // Example endpoint:
  //   GET https://www.aucklandcouncil.govt.nz/api/address/search?query=500+Queen+Street

  (void)query;

  details.displayAddress = "500 Queen Street, Auckland Central";
  details.addressId = "12342478585";
  details.unit = "";
  details.street = "Queen Street";
  details.suburb = "Auckland Central";

  return !details.addressId.empty();
}

bool fetchCollectionDays(const std::string& addressId, std::vector<CollectionDay>& days) {
  // This placeholder maps the expected data structure for a weekly display update.
  // Example endpoint:
  //   GET https://www.aucklandcouncil.govt.nz/api/kerbside?address=12342478585

  (void)addressId;

  days.clear();
  days.push_back({"Rubbish", "Rubbish", "Thursday, 24 September"});
  days.push_back({"Food scraps", "Food scraps", "Thursday, 24 September"});
  days.push_back({"Recycling", "Recycling", "Thursday, 24 September"});

  return !days.empty();
}

}  // namespace auckland_council
