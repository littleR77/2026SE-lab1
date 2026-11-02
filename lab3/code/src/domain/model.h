// Copyright 2026 R
// 领域模型：与实验二类图中的实体一一对应，只保存数据，不含业务规则。
#ifndef DOMAIN_MODEL_H_
#define DOMAIN_MODEL_H_

#include <cstdint>
#include <string>
#include <vector>

namespace ticketing {

// 所有实体的编号；0 表示“无”。
using Id = int64_t;
// 时间统一用“自 1970-01-01 00:00 起的分钟数”表示（本地时间，不含时区）。
using Time = int64_t;

// M1：用户角色。游客不保存为用户，只出现在 Actor 中。
enum class Role { kGuest, kParticipant, kOrganizer, kChecker, kAdmin };

// M2/M3：活动状态与修改申请状态。
enum class ActivityStatus { kDraft, kPending, kPublished, kEnded, kCancelled };
enum class ChangeStatus { kPending, kApproved, kRejected, kWithdrawn };

// M5：报名记录的 6 种状态（实验一核心状态表）。
enum class RegStatus {
  kWaiting,     // 候补中
  kConfirmed,   // 报名成功
  kCheckedIn,   // 已签到
  kCancelled,   // 已取消
  kAbsent,      // 缺席
  kExpired,     // 候补失效
};

// M6：票券种类。候补凭证不能用于入场。
enum class TicketKind { kFormal, kWaitlist };

// M9：评论作者身份标识，由系统判定，不能由作者自行指定。
enum class Badge { kGuest, kMember, kAttended, kOfficial };

struct User {
  Id id = 0;
  std::string account;       // 唯一
  std::string display_name;
  std::string password_hash;  // 加盐哈希，不保存明文
  Role role = Role::kParticipant;
};

// 当前操作者。游客的 user_id 为 0。
struct Actor {
  Id user_id = 0;
  Role role = Role::kGuest;
};

struct Activity {
  Id id = 0;
  Id organizer_id = 0;
  std::string title;
  std::string description;
  std::string category;
  ActivityStatus status = ActivityStatus::kDraft;
  std::string review_reason;  // 最近一次驳回原因
  std::string cancel_reason;
  int version = 1;            // 每次审核生效的变更都会加一
  Time created_at = 0;
};

struct EventSession {
  Id id = 0;
  Id activity_id = 0;
  std::string location;
  Time start_at = 0;
  Time end_at = 0;
  Time reg_open_at = 0;
  Time reg_close_at = 0;
  Time checkin_start_at = 0;  // 退票截止即检票开始（REQ-027）
  bool cancelled = false;
  bool finished = false;      // 已由定时调度做过缺席处理
};

struct TicketType {
  Id id = 0;
  Id session_id = 0;
  std::string name;  // 同一场次内唯一
  int capacity = 0;  // 正整数，发布后只增不减
  bool cancelled = false;
};

struct Registration {
  Id id = 0;
  Id user_id = 0;
  Id session_id = 0;
  Id ticket_type_id = 0;
  RegStatus status = RegStatus::kWaiting;
  int64_t queue_seq = 0;  // 候补排队序号，越小越靠前
  Time created_at = 0;
  Time updated_at = 0;
  bool hidden_by_owner = false;  // 个人列表清理，不删除历史
};

struct Ticket {
  std::string code;  // 唯一
  Id registration_id = 0;
  TicketKind kind = TicketKind::kFormal;
  bool valid = true;
};

struct CheckinAssignment {
  Id checker_id = 0;
  Id session_id = 0;
};

struct Checkin {
  std::string ticket_code;  // 每张正式票至多一条
  Id checker_id = 0;
  Time checked_at = 0;
};

struct CapacityChange {
  Id ticket_type_id = 0;
  int capacity = 0;
};

// 修改申请只针对一个场次：地点、时间和各票种名额（只增不减）。
struct ChangeRequest {
  Id id = 0;
  Id activity_id = 0;
  Id session_id = 0;
  Id requester_id = 0;
  int base_version = 0;
  ChangeStatus status = ChangeStatus::kPending;
  std::string reason;  // 驳回原因
  std::string new_location;
  Time new_start_at = 0;
  Time new_end_at = 0;
  Time new_reg_close_at = 0;
  Time new_checkin_start_at = 0;
  std::vector<CapacityChange> capacities;  // 各票种的新名额
  Time created_at = 0;
};

struct Message {
  Id id = 0;
  Id user_id = 0;
  Id registration_id = 0;
  std::string type;
  std::string content;
  std::string event_key;  // 唯一，防止同一事件重复通知
  bool read = false;
  Time created_at = 0;
};

struct Comment {
  Id id = 0;
  Id activity_id = 0;
  Id parent_id = 0;  // 0 表示顶层评论
  Id user_id = 0;    // 0 表示游客
  std::string guest_name;
  std::string content;
  Badge badge = Badge::kGuest;
  Time created_at = 0;
};

struct CommentLike {
  Id comment_id = 0;
  Id user_id = 0;
};

struct Rating {
  Id activity_id = 0;
  Id user_id = 0;
  int stars = 0;  // 1..5
  Time created_at = 0;
};

struct Favorite {
  Id user_id = 0;
  Id activity_id = 0;
  Time created_at = 0;
};

struct OperationLog {
  Id id = 0;
  Id actor_id = 0;  // 0 表示游客或系统
  std::string actor_name;
  std::string action;
  std::string target;
  Id activity_id = 0;
  std::string result;
  Time created_at = 0;
};

// 枚举的中文名称，用于界面显示和数据文件。
const char* RoleName(Role role);
const char* ActivityStatusName(ActivityStatus status);
const char* ChangeStatusName(ChangeStatus status);
const char* RegStatusName(RegStatus status);
const char* BadgeName(Badge badge);

// 报名是否处于“有效”状态（候补中、报名成功、已签到）。
bool IsActiveRegistration(RegStatus status);
// 报名是否占用名额（报名成功、已签到、缺席，见 REQ-018）。
bool OccupiesSeat(RegStatus status);

}  // namespace ticketing

#endif  // DOMAIN_MODEL_H_
