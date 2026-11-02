// Copyright 2026 R
// M11 名单与统计：容量、报名、候补、签到、缺席统计和名单导出（REQ-017、018）。
#ifndef SERVICE_STATS_SERVICE_H_
#define SERVICE_STATS_SERVICE_H_

#include <string>
#include <utility>
#include <vector>

#include "common/status.h"
#include "data/store.h"
#include "domain/model.h"
#include "service/audit_log.h"

namespace ticketing {

struct Counts {
  int activities = 0;
  int capacity = 0;
  int confirmed = 0;   // 报名成功（未签到）
  int checked_in = 0;
  int absent = 0;
  int waiting = 0;
  int cancelled = 0;
  // 占用 = 报名成功 + 已签到 + 缺席；候补和取消不计入。
  int occupied() const { return confirmed + checked_in + absent; }
  // 签到率 = 已签到 / 占用，分母为 0 时为 0。
  double checkin_rate() const;
};

// 名单中的一行。
struct RosterRow {
  Id registration_id = 0;
  std::string name;
  std::string session;
  std::string ticket_type;
  RegStatus status = RegStatus::kWaiting;
  Time created_at = 0;
  Time checked_at = 0;  // 未签到为 0
};

class StatsService {
 public:
  explicit StatsService(Store* store) : store_(store) {}

  // 单个活动的统计。前置条件：actor 能管理该活动。
  Status ActivityCounts(const Actor& actor, Id activity_id, Counts* out) const;
  // 平台统计。前置条件：actor 为管理员。
  Status PlatformCounts(const Actor& actor, Counts* out) const;
  // 各分类的占用名额，按占用降序。前置条件：actor 为管理员。
  Status OccupiedByCategory(
      const Actor& actor,
      std::vector<std::pair<std::string, int>>* out) const;

  // 活动名单，包含所有状态（REQ-017）。前置条件：actor 能管理该活动。
  Status Roster(const Actor& actor, Id activity_id,
                std::vector<RosterRow>* out) const;
  // 把名单导出为 UTF-8（带 BOM，便于 Excel 打开）的 CSV 文件。
  Status ExportCsv(const Actor& actor, Id activity_id,
                   const std::string& path) const;

 private:
  void Accumulate(Id activity_id, Counts* counts) const;

  Store* store_;
};

}  // namespace ticketing

#endif  // SERVICE_STATS_SERVICE_H_
