#pragma once

#include <string>

struct CollectionDay {
  std::string type;
  std::string label;
  std::string date;
};

struct AddressDetails {
  std::string displayAddress;
  std::string addressId;
  std::string unit;
  std::string street;
  std::string suburb;
};
