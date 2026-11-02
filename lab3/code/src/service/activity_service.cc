// Copyright 2026 R
#include "service/activity_service.h"

#include <algorithm>
#include <string>
#include <vector>

#include "service/account_service.h"

namespace ticketing {
namespace {

std::string Trim(const std::string& text) {
  const size_t begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return "";
  const size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

bool Contains(const std::vector<Id>& ids, Id id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

std::string ImpactText(const CancelImpact& impact) {
  return "取消 " + std::to_string(impact.cancelled) + " 份报名，清空候补 " +
         std::to_string(impact.waitlist_removed) + " 人";
}

// 检查一个场次的时间规则（REQ-027），prefix 用于错误提示。
Status CheckTimes(const std::string& prefix, Time reg_open, Time reg_close,
                  Time start, Time end, Time checkin_start) {
  if (reg_open >= reg_close) {
    return Status::Error(prefix + "报名开放时间须早于报名截止时间");
  }
  if (reg_close > start) {
    return Status::Error(prefix + "报名截止时间不能晚于活动开始");
  }
  if (start >= end) {
    return Status::Error(prefix + "活动开始时间须早于结束时间");
  }
  if (checkin_start > start) {
    return Status::Error(prefix + "检票开始时间不能晚于活动开始");
  }
  if (checkin_start <= reg_open) {
    return Status::Error(prefix + "检票开始时间须晚于报名开放");
  }
  return Status::Ok();
}

}  // namespace

Status ActivityService::Validate(const ActivityDraft& draft) {
  if (Trim(draft.title).empty()) return Status::Error("请填写活动名称");
  if (Trim(draft.category).empty()) return Status::Error("请选择活动分类");
  if (draft.sessions.empty()) return Status::Error("至少需要一个场次");
  for (size_t i = 0; i < draft.sessions.size(); ++i) {
    const SessionDraft& session = draft.sessions[i];
    const std::string prefix = "第 " + std::to_string(i + 1) + " 个场次：";
    if (Trim(session.location).empty()) {
      return Status::Error(prefix + "请填写地点");
    }
    Status times = CheckTimes(prefix, session.reg_open_at, session.reg_close_at,
                              session.start_at, session.end_at,
                              session.checkin_start_at);
    if (!times.ok()) return times;
    if (session.ticket_types.empty()) {
      return Status::Error(prefix + "至少需要一个票种");
    }
    for (size_t j = 0; j < session.ticket_types.size(); ++j) {
      const TicketTypeDraft& type = session.ticket_types[j];
      if (Trim(type.name).empty()) {
        return Status::Error(prefix + "票种名称不能为空");
      }
      if (type.capacity <= 0) {
        return Status::Error(prefix + "「" + type.name + "」名额须为正整数");
      }
      for (size_t k = 0; k < j; ++k) {
        if (Trim(session.ticket_types[k].name) == Trim(type.name)) {
          return Status::Error(prefix + "票种名称「" + type.name + "」重复");
        }
      }
    }
  }
  return Status::Ok();
}

ActivityDraft ActivityService::LoadDraft(Id activity_id) {
  ActivityDraft draft;
  const Activity* activity = store_->FindActivity(activity_id);
  if (activity == nullptr) return draft;
  draft.title = activity->title;
  draft.description = activity->description;
  draft.category = activity->category;
  for (const EventSession* session : store_->SessionsOf(activity_id)) {
    SessionDraft sd;
    sd.id = session->id;
    sd.location = session->location;
    sd.start_at = session->start_at;
    sd.end_at = session->end_at;
    sd.reg_open_at = session->reg_open_at;
    sd.reg_close_at = session->reg_close_at;
    sd.checkin_start_at = session->checkin_start_at;
    for (const TicketType* type : store_->TicketTypesOf(session->id)) {
      sd.ticket_types.push_back({type->id, type->name, type->capacity});
    }
    draft.sessions.push_back(sd);
  }
  return draft;
}

void ActivityService::WriteSessions(Id activity_id,
                                    const ActivityDraft& draft) {
  Data& data = store_->data();
  std::vector<Id> kept_sessions;
  std::vector<Id> kept_types;
  for (const SessionDraft& sd : draft.sessions) {
    // 表单中带编号的场次就地更新，新场次追加，以保留检票授权等关联。
    EventSession* session = sd.id != 0 ? store_->FindSession(sd.id) : nullptr;
    if (session == nullptr || session->activity_id != activity_id) {
      EventSession created;
      created.id = store_->NextId();
      created.activity_id = activity_id;
      data.sessions.push_back(created);
      session = store_->FindSession(created.id);
    }
    session->location = Trim(sd.location);
    session->start_at = sd.start_at;
    session->end_at = sd.end_at;
    session->reg_open_at = sd.reg_open_at;
    session->reg_close_at = sd.reg_close_at;
    session->checkin_start_at = sd.checkin_start_at;
    const Id session_id = session->id;
    kept_sessions.push_back(session_id);
    for (const TicketTypeDraft& td : sd.ticket_types) {
      TicketType* type = td.id != 0 ? store_->FindTicketType(td.id) : nullptr;
      if (type == nullptr || type->session_id != session_id) {
        TicketType created;
        created.id = store_->NextId();
        created.session_id = session_id;
        data.ticket_types.push_back(created);
        type = store_->FindTicketType(created.id);
      }
      type->name = Trim(td.name);
      type->capacity = td.capacity;
      kept_types.push_back(type->id);
    }
  }
  // 删除表单中已经移除的场次和票种。
  std::vector<Id> removed_sessions;
  for (const EventSession& session : data.sessions) {
    if (session.activity_id == activity_id &&
        !Contains(kept_sessions, session.id)) {
      removed_sessions.push_back(session.id);
    }
  }
  data.ticket_types.erase(
      std::remove_if(data.ticket_types.begin(), data.ticket_types.end(),
                     [&](const TicketType& t) {
                       return Contains(removed_sessions, t.session_id) ||
                              (Contains(kept_sessions, t.session_id) &&
                               !Contains(kept_types, t.id));
                     }),
      data.ticket_types.end());
  data.sessions.erase(
      std::remove_if(data.sessions.begin(), data.sessions.end(),
                     [&](const EventSession& s) {
                       return Contains(removed_sessions, s.id);
                     }),
      data.sessions.end());
  data.assignments.erase(
      std::remove_if(data.assignments.begin(), data.assignments.end(),
                     [&](const CheckinAssignment& a) {
                       return Contains(removed_sessions, a.session_id);
                     }),
      data.assignments.end());
}

Status ActivityService::CreateDraft(const Actor& actor,
                                    const ActivityDraft& draft, Id* out_id) {
  if (actor.role != Role::kOrganizer) {
    return Status::Error("只有组织者可以创建活动");
  }
  Status valid = Validate(draft);
  if (!valid.ok()) return valid;
  Transaction tx(store_);
  Activity activity;
  activity.id = store_->NextId();
  activity.organizer_id = actor.user_id;
  activity.title = Trim(draft.title);
  activity.description = draft.description;
  activity.category = Trim(draft.category);
  activity.created_at = clock_->Now();
  store_->data().activities.push_back(activity);
  WriteSessions(activity.id, draft);
  audit_->Record(actor, "创建活动草稿", activity.title, activity.id, "成功");
  tx.Commit();
  if (out_id != nullptr) *out_id = activity.id;
  return Status::Ok("草稿已保存");
}

Status ActivityService::UpdateDraft(const Actor& actor, Id activity_id,
                                    const ActivityDraft& draft) {
  Activity* activity = store_->FindActivity(activity_id);
  if (activity == nullptr || !IsOrganizerOf(actor, *activity)) {
    return Status::Error("只能编辑本人的活动");
  }
  if (activity->status != ActivityStatus::kDraft) {
    return Status::Error("只有草稿可以直接编辑，已发布活动请申请修改");
  }
  Status valid = Validate(draft);
  if (!valid.ok()) return valid;
  Transaction tx(store_);
  activity->title = Trim(draft.title);
  activity->description = draft.description;
  activity->category = Trim(draft.category);
  WriteSessions(activity_id, draft);
  audit_->Record(actor, "编辑活动草稿", activity->title, activity_id, "成功");
  tx.Commit();
  return Status::Ok("草稿已保存");
}

Status ActivityService::Submit(const Actor& actor, Id activity_id) {
  Activity* activity = store_->FindActivity(activity_id);
  if (activity == nullptr || !IsOrganizerOf(actor, *activity)) {
    return Status::Error("只能提交本人的活动");
  }
  if (activity->status != ActivityStatus::kDraft) {
    return Status::Error(std::string("当前状态不能提交审核：") +
                         ActivityStatusName(activity->status));
  }
  Status valid = Validate(LoadDraft(activity_id));
  if (!valid.ok()) return valid;
  activity->status = ActivityStatus::kPending;
  store_->MarkDirty();
  audit_->Record(actor, "提交审核", activity->title, activity_id, "成功");
  return Status::Ok("已提交管理员审核");
}

Status ActivityService::RequestChange(const Actor& actor,
                                      const ChangeRequest& request,
                                      Id* out_id) {
  const Activity* activity = store_->FindActivity(request.activity_id);
  if (activity == nullptr || !IsOrganizerOf(actor, *activity)) {
    return Status::Error("只能修改本人的活动");
  }
  if (activity->status != ActivityStatus::kPublished) {
    return Status::Error("只有已发布的活动可以申请修改");
  }
  if (PendingChangeOf(activity->id) != nullptr) {
    return Status::Error("该活动已有待审核的修改申请");
  }
  const EventSession* session = store_->FindSession(request.session_id);
  if (session == nullptr || session->activity_id != activity->id ||
      session->cancelled || session->finished) {
    return Status::Error("该场次不能修改");
  }
  if (Trim(request.new_location).empty()) return Status::Error("请填写地点");
  Status times = CheckTimes("", session->reg_open_at, request.new_reg_close_at,
                            request.new_start_at, request.new_end_at,
                            request.new_checkin_start_at);
  if (!times.ok()) return times;
  bool changed = Trim(request.new_location) != session->location ||
                 request.new_start_at != session->start_at ||
                 request.new_end_at != session->end_at ||
                 request.new_reg_close_at != session->reg_close_at ||
                 request.new_checkin_start_at != session->checkin_start_at;
  for (const CapacityChange& change : request.capacities) {
    const TicketType* type = store_->FindTicketType(change.ticket_type_id);
    if (type == nullptr || type->session_id != session->id) {
      return Status::Error("票种不属于该场次");
    }
    if (change.capacity < type->capacity) {
      return Status::Error("「" + type->name + "」名额只能增加，不能减少");
    }
    if (change.capacity != type->capacity) changed = true;
  }
  if (!changed) return Status::Error("没有任何修改");

  Transaction tx(store_);
  ChangeRequest saved = request;
  saved.id = store_->NextId();
  saved.requester_id = actor.user_id;
  saved.base_version = activity->version;
  saved.status = ChangeStatus::kPending;
  saved.new_location = Trim(request.new_location);
  saved.created_at = clock_->Now();
  store_->data().change_requests.push_back(saved);
  audit_->Record(actor, "申请修改", "修改申请 #" + std::to_string(saved.id),
                 activity->id, "待审核");
  tx.Commit();
  if (out_id != nullptr) *out_id = saved.id;
  return Status::Ok("修改申请已提交，审核通过前仍显示原信息");
}

Status ActivityService::WithdrawChange(const Actor& actor, Id change_id) {
  ChangeRequest* change = store_->FindChangeRequest(change_id);
  if (change == nullptr || change->requester_id != actor.user_id) {
    return Status::Error("只能撤回本人的修改申请");
  }
  if (change->status != ChangeStatus::kPending) {
    return Status::Error("该申请已处理，不能撤回");
  }
  change->status = ChangeStatus::kWithdrawn;
  store_->MarkDirty();
  audit_->Record(actor, "撤回修改申请",
                 "修改申请 #" + std::to_string(change_id),
                 change->activity_id, "成功");
  return Status::Ok("已撤回，原配置不变");
}

Status ActivityService::CancelActivity(const Actor& actor, Id activity_id,
                                       const std::string& reason,
                                       CancelImpact* impact) {
  Activity* activity = store_->FindActivity(activity_id);
  if (activity == nullptr || !CanManage(actor, *activity)) {
    return Status::Error("没有权限取消该活动");
  }
  if (activity->status == ActivityStatus::kEnded ||
      activity->status == ActivityStatus::kCancelled) {
    return Status::Error("活动已结束或已取消");
  }
  if (Trim(reason).empty()) return Status::Error("请填写取消原因");
  Transaction tx(store_);
  activity->status = ActivityStatus::kCancelled;
  activity->cancel_reason = Trim(reason);
  for (EventSession* session : store_->SessionsOf(activity_id)) {
    session->cancelled = true;
  }
  for (ChangeRequest& change : store_->data().change_requests) {
    if (change.activity_id == activity_id &&
        change.status == ChangeStatus::kPending) {
      change.status = ChangeStatus::kWithdrawn;
    }
  }
  *impact = registrations_->CancelScope(activity_id, 0, 0);
  audit_->Record(actor, "取消活动", activity->title, activity_id,
                 "成功 · " + ImpactText(*impact));
  tx.Commit();
  return Status::Ok("活动已取消：" + ImpactText(*impact));
}

Status ActivityService::CancelSession(const Actor& actor, Id session_id,
                                      const std::string& reason,
                                      CancelImpact* impact) {
  EventSession* session = store_->FindSession(session_id);
  Activity* activity =
      session == nullptr ? nullptr : store_->FindActivity(session->activity_id);
  if (activity == nullptr || !CanManage(actor, *activity)) {
    return Status::Error("没有权限取消该场次");
  }
  if (activity->status != ActivityStatus::kPublished) {
    return Status::Error("只有已发布活动可以单独取消场次");
  }
  if (session->cancelled || session->finished) {
    return Status::Error("该场次已取消或已结束");
  }
  if (Trim(reason).empty()) return Status::Error("请填写取消原因");
  Transaction tx(store_);
  session->cancelled = true;
  *impact = registrations_->CancelScope(activity->id, session_id, 0);
  bool all_cancelled = true;
  for (const EventSession* other : store_->SessionsOf(activity->id)) {
    if (!other->cancelled) all_cancelled = false;
  }
  if (all_cancelled) {
    activity->status = ActivityStatus::kCancelled;
    activity->cancel_reason = Trim(reason);
  }
  audit_->Record(actor, "取消场次",
                 activity->title + " " + FormatShort(session->start_at),
                 activity->id, "成功 · " + ImpactText(*impact));
  tx.Commit();
  return Status::Ok("场次已取消：" + ImpactText(*impact));
}

Status ActivityService::CancelTicketType(const Actor& actor, Id ticket_type_id,
                                         const std::string& reason,
                                         CancelImpact* impact) {
  TicketType* type = store_->FindTicketType(ticket_type_id);
  EventSession* session =
      type == nullptr ? nullptr : store_->FindSession(type->session_id);
  Activity* activity =
      session == nullptr ? nullptr : store_->FindActivity(session->activity_id);
  if (activity == nullptr || !CanManage(actor, *activity)) {
    return Status::Error("没有权限取消该票种");
  }
  if (activity->status != ActivityStatus::kPublished) {
    return Status::Error("只有已发布活动可以单独取消票种");
  }
  if (type->cancelled || session->cancelled || session->finished) {
    return Status::Error("该票种已取消或场次已结束");
  }
  if (Trim(reason).empty()) return Status::Error("请填写取消原因");
  Transaction tx(store_);
  type->cancelled = true;
  *impact = registrations_->CancelScope(activity->id, session->id,
                                        ticket_type_id);
  audit_->Record(actor, "取消票种", activity->title + "「" + type->name + "」",
                 activity->id, "成功 · " + ImpactText(*impact));
  tx.Commit();
  return Status::Ok("票种已取消：" + ImpactText(*impact));
}

Status ActivityService::AssignChecker(const Actor& actor, Id session_id,
                                      Id checker_id) {
  const EventSession* session = store_->FindSession(session_id);
  const Activity* activity =
      session == nullptr ? nullptr : store_->FindActivity(session->activity_id);
  if (activity == nullptr || !CanManage(actor, *activity)) {
    return Status::Error("没有权限安排该场次的检票员");
  }
  const User* checker = store_->FindUser(checker_id);
  if (checker == nullptr || checker->role != Role::kChecker) {
    return Status::Error("只能分配检票员账号");
  }
  if (IsAssigned(session_id, checker_id)) return Status::Ok("已经分配过");
  store_->data().assignments.push_back({checker_id, session_id});
  store_->MarkDirty();
  audit_->Record(actor, "分配检票员",
                 checker->display_name + " → " + FormatShort(session->start_at),
                 activity->id, "成功");
  return Status::Ok("已授权 " + checker->display_name + " 检票");
}

Status ActivityService::UnassignChecker(const Actor& actor, Id session_id,
                                        Id checker_id) {
  const EventSession* session = store_->FindSession(session_id);
  const Activity* activity =
      session == nullptr ? nullptr : store_->FindActivity(session->activity_id);
  if (activity == nullptr || !CanManage(actor, *activity)) {
    return Status::Error("没有权限安排该场次的检票员");
  }
  std::vector<CheckinAssignment>& list = store_->data().assignments;
  const size_t before = list.size();
  list.erase(std::remove_if(list.begin(), list.end(),
                            [&](const CheckinAssignment& a) {
                              return a.session_id == session_id &&
                                     a.checker_id == checker_id;
                            }),
             list.end());
  if (list.size() == before) return Status::Ok("未分配过");
  store_->MarkDirty();
  audit_->Record(actor, "撤销检票授权", FormatShort(session->start_at),
                 activity->id, "成功");
  return Status::Ok("已撤销检票授权");
}

bool ActivityService::IsAssigned(Id session_id, Id checker_id) const {
  for (const CheckinAssignment& a : store_->data().assignments) {
    if (a.session_id == session_id && a.checker_id == checker_id) return true;
  }
  return false;
}

std::vector<const Activity*> ActivityService::ListManaged(
    const Actor& actor) const {
  std::vector<const Activity*> result;
  for (const Activity& activity : store_->data().activities) {
    if (CanManage(actor, activity)) result.push_back(&activity);
  }
  std::sort(result.begin(), result.end(),
            [](const Activity* a, const Activity* b) { return a->id > b->id; });
  return result;
}

const ChangeRequest* ActivityService::PendingChangeOf(Id activity_id) const {
  for (const ChangeRequest& change : store_->data().change_requests) {
    if (change.activity_id == activity_id &&
        change.status == ChangeStatus::kPending) {
      return &change;
    }
  }
  return nullptr;
}

}  // namespace ticketing
