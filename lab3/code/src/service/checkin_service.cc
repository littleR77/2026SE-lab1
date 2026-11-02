// Copyright 2026 R
#include "service/checkin_service.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ticketing {
namespace {

CheckinResult Reject(const std::string& message) {
  CheckinResult result;
  result.outcome = CheckinOutcome::kRejected;
  result.message = message;
  return result;
}

std::string Normalize(const std::string& code) {
  // 去掉空格并转为大写，方便手工输入。
  std::string out;
  for (const char c : code) {
    if (c == ' ' || c == '\t') continue;
    out += (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
  }
  return out;
}

}  // namespace

CheckinResult CheckinService::Verify(const Actor& actor, Id session_id,
                                     const std::string& code) {
  if (actor.role != Role::kChecker) return Reject("只有检票员可以检票");
  bool assigned = false;
  for (const CheckinAssignment& a : store_->data().assignments) {
    if (a.session_id == session_id && a.checker_id == actor.user_id) {
      assigned = true;
    }
  }
  if (!assigned) return Reject("你没有该场次的检票授权");
  const EventSession* session = store_->FindSession(session_id);
  if (session == nullptr || session->cancelled) return Reject("该场次已取消");
  const Time now = clock_->Now();
  if (now < session->checkin_start_at) return Reject("检票尚未开始");
  if (now >= session->end_at) return Reject("场次已结束，不能检票");

  const std::string normalized = Normalize(code);
  Ticket* ticket = store_->FindTicketByCode(normalized);
  if (ticket == nullptr) return Reject("票码不存在");
  Registration* reg = store_->FindRegistration(ticket->registration_id);
  if (reg == nullptr || reg->session_id != session_id) {
    return Reject("该票不属于本场次");
  }
  if (ticket->kind == TicketKind::kWaitlist) {
    return Reject("候补凭证不能入场");
  }
  CheckinResult result;
  if (const User* holder = store_->FindUser(reg->user_id)) {
    result.holder = holder->display_name;
  }
  if (const TicketType* type = store_->FindTicketType(reg->ticket_type_id)) {
    result.ticket_type = type->name;
  }
  // 失效票即使有过签到也按失效处理，不因历史签到而放行（REQ-015）。
  if (!ticket->valid) {
    CheckinResult rejected = Reject("票码已失效");
    rejected.holder = result.holder;
    return rejected;
  }
  // 已签到的票返回首次结果，不新增核销记录。
  if (const Checkin* first = store_->FindCheckin(normalized)) {
    result.outcome = CheckinOutcome::kRepeated;
    result.checked_at = first->checked_at;
    result.message = "该票已签到，首次签到 " + FormatShort(first->checked_at);
    audit_->Record(actor, "检票", "票 " + normalized, session->activity_id,
                   "重复，返回首次结果");
    return result;
  }
  if (reg->status != RegStatus::kConfirmed) return Reject("票码已失效");

  Transaction tx(store_);
  reg->status = RegStatus::kCheckedIn;
  reg->updated_at = now;
  store_->data().checkins.push_back({normalized, actor.user_id, now});
  audit_->Record(actor, "检票", "票 " + normalized, session->activity_id,
                 "通过");
  tx.Commit();
  result.outcome = CheckinOutcome::kPassed;
  result.checked_at = now;
  result.message = "核验通过";
  return result;
}

std::vector<const EventSession*> CheckinService::AssignedSessions(
    const Actor& actor) const {
  std::vector<const EventSession*> result;
  for (const CheckinAssignment& a : store_->data().assignments) {
    if (a.checker_id != actor.user_id) continue;
    const EventSession* session = store_->FindSession(a.session_id);
    if (session != nullptr && !session->cancelled) result.push_back(session);
  }
  std::sort(result.begin(), result.end(),
            [](const EventSession* a, const EventSession* b) {
              return a->start_at < b->start_at;
            });
  return result;
}

int CheckinService::CheckedInCount(Id session_id) const {
  int count = 0;
  for (const Registration& reg : store_->data().registrations) {
    if (reg.session_id == session_id && reg.status == RegStatus::kCheckedIn) {
      ++count;
    }
  }
  return count;
}

int CheckinService::ExpectedCount(Id session_id) const {
  int count = 0;
  for (const Registration& reg : store_->data().registrations) {
    if (reg.session_id == session_id && OccupiesSeat(reg.status)) ++count;
  }
  return count;
}

}  // namespace ticketing
