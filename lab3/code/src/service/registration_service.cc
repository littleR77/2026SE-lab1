// Copyright 2026 R
#include "service/registration_service.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace ticketing {
namespace {

// 定时调度、自动递补等系统操作在日志中记为“系统”。
const Actor kSystemActor{0, Role::kAdmin};

std::string RegLabel(Id id) { return "报名 R-" + std::to_string(id); }

}  // namespace

bool RegistrationService::InRegistrationWindow(
    const EventSession& session) const {
  const Time now = clock_->Now();
  return now >= session.reg_open_at && now < session.reg_close_at;
}

std::string RegistrationService::NewTicketCode(const char* prefix) {
  while (true) {
    // xorshift32 伪随机数，再混入编号和时间，最后检查唯一性。
    code_seed_ ^= code_seed_ << 13;
    code_seed_ ^= code_seed_ >> 17;
    code_seed_ ^= code_seed_ << 5;
    const uint32_t mixed =
        code_seed_ ^ static_cast<uint32_t>(store_->data().next_id * 2654435761u)
        ^ static_cast<uint32_t>(clock_->Now());
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%s-%04X-%04X", prefix,
             static_cast<unsigned>(mixed >> 16),
             static_cast<unsigned>(mixed & 0xFFFFu));
    if (store_->FindTicketByCode(buffer) == nullptr) return buffer;
  }
}

void RegistrationService::VoidTicketOf(Id registration_id) {
  for (Ticket& ticket : store_->data().tickets) {
    if (ticket.registration_id == registration_id) ticket.valid = false;
  }
}

void RegistrationService::IssueTicket(Registration* registration,
                                      TicketKind kind) {
  // 同一报名不能同时拥有多张可用票：先让旧票（如候补凭证）失效。
  VoidTicketOf(registration->id);
  Ticket ticket;
  ticket.code = NewTicketCode(kind == TicketKind::kFormal ? "TK" : "WL");
  ticket.registration_id = registration->id;
  ticket.kind = kind;
  ticket.valid = true;
  store_->data().tickets.push_back(ticket);
}

Status RegistrationService::Register(const Actor& actor, Id session_id,
                                     Id ticket_type_id, RegisterResult* out) {
  *out = RegisterResult();
  if (actor.role != Role::kParticipant) {
    return Status::Error("请先以参与者身份登录后再报名");
  }
  const EventSession* session = store_->FindSession(session_id);
  const TicketType* type = store_->FindTicketType(ticket_type_id);
  if (session == nullptr || type == nullptr || type->session_id != session_id) {
    return Status::Error("场次或票种不存在");
  }
  const Activity* activity = store_->FindActivity(session->activity_id);
  if (activity == nullptr || activity->status != ActivityStatus::kPublished) {
    return Status::Error("该活动当前不接受报名");
  }
  if (session->cancelled || type->cancelled) {
    return Status::Error("该场次或票种已取消");
  }
  if (clock_->Now() < session->reg_open_at) {
    return Status::Error("报名尚未开放");
  }
  if (clock_->Now() >= session->reg_close_at) {
    return Status::Error("报名已截止");
  }

  // 每人每场次仅一份有效报名：重复提交返回已有报名（REQ-008）。
  for (const Registration& reg : store_->data().registrations) {
    if (reg.user_id == actor.user_id && reg.session_id == session_id &&
        IsActiveRegistration(reg.status)) {
      out->registration_id = reg.id;
      out->existing = true;
      out->waitlisted = reg.status == RegStatus::kWaiting;
      out->position = WaitingPosition(reg.id);
      return Status::Ok("你已报名该场次，返回已有报名");
    }
  }

  // 名额检查和写入在同一事务中完成，单线程顺序执行，不会超额（REQ-012）。
  Transaction tx(store_);
  Registration reg;
  reg.id = store_->NextId();
  reg.user_id = actor.user_id;
  reg.session_id = session_id;
  reg.ticket_type_id = ticket_type_id;
  reg.created_at = clock_->Now();
  reg.updated_at = reg.created_at;
  const bool has_seat = Remaining(ticket_type_id) > 0;
  if (has_seat) {
    reg.status = RegStatus::kConfirmed;
  } else {
    reg.status = RegStatus::kWaiting;
    reg.queue_seq = store_->NextQueueSeq();
  }
  store_->data().registrations.push_back(reg);
  Registration* saved = store_->FindRegistration(reg.id);
  IssueTicket(saved, has_seat ? TicketKind::kFormal : TicketKind::kWaitlist);

  out->registration_id = reg.id;
  out->waitlisted = !has_seat;
  out->position = has_seat ? 0 : WaitingPosition(reg.id);
  if (!has_seat) {
    // 只有成功进入候补并取得候补凭证后才发送提醒（REQ-033、REQ-035）。
    const Ticket* voucher = store_->FindTicketOf(reg.id);
    notifier_->Notify(
        actor.user_id, reg.id, "候补提醒",
        "你已进入《" + activity->title + "》" + FormatShort(session->start_at) +
            " 场「" + type->name + "」的候补队列，当前第 " +
            std::to_string(out->position) + " 位。候补凭证 " + voucher->code +
            " 不能用于入场；有人退票或扩容时将按顺序自动递补。候补不代表已获得"
            "入场名额。",
        "waitlist:" + std::to_string(reg.id));
  }
  audit_->Record(actor, "报名", RegLabel(reg.id), activity->id,
                 has_seat ? "报名成功" : "进入候补");
  tx.Commit();
  return Status::Ok(has_seat ? "报名成功，已生成正式票"
                             : "名额已满，已进入候补，第 " +
                                   std::to_string(out->position) + " 位");
}

