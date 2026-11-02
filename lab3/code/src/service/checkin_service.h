// Copyright 2026 R
// M7 检票签到：授权检票员在检票窗口内核销正式票（REQ-014、REQ-015）。
#ifndef SERVICE_CHECKIN_SERVICE_H_
#define SERVICE_CHECKIN_SERVICE_H_

#include <string>
#include <vector>

#include "common/time_util.h"
#include "data/store.h"
#include "domain/model.h"
#include "service/audit_log.h"

namespace ticketing {

enum class CheckinOutcome { kPassed, kRepeated, kRejected };

struct CheckinResult {
  CheckinOutcome outcome = CheckinOutcome::kRejected;
  std::string message;
  std::string holder;   // 持票人姓名
  std::string ticket_type;
  Time checked_at = 0;  // 通过或重复时为首次签到时间
};

class CheckinService {
 public:
  CheckinService(Store* store, const Clock* clock, AuditLog* audit)
      : store_(store), clock_(clock), audit_(audit) {}

  // 核验票码。
  // 前置条件：actor 为被授权该场次的检票员；当前时间在
  //           [检票开始, 场次结束) 内；场次未取消。
  // 结果：有效正式票首次核验通过，报名转为已签到并记录核销时间和检票员；
  //       已签到的票返回首次结果，不新增记录（kRepeated）；
  //       不存在、已失效、候补凭证、非本场次的票码均拒绝。
  CheckinResult Verify(const Actor& actor, Id session_id,
                       const std::string& code);

  // actor 被授权的场次（未取消），按开始时间升序。
  std::vector<const EventSession*> AssignedSessions(const Actor& actor) const;
  int CheckedInCount(Id session_id) const;
  // 应到人数：报名成功、已签到和缺席之和。
  int ExpectedCount(Id session_id) const;

 private:
  Store* store_;
  const Clock* clock_;
  AuditLog* audit_;
};

}  // namespace ticketing

#endif  // SERVICE_CHECKIN_SERVICE_H_
