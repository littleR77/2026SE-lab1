// Copyright 2026 R
// M5 报名与候补 + M6 票券。名额相关的写操作全部集中在这里（实验二设计原因 2）。
#ifndef SERVICE_REGISTRATION_SERVICE_H_
#define SERVICE_REGISTRATION_SERVICE_H_

#include <string>
#include <vector>

#include "common/status.h"
#include "common/time_util.h"
#include "data/store.h"
#include "domain/model.h"
#include "service/audit_log.h"
#include "service/notifier.h"

namespace ticketing {

struct RegisterResult {
  Id registration_id = 0;
  bool waitlisted = false;  // true 表示进入候补
  bool existing = false;    // true 表示重复提交，返回的是已有有效报名
  int position = 0;         // 候补排位（从 1 开始），非候补为 0
};

// 取消活动、场次或票种时的影响统计。
struct CancelImpact {
  int cancelled = 0;        // 报名成功 -> 已取消
  int tickets_voided = 0;   // 失效的正式票（含已签到的票）
  int waitlist_removed = 0;  // 删除的候补记录和候补凭证
};

class RegistrationService {
 public:
  RegistrationService(Store* store, const Clock* clock, AuditLog* audit,
                      Notifier* notifier)
      : store_(store), clock_(clock), audit_(audit), notifier_(notifier) {}

  // 报名（REQ-007～009、012、013）。
  // 前置条件：actor 为参与者；活动已发布，场次和票种未取消且票种属于该场次；
  //           reg_open_at <= 当前时间 < reg_close_at。
  // 结果：同一场次已有有效报名时返回该报名（existing = true），不新增记录；
  //       有名额时生成“报名成功”和正式票；无名额时进入候补，
  //       生成候补凭证并发送一次入队提醒。
  Status Register(const Actor& actor, Id session_id, Id ticket_type_id,
                  RegisterResult* out);

  // 本人退票或退出候补（REQ-011、REQ-027）。
  // 前置条件：报名属于 actor；状态为报名成功或候补中；当前时间早于检票开始。
  // 结果：报名转为已取消，票券失效；释放名额后按顺序递补同票种候补。
  //       重复取消返回原结果，不重复释放名额。
  Status Cancel(const Actor& actor, Id registration_id);

  // 在报名窗口内按候补顺序递补该票种，直到没有空余名额或没有候补。
  // 返回递补人数。窗口外不递补（REQ-010）。
  int PromoteWaiting(Id ticket_type_id);

  // 取消范围内的报名：报名成功转为已取消，正式票失效；
  // 候补记录和候补凭证直接删除，不触发递补（实验二状态图修改）。
  // session_id 和 ticket_type_id 都为 0 时表示整个活动。
  CancelImpact CancelScope(Id activity_id, Id session_id, Id ticket_type_id);

  // 报名截止：该场次仍在候补的记录转为候补失效，候补凭证失效。返回条数。
  int ExpireWaiting(Id session_id);
  // 场次结束：未签到的报名成功转为缺席，所有正式票失效。返回缺席条数。
  int FinishSession(Id session_id);

  // 本人未清理的报名，按报名时间倒序。
  std::vector<const Registration*> ListMine(const Actor& actor) const;
  // 清理本人失效记录（REQ-020）。只能清理已取消、缺席、候补失效，
  // 或所属活动已结束、取消的记录。
  Status Hide(const Actor& actor, Id registration_id);
  // 清理本人全部可清理记录，返回清理条数。
  int HideAllInvalid(const Actor& actor);
  // 该报名能否被个人清理。
  bool CanHide(const Registration& registration) const;

  // 名额统计。
  int Occupied(Id ticket_type_id) const;
  int Remaining(Id ticket_type_id) const;
  int WaitingCount(Id ticket_type_id) const;
  // 候补排位（从 1 开始）；不在候补中时返回 0。
  int WaitingPosition(Id registration_id) const;

 private:
  // 生成唯一票码，prefix 为 "TK" 或 "WL"。
  std::string NewTicketCode(const char* prefix);
  // 为报名发放票券，同时让该报名的旧票失效。
  void IssueTicket(Registration* registration, TicketKind kind);
  void VoidTicketOf(Id registration_id);
  bool InRegistrationWindow(const EventSession& session) const;

  Store* store_;
  const Clock* clock_;
  AuditLog* audit_;
  Notifier* notifier_;
  uint32_t code_seed_ = 0x2026u;
};

}  // namespace ticketing

#endif  // SERVICE_REGISTRATION_SERVICE_H_
