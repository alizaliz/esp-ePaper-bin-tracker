#pragma once

#include <string>
#include <vector>

#include "collection_types.h"

namespace auckland_council {

// Fetches the next household collection dates for an address ID from the
// Auckland Council website. Needs a network connection. reference_year is used
// to work out the year, since the page leaves it out.
bool fetchCollectionDays(const std::string& address_id, int reference_year,
                         std::vector<CollectionDay>& days);

}  // namespace auckland_council
