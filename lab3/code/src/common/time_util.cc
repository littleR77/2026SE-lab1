// Copyright 2026 R
#include "common/time_util.h"

#include <cstdio>
#include <string>

namespace ticketing {
namespace {

// 公历日期与“自 1970-01-01 起的天数”互相换算（Howard Hinnant 的算法）。
int64_t DaysFromCivil(int year, int month, int day) {
  year -= month <= 2 ? 1 : 0;
  const int64_t era = (year >= 0 ? year : year - 399) / 400;
  const int64_t yoe = year - era * 400;
  const int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

void CivilFromDays(int64_t days, int* year, int* month, int* day) {
  days += 719468;
  const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const int64_t doe = days - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  *day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  *month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  *year = static_cast<int>(yoe + era * 400 + (*month <= 2 ? 1 : 0));
}

// 把分钟数向下取整到天（处理负数）。
int64_t FloorDiv(int64_t value, int64_t divisor) {
  int64_t quotient = value / divisor;
  if (value % divisor != 0 && value < 0) --quotient;
  return quotient;
}

// 从 text[*pos] 起读取恰好 width 位数字。
bool ReadNumber(const std::string& text, size_t* pos, size_t width,
                int* out) {
  if (*pos + width > text.size()) return false;
  int value = 0;
  for (size_t i = 0; i < width; ++i) {
    const char c = text[*pos + i];
    if (c < '0' || c > '9') return false;
    value = value * 10 + (c - '0');
  }
  *pos += width;
  *out = value;
  return true;
}

bool Expect(const std::string& text, size_t* pos, char c) {
  if (*pos >= text.size() || text[*pos] != c) return false;
  ++*pos;
  return true;
}

}  // namespace

Time MakeTime(int year, int month, int day, int hour, int minute) {
  return DaysFromCivil(year, month, day) * kMinutesPerDay +
         hour * kMinutesPerHour + minute;
}

std::string FormatTime(Time time) {
  const int64_t days = FloorDiv(time, kMinutesPerDay);
  const int64_t minutes = time - days * kMinutesPerDay;
  int year = 0, month = 0, day = 0;
  CivilFromDays(days, &year, &month, &day);
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d", year, month,
           day, static_cast<int>(minutes / 60), static_cast<int>(minutes % 60));
  return buffer;
}

std::string FormatShort(Time time) {
  // "2026-11-05 19:00" 去掉年份部分即 "11-05 19:00"，再把 '-' 换成 '/'。
  std::string text = FormatTime(time).substr(5);
  text[2] = '/';
  return text;
}

const char* WeekdayName(Time time) {
  static const char* const kNames[] = {"周日", "周一", "周二", "周三",
                                       "周四", "周五", "周六"};
  // 1970-01-01 是周四。
  const int64_t days = FloorDiv(time, kMinutesPerDay);
  const int64_t index = ((days + 4) % 7 + 7) % 7;
  return kNames[index];
}

bool ParseTime(const std::string& text, Time* out) {
  size_t pos = 0;
  int year = 0, month = 0, day = 0, hour = 0, minute = 0;
  if (!ReadNumber(text, &pos, 4, &year) || !Expect(text, &pos, '-') ||
      !ReadNumber(text, &pos, 2, &month) || !Expect(text, &pos, '-') ||
      !ReadNumber(text, &pos, 2, &day)) {
    return false;
  }
  if (pos < text.size()) {
    if (!Expect(text, &pos, ' ') || !ReadNumber(text, &pos, 2, &hour) ||
        !Expect(text, &pos, ':') || !ReadNumber(text, &pos, 2, &minute) ||
        pos != text.size()) {
      return false;
    }
  }
  if (month < 1 || month > 12 || day < 1 || hour > 23 || minute > 59) {
    return false;
  }
  // 通过往返换算排除 2 月 30 日之类不存在的日期。
  int check_year = 0, check_month = 0, check_day = 0;
  CivilFromDays(DaysFromCivil(year, month, day), &check_year, &check_month,
                &check_day);
  if (check_month != month || check_day != day) return false;
  *out = MakeTime(year, month, day, hour, minute);
  return true;
}

}  // namespace ticketing
