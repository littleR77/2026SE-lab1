// Copyright 2026 R
// M10 收藏与个人中心（REQ-039、REQ-041）。
#ifndef SERVICE_FAVORITE_SERVICE_H_
#define SERVICE_FAVORITE_SERVICE_H_

#include <vector>

#include "common/status.h"
#include "common/time_util.h"
#include "data/store.h"
#include "domain/model.h"
#include "service/audit_log.h"

namespace ticketing {

class FavoriteService {
 public:
  FavoriteService(Store* store, const Clock* clock, AuditLog* audit)
      : store_(store), clock_(clock), audit_(audit) {}

  // 收藏。前置条件：参与者已登录，活动存在且可见。重复收藏保持一条。
  Status Add(const Actor& actor, Id activity_id);
  // 取消收藏。只能取消本人收藏；重复取消返回成功但不再记日志。
  Status Remove(const Actor& actor, Id activity_id);
  bool IsFavorite(const Actor& actor, Id activity_id) const;
  // 本人收藏，按收藏时间倒序；活动结束或取消后仍保留。
  std::vector<const Favorite*> ListMine(const Actor& actor) const;

 private:
  Store* store_;
  const Clock* clock_;
  AuditLog* audit_;
};

}  // namespace ticketing

#endif  // SERVICE_FAVORITE_SERVICE_H_
