// Copyright 2026 R
#include "service/stats_service.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "common/file_util.h"
#include "common/time_util.h"
#include "service/account_service.h"

namespace ticketing {
namespace {

// CSV 字段：含逗号、引号或换行时用引号包起来，内部引号写两次。
std::string CsvField(const std::string& text) {
  if (text.find_first_of(",\"\r\n") == std::string::npos) return text;
  std::string out = "\"";
  for (const char c : text) {
    out += c;
    if (c == '"') out += '"';
  }
  return out + "\"";
}

}  // namespace

double Counts::checkin_rate() const {
  const int denominator = occupied();
  return denominator == 0 ? 0.0
                          : static_cast<double>(checked_in) / denominator;
}

void StatsService::Accumulate(Id activity_id, Counts* counts) const {
  ++counts->activities;
  for (const EventSession* session : store_->SessionsOf(activity_id)) {
    if (session->cancelled) continue;
    for (const TicketType* type : store_->TicketTypesOf(session->id)) {
      if (!type->cancelled) counts->capacity += type->capacity;
    }
  }
  for (const Registration& reg : store_->data().registrations) {
    const EventSession* session = store_->FindSession(reg.session_id);
    if (session == nullptr || session->activity_id != activity_id) continue;
    switch (reg.status) {
      case RegStatus::kConfirmed: ++counts->confirmed; break;
      case RegStatus::kCheckedIn: ++counts->checked_in; break;
      case RegStatus::kAbsent: ++counts->absent; break;
      case RegStatus::kWaiting: ++counts->waiting; break;
      case RegStatus::kCancelled: ++counts->cancelled; break;
      case RegStatus::kExpired: break;
    }
  }
}

Status StatsService::ActivityCounts(const Actor& actor, Id activity_id,
                                    Counts* out) const {
  *out = Counts();
  const Activity* activity = store_->FindActivity(activity_id);
  if (activity == nullptr || !CanManage(actor, *activity)) {
    return Status::Error("没有权限查看该活动的统计");
  }
  Accumulate(activity_id, out);
  return Status::Ok();
}

Status StatsService::PlatformCounts(const Actor& actor, Counts* out) const {
  *out = Counts();
  if (actor.role != Role::kAdmin) {
    return Status::Error("只有管理员可以查看平台统计");
  }
  for (const Activity& activity : store_->data().activities) {
    if (activity.status != ActivityStatus::kDraft) Accumulate(activity.id, out);
  }
  return Status::Ok();
}

Status StatsService::OccupiedByCategory(
    const Actor& actor, std::vector<std::pair<std::string, int>>* out) const {
  out->clear();
  if (actor.role != Role::kAdmin) {
    return Status::Error("只有管理员可以查看平台统计");
  }
  std::map<std::string, int> totals;
  for (const Activity& activity : store_->data().activities) {
    if (activity.status == ActivityStatus::kDraft) continue;
    Counts counts;
    Accumulate(activity.id, &counts);
    totals[activity.category] += counts.occupied();
  }
  out->assign(totals.begin(), totals.end());
  std::stable_sort(out->begin(), out->end(),
                   [](const std::pair<std::string, int>& a,
                      const std::pair<std::string, int>& b) {
                     return a.second > b.second;
                   });
  return Status::Ok();
}

Status StatsService::Roster(const Actor& actor, Id activity_id,
                            std::vector<RosterRow>* out) const {
  out->clear();
  const Activity* activity = store_->FindActivity(activity_id);
  if (activity == nullptr || !CanManage(actor, *activity)) {
    return Status::Error("没有权限查看该活动的名单");
  }
  for (const Registration& reg : store_->data().registrations) {
    const EventSession* session = store_->FindSession(reg.session_id);
    if (session == nullptr || session->activity_id != activity_id) continue;
    RosterRow row;
    row.registration_id = reg.id;
    const User* user = store_->FindUser(reg.user_id);
    row.name = user == nullptr ? "" : user->display_name;
    row.session = FormatShort(session->start_at);
    const TicketType* type = store_->FindTicketType(reg.ticket_type_id);
    row.ticket_type = type == nullptr ? "" : type->name;
    row.status = reg.status;
    row.created_at = reg.created_at;
    for (const Ticket& ticket : store_->data().tickets) {
      if (ticket.registration_id != reg.id) continue;
      if (const Checkin* checkin = store_->FindCheckin(ticket.code)) {
        row.checked_at = checkin->checked_at;
      }
    }
    out->push_back(row);
  }
  std::sort(out->begin(), out->end(),
            [](const RosterRow& a, const RosterRow& b) {
              return a.registration_id < b.registration_id;
            });
  return Status::Ok();
}

Status StatsService::ExportCsv(const Actor& actor, Id activity_id,
                               const std::string& path) const {
  std::vector<RosterRow> rows;
  Status status = Roster(actor, activity_id, &rows);
  if (!status.ok()) return status;
  // 开头的 BOM 让 Excel 按 UTF-8 识别中文。
  std::string csv = "\xEF\xBB\xBF";
  csv += "报名编号,姓名,场次,票种,状态,报名时间,签到时间\n";
  for (const RosterRow& row : rows) {
    csv += "R-" + std::to_string(row.registration_id) + ',' +
           CsvField(row.name) + ',' + CsvField(row.session) + ',' +
           CsvField(row.ticket_type) + ',' + RegStatusName(row.status) + ',' +
           FormatTime(row.created_at) + ',' +
           (row.checked_at == 0 ? "" : FormatTime(row.checked_at)) + '\n';
  }
  if (!WriteWholeFile(path, csv)) return Status::Error("无法写入文件 " + path);
  return Status::Ok("已导出 " + std::to_string(rows.size()) + " 条到 " + path);
}

}  // namespace ticketing
