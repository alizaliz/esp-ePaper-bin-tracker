#pragma once

#include <string>
#include <vector>

#include "collection_types.h"

// Parses the Auckland Council collection day page for one address. Plain C++
// with no ESP-IDF dependencies, so it can be tested on a computer.
namespace council_parser {

// True once `html` (the start of the page) contains the whole household
// "Your next collection dates" block, so the rest of the page can be skipped.
bool hasNextDates(const std::string& html);

// Extracts the household next collection dates. The page gives dates like
// "Thursday, 8 October" with no year, so the year is the one closest to
// reference_year on which that date falls on that weekday.
bool parseNextDates(const std::string& html, int reference_year,
                    std::vector<CollectionDay>& days);

// Parses "Thursday, 8 October". Exposed for testing.
bool parseDate(const std::string& text, int reference_year, Date& date);

}  // namespace council_parser
