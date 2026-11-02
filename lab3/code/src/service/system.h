// Copyright 2026 R
// 把数据层和各业务模块组装在一起，供界面层和测试使用。
#ifndef SERVICE_SYSTEM_H_
#define SERVICE_SYSTEM_H_

#include <string>

#include "common/status.h"
#include "common/time_util.h"
#include "data/store.h"
#include "service/account_service.h"
#include "service/activity_service.h"
#include "service/audit_log.h"
#include "service/checkin_service.h"
#include "service/comment_service.h"
#include "service/favorite_service.h"
#include "service/notifier.h"
#include "service/query_service.h"
#include "service/registration_service.h"
#include "service/review_service.h"
#include "service/scheduler.h"
#include "service/stats_service.h"

namespace ticketing {

class System {
 public:
  // now 为模拟时钟的初始时间。
  explicit System(Time now);
  System(const System&) = delete;
  System& operator=(const System&) = delete;

  // 打开数据文件：存在则读取（并恢复保存时的时钟），否则写入演示数据。
  Status Open(const std::string& path);
  // 有变化时保存到 Open 时的文件。
  Status Persist();
  // 推进模拟时钟并运行定时调度。
  SchedulerReport AdvanceClock(Time minutes);

  // 写入演示数据：账号、各种状态的活动、报名、评论和评分。
  void Seed();

  Store store;
  Clock clock;
  AuditLog audit;
  Notifier notifier;
  AccountService accounts;
  RegistrationService registrations;
  ActivityService activities;
  ReviewService reviews;
  QueryService queries;
  CheckinService checkins;
  CommentService comments;
  FavoriteService favorites;
  StatsService stats;
  Scheduler scheduler;

 private:
  std::string path_;
};

}  // namespace ticketing

#endif  // SERVICE_SYSTEM_H_
