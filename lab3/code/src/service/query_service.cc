// Copyright 2026 R
#include "service/query_service.h"

#include <algorithm>
#include <string>
#include <vector>

#include "common/time_util.h"
#include "service/account_service.h"

namespace ticketing {
namespace {

bool ContainsText(const std::string& text, const std::string& keyword) {
  return text.find(keyword) != std::string::npos;
}

// 已结束和已取消的活动排在后面。
bool IsClosed(const Activity& activity) {
  return activity.status == ActivityStatus::kEnded ||
         activity.status == ActivityStatus::kCancelled;
}

}  // namespace

Status QueryService::Search(const Actor& actor, const ActivityFilter& filter,
                            std::vector<const Activity*>* out) const {
  out->clear();
  // 结束日期包含当天整天。
  const Time to_end = filter.to == 0 ? 0 : filter.to + kMinutesPerDay;
  if (filter.from != 0 && filter.to != 0 && filter.from > filter.to) {
    return Status::Error("开始日期不能晚于结束日期");
  }
  for (const Activity& activity : store_->data().activities) {
    if (!CanView(actor, activity)) continue;
    if (!filter.category.empty() && activity.category != filter.category) {
      continue;
    }
    if (filter.status >= 0 &&
        static_cast<int>(activity.status) != filter.status) {
      continue;
    }
    const std::vector<EventSession*> sessions =
        store_->SessionsOf(activity.id);
    bool date_match = filter.from == 0 && filter.to == 0;
    bool keyword_match = filter.keyword.empty() ||
                         ContainsText(activity.title, filter.keyword) ||
                         ContainsText(activity.description, filter.keyword);
    for (const EventSession* session : sessions) {
      if ((filter.from == 0 || session->start_at >= filter.from) &&
          (to_end == 0 || session->start_at < to_end)) {
        date_match = true;
      }
      if (!filter.keyword.empty() &&
          ContainsText(session->location, filter.keyword)) {
        keyword_match = true;
      }
    }
    if (date_match && keyword_match) out->push_back(&activity);
  }
  std::stable_sort(out->begin(), out->end(),
                   [this](const Activity* a, const Activity* b) {
                     if (IsClosed(*a) != IsClosed(*b)) return !IsClosed(*a);
                     return FirstStart(a->id) < FirstStart(b->id);
                   });
  return Status::Ok();
}

std::vector<std::string> QueryService::Categories() const {
  std::vector<std::string> result;
  for (const Activity& activity : store_->data().activities) {
    if (std::find(result.begin(), result.end(), activity.category) ==
        result.end()) {
      result.push_back(activity.category);
    }
  }
  return result;
}

Time QueryService::FirstStart(Id activity_id) const {
  Time first = 0;
  for (const EventSession* session : store_->SessionsOf(activity_id)) {
    if (first == 0 || session->start_at < first) first = session->start_at;
  }
  return first;
}

}  // namespace ticketing
