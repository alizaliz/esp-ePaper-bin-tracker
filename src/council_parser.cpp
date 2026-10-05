#include "council_parser.h"

#include <cstdlib>
#include <cstring>

namespace council_parser {
namespace {

// Markers in the page around the household (not commercial) collection block.
constexpr const char* HOUSEHOLD_MARKER = "Household collection";
constexpr const char* BLOCK_END_MARKER = "How often do I put my bins out";

struct Label {
  const char* text;
  BinType type;
};
constexpr Label LABELS[] = {
    {"Rubbish: <b>", BinType::Rubbish},
    {"Food scraps: <b>", BinType::FoodScraps},
    {"Recycling: <b>", BinType::Recycling},
};

const char* const WEEKDAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                "Thursday", "Friday", "Saturday"};
const char* const MONTHS[] = {"January", "February", "March", "April", "May", "June", "July",
                              "August", "September", "October", "November", "December"};

int indexOf(const char* const* names, int count, const std::string& word) {
  for (int i = 0; i < count; ++i) {
    if (word == names[i]) return i;
  }
  return -1;
}

}  // namespace

bool hasNextDates(const std::string& html) {
  const size_t start = html.find(HOUSEHOLD_MARKER);
  return start != std::string::npos && html.find(BLOCK_END_MARKER, start) != std::string::npos;
}

bool parseDate(const std::string& text, int reference_year, Date& date) {
  // "<Weekday>, <day> <Month>"
  const size_t comma = text.find(',');
  if (comma == std::string::npos) return false;
  const int weekday = indexOf(WEEKDAYS, 7, text.substr(0, comma));

  const char* p = text.c_str() + comma + 1;
  char* end = nullptr;
  const long day = strtol(p, &end, 10);
  if (end == p || day < 1 || day > 31) return false;
  while (*end == ' ') ++end;
  const int month = indexOf(MONTHS, 12, end) + 1;
  if (weekday < 0 || month < 1) return false;

  for (int offset : {0, 1, -1}) {
    const Date candidate = {reference_year + offset, month, static_cast<int>(day)};
    if (dayOfWeek(candidate) == weekday) {
      date = candidate;
      return true;
    }
  }
  return false;
}

bool parseNextDates(const std::string& html, int reference_year,
                    std::vector<CollectionDay>& days) {
  const size_t start = html.find(HOUSEHOLD_MARKER);
  if (start == std::string::npos) return false;
  const size_t block_end = html.find(BLOCK_END_MARKER, start);

  days.clear();
  for (const Label& label : LABELS) {
    const size_t pos = html.find(label.text, start);
    if (pos == std::string::npos || pos > block_end) continue;  // bin not collected here
    const size_t text_start = pos + strlen(label.text);
    const size_t text_end = html.find("</b>", text_start);
    if (text_end == std::string::npos) return false;

    CollectionDay day = {label.type, {}};
    if (!parseDate(html.substr(text_start, text_end - text_start), reference_year, day.date)) {
      return false;
    }
    days.push_back(day);
  }
  return !days.empty();
}

}  // namespace council_parser
