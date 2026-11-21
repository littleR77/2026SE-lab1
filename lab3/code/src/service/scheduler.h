// Copyright 2026 R
// S1 定时调度：按时间触发候补失效、缺席标记和活动结束。
// 只调用业务模块的接口，不直接修改报名数据（实验二设计原因 6）。
#ifndef SERVICE_SCHEDULER_H_
#define SERVICE_SCHEDULER_H_

#include "common/time_util.h"
#include "data/store.h"
#include "service/audit_log.h"
#include "service/registration_service.h"
#include "service/review_service.h"

namespace ticketing {

struct SchedulerReport {
  int expired = 0;   // 候补失效条数
  int absent = 0;    // 缺席条数
  int ended = 0;     // 转为已结束的活动数
  int changes_expired = 0;  // 转为已失效的修改申请数
};

class Scheduler {
 public:
  Scheduler(Store* store, const Clock* clock, AuditLog* audit,
            RegistrationService* registrations, ReviewService* reviews)
      : store_(store), clock_(clock), audit_(audit),
        registrations_(registrations), reviews_(reviews) {}

  // 处理所有已到期的事件。可以重复调用，已处理的不会再处理。
  // 1. 报名截止的场次：候补转为候补失效；
  // 2. 已结束且未处理的场次：未签到转为缺席，票券失效；
  // 3. 已发布活动的全部场次都已结束（或取消）时，活动转为已结束；
  // 4. 已经无法通过的待审核修改申请转为已失效，不再占用审核队列。
  SchedulerReport RunDue();

 private:
  Store* store_;
  const Clock* clock_;
  AuditLog* audit_;
  RegistrationService* registrations_;
  ReviewService* reviews_;
};

}  // namespace ticketing

#endif  // SERVICE_SCHEDULER_H_
