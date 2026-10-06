// Tests for src/council_parser.cpp. Plain C++, run on a computer:
//   tests/run.sh                 unit tests against tests/fixtures/
//   tests/run.sh --live FILE     check a freshly downloaded Council page
//
// --live is used by the weekly GitHub Actions check: it fails if the page no
// longer parses, or the dates it gives are implausible.

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "council_parser.h"

namespace {

int failures = 0;

#define CHECK(cond)                                                \
  do {                                                             \
    if (!(cond)) {                                                 \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
      ++failures;                                                  \
    }                                                              \
  } while (0)

std::string readFile(const char* path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    printf("can't read %s\n", path);
    exit(2);
  }
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

bool sameDate(const Date& d, int y, int m, int day) {
  return d.year == y && d.month == m && d.day == day;
}

const char* binName(BinType t) {
  switch (t) {
    case BinType::Rubbish: return "rubbish";
    case BinType::Recycling: return "recycling";
    case BinType::FoodScraps: return "food scraps";
  }
  return "?";
}

void testFixture(const std::string& page) {
  std::vector<CollectionDay> days;
  CHECK(council_parser::parseNextDates(page, 2026, days));
  CHECK(days.size() == 3);
  for (const auto& d : days) {
    // The commercial block in the fixture says Friday 9 October; the
    // household block, which must be the one read, says Thursday 8 October.
    CHECK(sameDate(d.date, 2026, 10, 8));
  }
}

void testStreaming(const std::string& page) {
  // The firmware reads the page in chunks and stops once the dates are in.
  std::string received;
  size_t at = 0;
  while (at < page.size() && !council_parser::hasNextDates(received)) {
    received.append(page, at, 512);
    at += 512;
  }
  CHECK(council_parser::hasNextDates(received));
  std::vector<CollectionDay> days;
  CHECK(council_parser::parseNextDates(received, 2026, days));
  CHECK(days.size() == 3);
}

void testDates() {
  Date d;
  CHECK(council_parser::parseDate("Thursday, 8 October", 2026, d) && sameDate(d, 2026, 10, 8));
  // Year rollover: the year is the one closest to the reference year in
  // which that date falls on that weekday.
  CHECK(council_parser::parseDate("Friday, 1 January", 2026, d) && sameDate(d, 2027, 1, 1));
  CHECK(council_parser::parseDate("Wednesday, 30 December", 2027, d) && sameDate(d, 2026, 12, 30));
  CHECK(!council_parser::parseDate("Someday, 1 May", 2026, d));
  CHECK(!council_parser::parseDate("Thursday, 32 October", 2026, d));
  CHECK(!council_parser::parseDate("Thursday 8 October", 2026, d));
}

void testMissingBlock() {
  std::vector<CollectionDay> days;
  CHECK(!council_parser::hasNextDates("<html>nothing here</html>"));
  CHECK(!council_parser::parseNextDates("<html>nothing here</html>", 2026, days));
}

// Days from today (local time) to a date.
long daysFromToday(const Date& d) {
  const time_t now = time(nullptr);
  struct tm today = *localtime(&now);
  today.tm_hour = 12;
  struct tm target = {};
  target.tm_year = d.year - 1900;
  target.tm_mon = d.month - 1;
  target.tm_mday = d.day;
  target.tm_hour = 12;
  return lround(difftime(mktime(&target), mktime(&today)) / 86400.0);
}

int live(const std::string& page) {
  const time_t now = time(nullptr);
  const int year = localtime(&now)->tm_year + 1900;
  std::vector<CollectionDay> days;
  if (!council_parser::parseNextDates(page, year, days)) {
    printf("FAIL: no household collection dates found. The Council page layout may have changed.\n");
    return 1;
  }
  int status = 0;
  for (const auto& d : days) {
    const long in_days = daysFromToday(d.date);
    printf("%-12s %04d-%02d-%02d (in %ld days)\n", binName(d.type), d.date.year, d.date.month,
           d.date.day, in_days);
    // Collections are at most fortnightly; allow extra for public holidays.
    if (in_days < -1 || in_days > 31) {
      printf("FAIL: %s date is implausible\n", binName(d.type));
      status = 1;
    }
  }
  if (status == 0) printf("OK: the Council page still parses\n");
  return status;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--live") return live(readFile(argv[2]));

  const std::string page = readFile(argc > 1 ? argv[1] : "tests/fixtures/collection_day_page.html");
  testFixture(page);
  testStreaming(page);
  testDates();
  testMissingBlock();
  if (failures) {
    printf("%d check(s) failed\n", failures);
    return 1;
  }
  printf("All council parser tests passed\n");
  return 0;
}
