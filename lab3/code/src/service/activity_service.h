// Copyright 2026 R
// M2 活动管理：草稿、提交审核、申请修改、取消活动，以及检票员分配（M7）。
#ifndef SERVICE_ACTIVITY_SERVICE_H_
#define SERVICE_ACTIVITY_SERVICE_H_

#include <string>
#include <vector>

#include "common/status.h"
#include "common/time_util.h"
#include "data/store.h"
#include "domain/model.h"
#include "service/audit_log.h"
#include "service/registration_service.h"

namespace ticketing {

// 编辑表单中的一个票种。id 为 0 表示新增。
struct TicketTypeDraft {
  Id id = 0;
  std::string name;
  int capacity = 0;
};

// 编辑表单中的一个场次。id 为 0 表示新增。
struct SessionDraft {
  Id id = 0;
  std::string location;
  Time start_at = 0;
  Time end_at = 0;
  Time reg_open_at = 0;
  Time reg_close_at = 0;
  Time checkin_start_at = 0;
  std::vector<TicketTypeDraft> ticket_types;
};

struct ActivityDraft {
  std::string title;
  std::string description;
  std::string category;
  std::vector<SessionDraft> sessions;
};

class ActivityService {
 public:
  ActivityService(Store* store, const Clock* clock, AuditLog* audit,
                  RegistrationService* registrations)
      : store_(store), clock_(clock), audit_(audit),
        registrations_(registrations) {}

  // 校验草稿内容（REQ-002、REQ-027）：名称、分类非空；至少一个场次；
  // 每个场次地点非空，报名开放 < 报名截止 <= 活动开始 < 活动结束，
  // 检票开始 <= 活动开始，且检票开始晚于报名开放；
  // 每个场次至少一个票种，票种名称在场次内唯一，名额为正整数。
  static Status Validate(const ActivityDraft& draft);

  // 由活动当前数据生成草稿表单。
  ActivityDraft LoadDraft(Id activity_id);

  // 创建草稿。前置条件：actor 为组织者，草稿通过校验。
  Status CreateDraft(const Actor& actor, const ActivityDraft& draft,
                     Id* out_id);
  // 保存草稿。前置条件：活动属于 actor 且为草稿状态，草稿通过校验。
  // 结果：整体替换活动的场次和票种配置。
  Status UpdateDraft(const Actor& actor, Id activity_id,
                     const ActivityDraft& draft);
  // 提交审核：草稿 -> 待审核。已发布等其他状态不能再提交（REQ-003）。
  Status Submit(const Actor& actor, Id activity_id);

  // 申请修改已发布活动的一个场次（REQ-004）。
  // 前置条件：活动属于 actor 且已发布；没有其他待审修改申请；
  //           新时间合法；各票种新名额不小于原名额，且至少有一项变化。
  // 结果：保存修改前版本号，待管理员审核；审核前仍使用原配置。
  Status RequestChange(const Actor& actor, const ChangeRequest& request,
                       Id* out_id);
  // 撤回本人待审的修改申请，原配置不变。
  Status WithdrawChange(const Actor& actor, Id change_id);

  // 取消整个活动、一个场次或一个票种（REQ-005）。
  // 前置条件：actor 能管理该活动；活动已发布
  //           （待审核或草稿时只能取消整个活动）；
  //           目标尚未取消、尚未结束；原因非空。
  // 结果：范围内报名取消、票券失效，候补删除且不递补；范围外不变。
  Status CancelActivity(const Actor& actor, Id activity_id,
                        const std::string& reason, CancelImpact* impact);
  Status CancelSession(const Actor& actor, Id session_id,
                       const std::string& reason, CancelImpact* impact);
  Status CancelTicketType(const Actor& actor, Id ticket_type_id,
                          const std::string& reason, CancelImpact* impact);

  // 分配或撤销检票员（M7）。前置条件：actor 能管理该场次所属活动，
  // checker_id 为检票员角色。发布活动不会自动授予检票权限。
  Status AssignChecker(const Actor& actor, Id session_id, Id checker_id);
  Status UnassignChecker(const Actor& actor, Id session_id, Id checker_id);
  bool IsAssigned(Id session_id, Id checker_id) const;

  // 组织者本人的全部活动（管理员为全部活动），按编号倒序。
  std::vector<const Activity*> ListManaged(const Actor& actor) const;
  // 活动当前待审的修改申请；没有时返回 nullptr。
  const ChangeRequest* PendingChangeOf(Id activity_id) const;

 private:
  // 把草稿中的场次和票种写入数据表（用于创建和保存草稿）。
  void WriteSessions(Id activity_id, const ActivityDraft& draft);

  Store* store_;
  const Clock* clock_;
  AuditLog* audit_;
  RegistrationService* registrations_;
};

}  // namespace ticketing

#endif  // SERVICE_ACTIVITY_SERVICE_H_
