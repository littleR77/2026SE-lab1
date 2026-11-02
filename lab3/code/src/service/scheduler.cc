// Copyright 2026 R
#include "service/scheduler.h"

#include <string>

namespace ticketing {
namespace {

const Actor kSystemActor{0, Role::kAdmin};

}  // namespace

SchedulerReport Scheduler::RunDue() {
  SchedulerReport report;
  const Time now = clock_->Now();
  for (EventSession& session : store_->data().sessions) {
    const Activity* activity = store_->FindActivity(session.activity_id);
    if (activity == nullptr || session.cancelled ||
        activity->status != ActivityStatus::kPublished) {
      continue;
    }
    const std::string label =
        activity->title + " " + FormatShort(session.start_at);
    if (now >= session.reg_close_at) {
      const int expired = registrations_->ExpireWaiting(session.id);
      if (expired > 0) {
        audit_->Record(kSystemActor, "候补失效", label, activity->id,
                       std::to_string(expired) + " 条");
        report.expired += expired;
      }
    }
    if (!session.finished && now >= session.end_at) {
      const int absent = registrations_->FinishSession(session.id);
      session.finished = true;
      audit_->Record(kSystemActor, "标记缺席", label, activity->id,
                     std::to_string(absent) + " 条");
      report.absent += absent;
    }
  }
  for (Activity& activity : store_->data().activities) {
    if (activity.status != ActivityStatus::kPublished) continue;
    bool all_done = true;
    bool any_finished = false;
    for (const EventSession* session : store_->SessionsOf(activity.id)) {
      if (session->finished) any_finished = true;
      if (!session->finished && !session->cancelled) all_done = false;
    }
    if (all_done && any_finished) {
      activity.status = ActivityStatus::kEnded;
      audit_->Record(kSystemActor, "活动结束", activity.title, activity.id,
                     "成功");
      ++report.ended;
    }
  }
  if (report.expired + report.absent + report.ended > 0) store_->MarkDirty();
  return report;
}

}  // namespace ticketing
