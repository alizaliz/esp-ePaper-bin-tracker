#pragma once

#include <string>
#include <vector>

#include "collection_types.h"

class DisplayManager {
 public:
  void init();
  void showCollectionDays(const std::vector<CollectionDay>& days);
  void showAddressSummary(const AddressDetails& details);
  void sleep();
};
