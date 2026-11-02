// Copyright 2026 R
#include "service/comment_service.h"

#include <algorithm>
#include <string>
#include <vector>

#include "service/account_service.h"

namespace ticketing {
namespace {

std::string Trim(const std::string& text) {
  const size_t begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return "";
  const size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

}  // namespace

Badge CommentService::BadgeFor(const Actor& actor,
                               const Activity& activity) const {
  if (actor.user_id == 0) return Badge::kGuest;
  if (IsOrganizerOf(actor, activity)) return Badge::kOfficial;
  // “已出席”依据真实签到记录判定，作者不能自行指定（REQ-040）。
  for (const Registration& reg : store_->data().registrations) {
    if (reg.user_id != actor.user_id || reg.status != RegStatus::kCheckedIn) {
      continue;
    }
    const EventSession* session = store_->FindSession(reg.session_id);
    if (session != nullptr && session->activity_id == activity.id) {
      return Badge::kAttended;
    }
  }
  return Badge::kMember;
}

Status CommentService::Post(const Actor& actor, Id activity_id, Id parent_id,
                            const std::string& guest_name,
                            const std::string& content, Id* out_id) {
  const Activity* activity = store_->FindActivity(activity_id);
  if (activity == nullptr || !CanView(actor, *activity)) {
    return Status::Error("活动不存在或不可见");
  }
  const std::string text = Trim(content);
  if (text.empty()) return Status::Error("评论内容不能为空");
  if (parent_id != 0) {
    const Comment* parent = store_->FindComment(parent_id);
    if (parent == nullptr || parent->activity_id != activity_id) {
      return Status::Error("被回复的评论不存在");
    }
  }
  Comment comment;
  if (actor.user_id == 0) {
    comment.guest_name = Trim(guest_name);
    if (comment.guest_name.empty()) return Status::Error("游客请先填写昵称");
  }
  comment.id = store_->NextId();
  comment.activity_id = activity_id;
  comment.parent_id = parent_id;
  comment.user_id = actor.user_id;
  comment.content = text;
  comment.badge = BadgeFor(actor, *activity);
  comment.created_at = clock_->Now();
  store_->data().comments.push_back(comment);
  store_->MarkDirty();
  audit_->Record(actor, parent_id == 0 ? "评论" : "回复",
                 "评论 #" + std::to_string(comment.id), activity_id, "成功");
  if (out_id != nullptr) *out_id = comment.id;
  return Status::Ok(parent_id == 0 ? "评论已发表" : "回复已发表");
}

Status CommentService::Like(const Actor& actor, Id comment_id) {
  if (actor.role != Role::kParticipant) {
    return Status::Error("请以参与者身份登录后点赞");
  }
  const Comment* comment = store_->FindComment(comment_id);
  if (comment == nullptr) return Status::Error("评论不存在");
  if (comment->user_id == actor.user_id) {
    return Status::Error("不能给自己的评论点赞");
  }
  if (HasLiked(actor, comment_id)) return Status::Ok("已经点过赞了");
  store_->data().likes.push_back({comment_id, actor.user_id});
  store_->MarkDirty();
  audit_->Record(actor, "点赞", "评论 #" + std::to_string(comment_id),
                 comment->activity_id, "成功");
  return Status::Ok("点赞成功");
}

int CommentService::LikeCount(Id comment_id) const {
  int count = 0;
  for (const CommentLike& like : store_->data().likes) {
    if (like.comment_id == comment_id) ++count;
  }
  return count;
}

bool CommentService::HasLiked(const Actor& actor, Id comment_id) const {
  if (actor.user_id == 0) return false;
  for (const CommentLike& like : store_->data().likes) {
    if (like.comment_id == comment_id && like.user_id == actor.user_id) {
      return true;
    }
  }
  return false;
}

Status CommentService::Rate(const Actor& actor, Id activity_id, int stars) {
  if (actor.role != Role::kParticipant) {
    return Status::Error("请以参与者身份登录后评分");
  }
  const Activity* activity = store_->FindActivity(activity_id);
  if (activity == nullptr) return Status::Error("活动不存在");
  if (activity->status != ActivityStatus::kEnded) {
    return Status::Error("活动结束后才能评分");
  }
  if (stars < 1 || stars > 5) return Status::Error("评分须为 1～5 星");
  for (Rating& rating : store_->data().ratings) {
    if (rating.activity_id == activity_id && rating.user_id == actor.user_id) {
      if (rating.stars == stars) return Status::Ok("评分未变化");
      rating.stars = stars;
      rating.created_at = clock_->Now();
      store_->MarkDirty();
      audit_->Record(actor, "评分", std::to_string(stars) + " 星",
                     activity_id, "修改评分");
      return Status::Ok("评分已更新");
    }
  }
  store_->data().ratings.push_back(
      {activity_id, actor.user_id, stars, clock_->Now()});
  store_->MarkDirty();
  audit_->Record(actor, "评分", std::to_string(stars) + " 星", activity_id,
                 "成功");
  return Status::Ok("感谢评分");
}

bool CommentService::Average(Id activity_id, double* average,
                             int* count) const {
  int total = 0;
  int n = 0;
  for (const Rating& rating : store_->data().ratings) {
    if (rating.activity_id == activity_id) {
      total += rating.stars;
      ++n;
    }
  }
  *count = n;
  if (n == 0) return false;
  *average = static_cast<double>(total) / n;
  return true;
}

int CommentService::MyRating(const Actor& actor, Id activity_id) const {
  for (const Rating& rating : store_->data().ratings) {
    if (rating.activity_id == activity_id && rating.user_id == actor.user_id) {
      return rating.stars;
    }
  }
  return 0;
}

std::vector<const Comment*> CommentService::TopLevel(Id activity_id) const {
  std::vector<const Comment*> result;
  for (const Comment& comment : store_->data().comments) {
    if (comment.activity_id == activity_id && comment.parent_id == 0) {
      result.push_back(&comment);
    }
  }
  std::sort(result.begin(), result.end(),
            [](const Comment* a, const Comment* b) { return a->id > b->id; });
  return result;
}

std::vector<const Comment*> CommentService::Replies(Id comment_id) const {
  std::vector<const Comment*> result;
  for (const Comment& comment : store_->data().comments) {
    if (comment.parent_id == comment_id) result.push_back(&comment);
  }
  return result;
}

int CommentService::CountOf(Id activity_id) const {
  int count = 0;
  for (const Comment& comment : store_->data().comments) {
    if (comment.activity_id == activity_id) ++count;
  }
  return count;
}

}  // namespace ticketing
