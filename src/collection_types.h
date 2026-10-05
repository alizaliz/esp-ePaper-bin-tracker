#pragma once

struct Date {
  int year = 0;
  int month = 0;  // 1-12
  int day = 0;    // 1-31

  bool operator==(const Date& other) const {
    return year == other.year && month == other.month && day == other.day;
  }
  bool operator<(const Date& other) const {
    if (year != other.year) return year < other.year;
    if (month != other.month) return month < other.month;
    return day < other.day;
  }
};

// 0 = Sunday ... 6 = Saturday (Sakamoto's algorithm).
inline int dayOfWeek(const Date& date) {
  static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  int y = date.year - (date.month < 3 ? 1 : 0);
  return (y + y / 4 - y / 100 + y / 400 + offsets[date.month - 1] + date.day) % 7;
}

enum class BinType { Rubbish, Recycling, FoodScraps };

struct CollectionDay {
  BinType type;
  Date date;  // next collection for this bin
};
