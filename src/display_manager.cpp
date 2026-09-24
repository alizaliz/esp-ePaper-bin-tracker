#include "display_manager.h"

#include <iostream>
#include <string>
#include <vector>

void DisplayManager::init() {
  std::cout << "Display initialized\n";
}

void DisplayManager::showCollectionDays(const std::vector<CollectionDay>& days) {
  std::cout << "Collection schedule:\n";
  for (const auto& day : days) {
    std::cout << day.type << ": " << day.date << "\n";
  }
}

void DisplayManager::showAddressSummary(const AddressDetails& details) {
  std::cout << "Address: " << details.displayAddress << "\n";
  std::cout << "Address ID: " << details.addressId << "\n";
}
