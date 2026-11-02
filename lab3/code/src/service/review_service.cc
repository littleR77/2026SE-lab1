// Copyright 2026 R
#include "service/review_service.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ticketing {
namespace {

std::string ChangeLabel(Id id) { return "修改申请 #" + std::to_string(id); }

bool IsBlank(const std::string& text) {
  return text.find_first_not_of(" \t\r\n") == std::string::npos;
}

}  // namespace

Status ReviewService::Approve(const Actor& actor, Id activity_id,
                              int expected_version) {
  if (actor.role != Role::kAdmin) return Status::Error("只有管理员可以审核");
  Activity* activity = store_->FindActivity(activity_id);
  if (activity == nullptr || activity->status != ActivityStatus::kPending) {
    return Status::Error("该活动不在待审核状态，可能已被处理");
  }
  if (activity->version != expected_version) {
    return Status::Error("活动内容已变化，请刷新后重新审核");
  }
  for (const EventSession* session : store_->SessionsOf(activity_id)) {
    if (session->reg_close_at <= clock_->Now()) {
      return Status::Error("有场次的报名截止时间已过，不能直接发布，请驳回");
    }
  }
  activity->status = ActivityStatus::kPublished;
  activity->review_reason.clear();
  store_->MarkDirty();
  audit_->Record(actor, "审核申办", activity->title, activity_id, "通过");
  return Status::Ok("已通过，活动发布");
}

Status ReviewService::Reject(const Actor& actor, Id activity_id,
                             const std::string& reason) {
  if (actor.role != Role::kAdmin) return Status::Error("只有管理员可以审核");
  Activity* activity = store_->FindActivity(activity_id);
  if (activity == nullptr || activity->status != ActivityStatus::kPending) {
    return Status::Error("该活动不在待审核状态，可能已被处理");
  }
  if (IsBlank(reason)) return Status::Error("驳回时必须填写原因");
  activity->status = ActivityStatus::kDraft;
  activity->review_reason = reason;
  store_->MarkDirty();
  audit_->Record(actor, "审核申办", activity->title, activity_id, "驳回");
  return Status::Ok("已驳回，活动回到草稿");
}

Status ReviewService::ApproveChange(const Actor& actor, Id change_id,
                                    int* promoted) {
  *promoted = 0;
  if (actor.role != Role::kAdmin) return Status::Error("只有管理员可以审核");
  ChangeRequest* change = store_->FindChangeRequest(change_id);
  if (change == nullptr || change->status != ChangeStatus::kPending) {
    return Status::Error("该申请不在待审核状态，可能已被处理");
  }
  Activity* activity = store_->FindActivity(change->activity_id);
  EventSession* session = store_->FindSession(change->session_id);
  if (activity->status != ActivityStatus::kPublished || session->cancelled ||
      session->finished) {
    return Status::Error("活动或场次状态已变化，不能应用该申请");
  }
  if (activity->version != change->base_version) {
    return Status::Error("活动已被其他修改更新，请驳回后让组织者重新申请");
  }
  for (const CapacityChange& item : change->capacities) {
    const TicketType* type = store_->FindTicketType(item.ticket_type_id);
    if (type == nullptr || item.capacity < type->capacity) {
      return Status::Error("名额只增不减，该申请不能通过");
    }
  }

  // 全部修改在同一事务中生效，避免出现部分更新（REQ-004、REQ-025）。
  Transaction tx(store_);
  session->location = change->new_location;
  session->start_at = change->new_start_at;
  session->end_at = change->new_end_at;
  session->reg_close_at = change->new_reg_close_at;
  session->checkin_start_at = change->new_checkin_start_at;
  for (const CapacityChange& item : change->capacities) {
    store_->FindTicketType(item.ticket_type_id)->capacity = item.capacity;
  }
  change->status = ChangeStatus::kApproved;
  ++activity->version;
  audit_->Record(actor, "审核修改申请", ChangeLabel(change_id), activity->id,
                 "通过");
  for (const CapacityChange& item : change->capacities) {
    *promoted += registrations_->PromoteWaiting(item.ticket_type_id);
  }
  tx.Commit();
  return Status::Ok(*promoted > 0
                        ? "已通过，并递补 " + std::to_string(*promoted) + " 人"
                        : "已通过，修改生效");
}

Status ReviewService::RejectChange(const Actor& actor, Id change_id,
                                   const std::string& reason) {
  if (actor.role != Role::kAdmin) return Status::Error("只有管理员可以审核");
  ChangeRequest* change = store_->FindChangeRequest(change_id);
  if (change == nullptr || change->status != ChangeStatus::kPending) {
    return Status::Error("该申请不在待审核状态，可能已被处理");
  }
  if (IsBlank(reason)) return Status::Error("驳回时必须填写原因");
  change->status = ChangeStatus::kRejected;
  change->reason = reason;
  store_->MarkDirty();
  audit_->Record(actor, "审核修改申请", ChangeLabel(change_id),
                 change->activity_id, "驳回");
  return Status::Ok("已驳回，原配置不变");
}

std::vector<const Activity*> ReviewService::PendingActivities(
    const Actor& actor) const {
  std::vector<const Activity*> result;
  if (actor.role != Role::kAdmin) return result;
  for (const Activity& activity : store_->data().activities) {
    if (activity.status == ActivityStatus::kPending) {
      result.push_back(&activity);
    }
  }
  return result;
}

std::vector<const ChangeRequest*> ReviewService::PendingChanges(
    const Actor& actor) const {
  std::vector<const ChangeRequest*> result;
  if (actor.role != Role::kAdmin) return result;
  for (const ChangeRequest& change : store_->data().change_requests) {
    if (change.status == ChangeStatus::kPending) result.push_back(&change);
  }
  return result;
}

int ReviewService::EstimatePromotion(const ChangeRequest& request) const {
  int total = 0;
  for (const CapacityChange& item : request.capacities) {
    const TicketType* type = store_->FindTicketType(item.ticket_type_id);
    if (type == nullptr) continue;
    const int added = item.capacity - type->capacity +
                      registrations_->Remaining(item.ticket_type_id);
    total += std::min(std::max(added, 0),
                      registrations_->WaitingCount(item.ticket_type_id));
  }
  return total;
}

}  // namespace ticketing
