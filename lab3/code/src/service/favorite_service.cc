// Copyright 2026 R
#include "service/favorite_service.h"

#include <algorithm>
#include <vector>

#include "service/account_service.h"

namespace ticketing {

Status FavoriteService::Add(const Actor& actor, Id activity_id) {
  if (actor.role != Role::kParticipant) {
    return Status::Error("请以参与者身份登录后收藏");
  }
  const Activity* activity = store_->FindActivity(activity_id);
  if (activity == nullptr || !CanView(actor, *activity)) {
    return Status::Error("活动不存在或不可见");
  }
  if (IsFavorite(actor, activity_id)) return Status::Ok("已在收藏中");
  store_->data().favorites.push_back(
      {actor.user_id, activity_id, clock_->Now()});
  store_->MarkDirty();
  audit_->Record(actor, "收藏", activity->title, activity_id, "成功");
  return Status::Ok("已收藏");
}

Status FavoriteService::Remove(const Actor& actor, Id activity_id) {
  if (actor.user_id == 0) return Status::Error("请先登录");
  std::vector<Favorite>& list = store_->data().favorites;
  auto it = std::find_if(list.begin(), list.end(), [&](const Favorite& f) {
    return f.user_id == actor.user_id && f.activity_id == activity_id;
  });
  if (it == list.end()) return Status::Ok("未收藏该活动");
  list.erase(it);
  store_->MarkDirty();
  const Activity* activity = store_->FindActivity(activity_id);
  audit_->Record(actor, "取消收藏", activity ? activity->title : "",
                 activity_id, "成功");
  return Status::Ok("已取消收藏");
}

bool FavoriteService::IsFavorite(const Actor& actor, Id activity_id) const {
  for (const Favorite& f : store_->data().favorites) {
    if (f.user_id == actor.user_id && f.activity_id == activity_id) {
      return true;
    }
  }
  return false;
}

std::vector<const Favorite*> FavoriteService::ListMine(
    const Actor& actor) const {
  std::vector<const Favorite*> result;
  if (actor.user_id == 0) return result;
  for (const Favorite& f : store_->data().favorites) {
    if (f.user_id == actor.user_id) result.push_back(&f);
  }
  // 后收藏的在前；同一时间按添加顺序倒序。
  std::reverse(result.begin(), result.end());
  std::stable_sort(result.begin(), result.end(),
                   [](const Favorite* a, const Favorite* b) {
                     return a->created_at > b->created_at;
                   });
  return result;
}

}  // namespace ticketing
