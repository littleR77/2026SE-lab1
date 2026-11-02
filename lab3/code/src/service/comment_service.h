// Copyright 2026 R
// M9 评论与评分：开放评论和回复、点赞、五星评分（REQ-032～040）。
#ifndef SERVICE_COMMENT_SERVICE_H_
#define SERVICE_COMMENT_SERVICE_H_

#include <string>
#include <vector>

#include "common/status.h"
#include "common/time_util.h"
#include "data/store.h"
#include "domain/model.h"
#include "service/audit_log.h"

namespace ticketing {

class CommentService {
 public:
  CommentService(Store* store, const Clock* clock, AuditLog* audit)
      : store_(store), clock_(clock), audit_(audit) {}

  // 发表评论或回复。任何能查看该活动的人都可以，包括游客。
  // 前置条件：活动可见；内容去掉首尾空白后非空；parent_id 不为 0 时
  //           被回复评论存在且属于同一活动。游客须填写昵称。
  // 结果：作者标识由系统判定（活动官方 > 已出席 > 普通用户 > 游客）。
  Status Post(const Actor& actor, Id activity_id, Id parent_id,
              const std::string& guest_name, const std::string& content,
              Id* out_id);

  // 给他人评论点赞。前置条件：已登录、评论存在且作者不是本人。
  // 重复点赞保持一次，返回成功但不再计数。
  Status Like(const Actor& actor, Id comment_id);
  int LikeCount(Id comment_id) const;
  bool HasLiked(const Actor& actor, Id comment_id) const;

  // 五星评分。前置条件：参与者已登录，活动已结束，1 <= stars <= 5。
  // 每人每活动一条，再次提交改为新分值，不重复计入平均分。
  Status Rate(const Actor& actor, Id activity_id, int stars);
  // 平均分（四舍五入保留一位小数前的原值）；没有评分时返回 false。
  bool Average(Id activity_id, double* average, int* count) const;
  // 本人对该活动的评分；没有时返回 0。
  int MyRating(const Actor& actor, Id activity_id) const;

  // 活动的顶层评论（按时间倒序）与某评论的回复（按时间升序）。
  std::vector<const Comment*> TopLevel(Id activity_id) const;
  std::vector<const Comment*> Replies(Id comment_id) const;
  int CountOf(Id activity_id) const;

  // 判定作者身份标识。
  Badge BadgeFor(const Actor& actor, const Activity& activity) const;

 private:
  Store* store_;
  const Clock* clock_;
  AuditLog* audit_;
};

}  // namespace ticketing

#endif  // SERVICE_COMMENT_SERVICE_H_
