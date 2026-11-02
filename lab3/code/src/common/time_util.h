// Copyright 2026 R
// 时间工具与模拟时钟。系统内所有“当前时间”都来自 Clock，
// 这样演示和测试时可以推进时间，触发截止、检票和缺席等规则。
#ifndef COMMON_TIME_UTIL_H_
#define COMMON_TIME_UTIL_H_

#include <string>

#include "domain/model.h"

namespace ticketing {

constexpr Time kMinutesPerHour = 60;
constexpr Time kMinutesPerDay = 24 * 60;

// 由年月日时分构造时间。
Time MakeTime(int year, int month, int day, int hour, int minute);

// 格式化为 "2026-11-05 19:00"。
std::string FormatTime(Time time);
// 格式化为 "11/05 19:00"，用于列表等紧凑场合。
std::string FormatShort(Time time);
// 返回 "周一" 至 "周日"。
const char* WeekdayName(Time time);

// 解析 "YYYY-MM-DD HH:MM"（也接受只有日期的 "YYYY-MM-DD"，视为 00:00）。
// 格式或数值非法时返回 false，且不修改 *out。
bool ParseTime(const std::string& text, Time* out);

// 模拟时钟：保存当前时间，可以设置或推进。
class Clock {
 public:
  explicit Clock(Time now) : now_(now) {}

  Time Now() const { return now_; }
  void Set(Time now) { now_ = now; }
  void Advance(Time minutes) { now_ += minutes; }

 private:
  Time now_;
};

}  // namespace ticketing

#endif  // COMMON_TIME_UTIL_H_