Status RegistrationService::Cancel(const Actor& actor, Id registration_id) {
  Registration* reg = store_->FindRegistration(registration_id);
  if (reg == nullptr || actor.user_id == 0 || reg->user_id != actor.user_id) {
    return Status::Error("只能取消本人的报名");
  }
  if (reg->status == RegStatus::kCancelled) {
    return Status::Ok("该报名已取消");
  }
  if (reg->status != RegStatus::kConfirmed &&
      reg->status != RegStatus::kWaiting) {
    return Status::Error(std::string("当前状态不能退票：") +
                         RegStatusName(reg->status));
  }
  const EventSession* session = store_->FindSession(reg->session_id);
  if (clock_->Now() >= session->checkin_start_at) {
    return Status::Error("检票已开始，不能再退票");
  }

  Transaction tx(store_);
  const bool was_confirmed = reg->status == RegStatus::kConfirmed;
  reg->status = RegStatus::kCancelled;
  reg->updated_at = clock_->Now();
  VoidTicketOf(reg->id);
  audit_->Record(actor, was_confirmed ? "退票" : "退出候补", RegLabel(reg->id),
                 session->activity_id, "成功");
  int promoted = 0;
  if (was_confirmed) promoted = PromoteWaiting(reg->ticket_type_id);
  tx.Commit();
  if (!was_confirmed) return Status::Ok("已退出候补");
  return Status::Ok(promoted > 0 ? "退票成功，名额已递补给候补者"
                                 : "退票成功，票码已失效");
}

int RegistrationService::PromoteWaiting(Id ticket_type_id) {
  const TicketType* type = store_->FindTicketType(ticket_type_id);
  if (type == nullptr || type->cancelled) return 0;
  const EventSession* session = store_->FindSession(type->session_id);
  const Activity* activity = store_->FindActivity(session->activity_id);
  if (session->cancelled || !InRegistrationWindow(*session) ||
      activity->status != ActivityStatus::kPublished) {
    return 0;
  }
  int promoted = 0;
  while (Remaining(ticket_type_id) > 0) {
    // 找到同票种中排队序号最小的候补。
    Registration* first = nullptr;
    for (Registration& reg : store_->data().registrations) {
      if (reg.ticket_type_id == ticket_type_id &&
          reg.status == RegStatus::kWaiting &&
          (first == nullptr || reg.queue_seq < first->queue_seq)) {
        first = &reg;
      }
    }
    if (first == nullptr) break;
    first->status = RegStatus::kConfirmed;
    first->updated_at = clock_->Now();
    IssueTicket(first, TicketKind::kFormal);
    const Ticket* ticket = store_->FindTicketOf(first->id);
    notifier_->Notify(first->user_id, first->id, "递补成功",
                      "《" + activity->title + "》" +
                          FormatShort(session->start_at) + " 场「" +
                          type->name + "」已为你递补为正式报名，票码 " +
                          ticket->code + "，可凭此票入场。",
                      "promote:" + std::to_string(first->id));
    audit_->Record(kSystemActor, "候补递补", RegLabel(first->id),
                   activity->id, "成功");
    ++promoted;
  }
  if (promoted > 0) store_->MarkDirty();
  return promoted;
}

CancelImpact RegistrationService::CancelScope(Id activity_id, Id session_id,
                                              Id ticket_type_id) {
  CancelImpact impact;
  const Activity* activity = store_->FindActivity(activity_id);
  std::vector<Id> removed;
  for (Registration& reg : store_->data().registrations) {
    const EventSession* session = store_->FindSession(reg.session_id);
    if (session->activity_id != activity_id) continue;
    if (session_id != 0 && reg.session_id != session_id) continue;
    if (ticket_type_id != 0 && reg.ticket_type_id != ticket_type_id) continue;
    if (!IsActiveRegistration(reg.status)) continue;
    notifier_->Notify(reg.user_id, 0, "取消通知",
                      "你报名的《" + activity->title + "》" +
                          FormatShort(session->start_at) +
                          " 场已被取消，相关票券已失效。",
                      "cancel:" + std::to_string(reg.id));
    if (reg.status == RegStatus::kWaiting) {
      // 候补记录和候补凭证直接删除，不触发递补。
      removed.push_back(reg.id);
      continue;
    }
    if (reg.status == RegStatus::kConfirmed) {
      reg.status = RegStatus::kCancelled;
      reg.updated_at = clock_->Now();
      ++impact.cancelled;
    }
    // 已签到的记录保留签到历史，只让票失效。
    VoidTicketOf(reg.id);
    ++impact.tickets_voided;
  }
  auto is_removed = [&removed](Id id) {
    return std::find(removed.begin(), removed.end(), id) != removed.end();
  };
  std::vector<Registration>& regs = store_->data().registrations;
  regs.erase(std::remove_if(regs.begin(), regs.end(),
                            [&](const Registration& r) {
                              return is_removed(r.id);
                            }),
             regs.end());
  std::vector<Ticket>& tickets = store_->data().tickets;
  tickets.erase(std::remove_if(tickets.begin(), tickets.end(),
                               [&](const Ticket& t) {
                                 return is_removed(t.registration_id);
                               }),
                tickets.end());
  impact.waitlist_removed = static_cast<int>(removed.size());
  store_->MarkDirty();
  return impact;
}

