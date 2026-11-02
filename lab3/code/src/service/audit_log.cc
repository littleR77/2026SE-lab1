// Copyright 2026 R
#include "service/audit_log.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ticketing {

void AuditLog::Record(const Actor& actor, const std::string& action,
                      const std::string& target, Id activity_id,
                      const std::string& result) {
  OperationLog log;
  log.id = store_->NextId();
  log.actor_id = actor.user_id;
  if (const User* user = store_->FindUser(actor.user_id)) {
    log.actor_name = user->display_name;
  } else {
    // 没有用户编号的操作者：游客，或者定时调度等系统任务。
    log.actor_name = actor.role == Role::kGuest ? "游客" : "系统";
  }
  log.action = action;
  log.target = target;
  log.activity_id = activity_id;
  log.result = result;
  log.created_at = clock_->Now();
  store_->data().logs.push_back(log);
  store_->MarkDirty();
}

Status AuditLog::Query(const Actor& actor, const LogFilter& filter,
                       std::vector<const OperationLog*>* out) const {
  out->clear();
  if (actor.role != Role::kAdmin && actor.role != Role::kOrganizer) {
    return Status::Error("只有组织者和管理员可以查询操作日志");
  }
  for (const OperationLog& log : store_->data().logs) {
    const Activity* activity = store_->FindActivity(log.activity_id);
    if (actor.role == Role::kOrganizer &&
        (activity == nullptr || activity->organizer_id != actor.user_id)) {
      continue;
    }
    if (filter.actor_id != 0 && log.actor_id != filter.actor_id) continue;
    if (!filter.action.empty() && log.action != filter.action) continue;
    if (!filter.keyword.empty()) {
      const bool in_target = log.target.find(filter.keyword) !=
                             std::string::npos;
      const bool in_title =
          activity != nullptr &&
          activity->title.find(filter.keyword) != std::string::npos;
      if (!in_target && !in_title) continue;
    }
    out->push_back(&log);
  }
  std::sort(out->begin(), out->end(),
            [](const OperationLog* a, const OperationLog* b) {
              if (a->created_at != b->created_at) {
                return a->created_at > b->created_at;
              }
              return a->id > b->id;
            });
  return Status::Ok();
}

}  // namespace ticketing
