// Copyright 2026 R
#include "domain/model.h"

namespace ticketing {

const char* RoleName(Role role) {
  switch (role) {
    case Role::kGuest: return "游客";
    case Role::kParticipant: return "参与者";
    case Role::kOrganizer: return "组织者";
    case Role::kChecker: return "检票员";
    case Role::kAdmin: return "管理员";
  }
  return "";
}

const char* ActivityStatusName(ActivityStatus status) {
  switch (status) {
    case ActivityStatus::kDraft: return "草稿";
    case ActivityStatus::kPending: return "待审核";
    case ActivityStatus::kPublished: return "已发布";
    case ActivityStatus::kEnded: return "已结束";
    case ActivityStatus::kCancelled: return "已取消";
  }
  return "";
}

const char* ChangeStatusName(ChangeStatus status) {
  switch (status) {
    case ChangeStatus::kPending: return "待审核";
    case ChangeStatus::kApproved: return "已通过";
    case ChangeStatus::kRejected: return "已驳回";
    case ChangeStatus::kWithdrawn: return "已撤回";
  }
  return "";
}

const char* RegStatusName(RegStatus status) {
  switch (status) {
    case RegStatus::kWaiting: return "候补中";
    case RegStatus::kConfirmed: return "报名成功";
    case RegStatus::kCheckedIn: return "已签到";
    case RegStatus::kCancelled: return "已取消";
    case RegStatus::kAbsent: return "缺席";
    case RegStatus::kExpired: return "候补失效";
  }
  return "";
}

const char* BadgeName(Badge badge) {
  switch (badge) {
    case Badge::kGuest: return "游客";
    case Badge::kMember: return "";
    case Badge::kAttended: return "已出席";
    case Badge::kOfficial: return "活动官方";
  }
  return "";
}

bool IsActiveRegistration(RegStatus status) {
  return status == RegStatus::kWaiting || status == RegStatus::kConfirmed ||
         status == RegStatus::kCheckedIn;
}

bool OccupiesSeat(RegStatus status) {
  return status == RegStatus::kConfirmed || status == RegStatus::kCheckedIn ||
         status == RegStatus::kAbsent;
}

}  // namespace ticketing