int RegistrationService::ExpireWaiting(Id session_id) {
  int count = 0;
  for (Registration& reg : store_->data().registrations) {
    if (reg.session_id != session_id || reg.status != RegStatus::kWaiting) {
      continue;
    }
    reg.status = RegStatus::kExpired;
    reg.updated_at = clock_->Now();
    VoidTicketOf(reg.id);
    ++count;
  }
  if (count > 0) store_->MarkDirty();
  return count;
}

int RegistrationService::FinishSession(Id session_id) {
  ExpireWaiting(session_id);
  int absent = 0;
  for (Registration& reg : store_->data().registrations) {
    if (reg.session_id != session_id) continue;
    if (reg.status == RegStatus::kConfirmed) {
      reg.status = RegStatus::kAbsent;
      reg.updated_at = clock_->Now();
      ++absent;
    }
    // 场次结束后所有入场票都不能再检票。
    VoidTicketOf(reg.id);
  }
  store_->MarkDirty();
  return absent;
}

std::vector<const Registration*> RegistrationService::ListMine(
    const Actor& actor) const {
  std::vector<const Registration*> result;
  if (actor.user_id == 0) return result;
  for (const Registration& reg : store_->data().registrations) {
    if (reg.user_id == actor.user_id && !reg.hidden_by_owner) {
      result.push_back(&reg);
    }
  }
  std::sort(result.begin(), result.end(),
            [](const Registration* a, const Registration* b) {
              return a->id > b->id;
            });
  return result;
}

bool RegistrationService::CanHide(const Registration& registration) const {
  if (registration.status == RegStatus::kCancelled ||
      registration.status == RegStatus::kAbsent ||
      registration.status == RegStatus::kExpired) {
    return true;
  }
  const EventSession* session = store_->FindSession(registration.session_id);
  const Activity* activity = store_->FindActivity(session->activity_id);
  return session->finished || session->cancelled ||
         activity->status == ActivityStatus::kEnded ||
         activity->status == ActivityStatus::kCancelled;
}

Status RegistrationService::Hide(const Actor& actor, Id registration_id) {
  Registration* reg = store_->FindRegistration(registration_id);
  if (reg == nullptr || actor.user_id == 0 || reg->user_id != actor.user_id) {
    return Status::Error("只能清理本人的记录");
  }
  if (!CanHide(*reg)) return Status::Error("正在生效的报名和票券不能清理");
  if (reg->hidden_by_owner) return Status::Ok("该记录已清理");
  reg->hidden_by_owner = true;
  store_->MarkDirty();
  const EventSession* session = store_->FindSession(reg->session_id);
  audit_->Record(actor, "清理票券记录", RegLabel(reg->id),
                 session->activity_id, "成功");
  return Status::Ok("已从列表中清理，后台历史不受影响");
}

int RegistrationService::HideAllInvalid(const Actor& actor) {
  int count = 0;
  for (const Registration* reg : ListMine(actor)) {
    if (CanHide(*reg) && Hide(actor, reg->id).ok()) ++count;
  }
  return count;
}

int RegistrationService::Occupied(Id ticket_type_id) const {
  int count = 0;
  for (const Registration& reg : store_->data().registrations) {
    if (reg.ticket_type_id == ticket_type_id && OccupiesSeat(reg.status)) {
      ++count;
    }
  }
  return count;
}

int RegistrationService::Remaining(Id ticket_type_id) const {
  const TicketType* type = store_->FindTicketType(ticket_type_id);
  if (type == nullptr) return 0;
  return std::max(0, type->capacity - Occupied(ticket_type_id));
}

int RegistrationService::WaitingCount(Id ticket_type_id) const {
  int count = 0;
  for (const Registration& reg : store_->data().registrations) {
    if (reg.ticket_type_id == ticket_type_id &&
        reg.status == RegStatus::kWaiting) {
      ++count;
    }
  }
  return count;
}

int RegistrationService::WaitingPosition(Id registration_id) const {
  const Registration* target = store_->FindRegistration(registration_id);
  if (target == nullptr || target->status != RegStatus::kWaiting) return 0;
  int position = 0;
  for (const Registration& reg : store_->data().registrations) {
    if (reg.ticket_type_id == target->ticket_type_id &&
        reg.status == RegStatus::kWaiting &&
        reg.queue_seq <= target->queue_seq) {
      ++position;
    }
  }
  return position;
}

}  // namespace ticketing
