// Copyright 2026 R
// M3 活动审核：审核申办和修改申请（REQ-003、REQ-004）。
#ifndef SERVICE_REVIEW_SERVICE_H_
#define SERVICE_REVIEW_SERVICE_H_

#include <string>
#include <vector>

#include "common/status.h"
#include "common/time_util.h"
#include "data/store.h"
#include "domain/model.h"
#include "service/audit_log.h"
#include "service/registration_service.h"

namespace ticketing {

class ReviewService {
 public:
  ReviewService(Store* store, const Clock* clock, AuditLog* audit,
                RegistrationService* registrations)
      : store_(store), clock_(clock), audit_(audit),
        registrations_(registrations) {}

  // 通过申办：待审核 -> 已发布。
  // 前置条件：actor 为管理员；活动仍为待审核且版本号等于 expected_version
  //           （防止审核期间内容已变化）；所有场次的报名截止晚于当前时间。
  Status Approve(const Actor& actor, Id activity_id, int expected_version);
  // 驳回申办：待审核 -> 草稿，并保存原因。原因不能为空。
  Status Reject(const Actor& actor, Id activity_id, const std::string& reason);

  // 通过修改申请：核对基准版本，再次检查名额只增不减和时间规则，
  // 一次性应用全部修改，版本号加一；扩容后按顺序递补候补者。
  Status ApproveChange(const Actor& actor, Id change_id, int* promoted);
  // 驳回修改申请，原配置不变。原因不能为空。
  Status RejectChange(const Actor& actor, Id change_id,
                      const std::string& reason);

  // 修改申请为什么已经无法通过；还能审核时返回空串。
  // 场次已开始、结束或取消，活动不再是已发布，或活动版本已变化时无法通过。
  std::string ChangeBlocker(const ChangeRequest& change) const;
  // 管理员关闭一条无法通过的修改申请：待审核 -> 已失效，原因记为阻碍原因。
  // 前置条件：actor 为管理员；申请仍待审核且 ChangeBlocker 非空。
  Status ExpireChange(const Actor& actor, Id change_id);
  // 由定时调度调用：把所有无法通过的待审核申请标为已失效，返回条数。
  int ExpireStaleChanges();

  // 待审核的活动和修改申请，按提交时间升序。
  std::vector<const Activity*> PendingActivities(const Actor& actor) const;
  std::vector<const ChangeRequest*> PendingChanges(const Actor& actor) const;
  // 预估通过修改申请后会递补的人数（用于审核台提示）。
  int EstimatePromotion(const ChangeRequest& request) const;

 private:
  Store* store_;
  const Clock* clock_;
  AuditLog* audit_;
  RegistrationService* registrations_;
};

}  // namespace ticketing

#endif  // SERVICE_REVIEW_SERVICE_H_
