#pragma once

#include <string>
#include <vector>

#include "collection_types.h"

namespace auckland_council {

bool resolveAddress(const std::string& query, AddressDetails& details);
bool fetchCollectionDays(const std::string& addressId, std::vector<CollectionDay>& days);

}  // namespace auckland_council
