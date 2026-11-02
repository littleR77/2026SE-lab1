// Copyright 2026 R
// M12 操作日志：记录关键写操作，并按权限查询（REQ-019）。
#ifndef SERVICE_AUDIT_LOG_H_
#define SERVICE_AUDIT_LOG_H_

#include <string>
#include <vector>

#include "common/status.h"
#include "common/time_util.h"
#include "data/store.h"
#include "domain/model.h"

namespace ticketing {

struct LogFilter {
  Id actor_id = 0;       // 0 表示不限
  std::string action;    // 空表示不限
  std::string keyword;   // 在对象和活动名称中查找
};

class AuditLog {
 public:
  AuditLog(Store* store, const Clock* clock) : store_(store), clock_(clock) {}

  // 记录一条已经生效的操作。actor.user_id 为 0 时记为“游客/系统”。
  // 规约：只应在操作成功后调用；失败或重复未生效的请求不调用。
  void Record(const Actor& actor, const std::string& action,
              const std::string& target, Id activity_id,
              const std::string& result);

  // 查询日志，按时间倒序。
  // 前置条件：管理员可查全部；组织者只能查本人活动；其他角色返回错误。
  // 结果写入 *out，返回是否有权限。
  Status Query(const Actor& actor, const LogFilter& filter,
               std::vector<const OperationLog*>* out) const;

 private:
  Store* store_;
  const Clock* clock_;
};

}  // namespace ticketing

#endif  // SERVICE_AUDIT_LOG_H_
