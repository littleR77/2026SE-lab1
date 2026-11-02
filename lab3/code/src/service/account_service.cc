// Copyright 2026 R
#include "service/account_service.h"

#include <cstdio>
#include <string>
#include <vector>

namespace ticketing {
namespace {

bool IsValidAccount(const std::string& account) {
  if (account.size() < 3 || account.size() > 20) return false;
  for (const char c : account) {
    const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    const bool digit = c >= '0' && c <= '9';
    if (!letter && !digit && c != '_') return false;
  }
  return true;
}

}  // namespace

std::string AccountService::HashPassword(const std::string& account,
                                         const std::string& password) {
  // FNV-1a 64 位，以账号作盐并迭代多轮。仅用于课程演示，
  // 真实系统应使用 bcrypt、Argon2 等专门的密码哈希算法。
  uint64_t hash = 1469598103934665603ULL;
  const std::string salted = account + ":" + password;
  for (int round = 0; round < 1000; ++round) {
    for (const char c : salted) {
      hash ^= static_cast<unsigned char>(c);
      hash *= 1099511628211ULL;
    }
  }
  char buffer[17];
  snprintf(buffer, sizeof(buffer), "%016llx",
           static_cast<unsigned long long>(hash));  // NOLINT(runtime/int)
  return buffer;
}

Status AccountService::Register(const std::string& account,
                                 const std::string& display_name,
                                 const std::string& password, Role role,
                                 Id* out_id) {
  if (!IsValidAccount(account)) {
    return Status::Error("账号须为 3～20 位字母、数字或下划线");
  }
  if (display_name.empty()) return Status::Error("请填写昵称");
  if (password.size() < 6) return Status::Error("密码至少 6 位");
  if (role == Role::kGuest) return Status::Error("不能注册游客账号");
  if (store_->FindUserByAccount(account) != nullptr) {
    return Status::Error("该账号已被注册");
  }
  User user;
  user.id = store_->NextId();
  user.account = account;
  user.display_name = display_name;
  user.password_hash = HashPassword(account, password);
  user.role = role;
  store_->data().users.push_back(user);
  store_->MarkDirty();
  if (out_id != nullptr) *out_id = user.id;
  audit_->Record(Actor{user.id, role}, "注册账号", "用户 " + account, 0,
                 "成功");
  return Status::Ok("注册成功");
}

Status AccountService::Login(const std::string& account,
                             const std::string& password, Actor* out) {
  const User* user = store_->FindUserByAccount(account);
  if (user == nullptr ||
      user->password_hash != HashPassword(account, password)) {
    return Status::Error("账号或密码错误");
  }
  out->user_id = user->id;
  out->role = user->role;
  return Status::Ok("登录成功");
}

const User* AccountService::FindUser(Id id) const {
  return store_->FindUser(id);
}

std::string AccountService::NameOf(Id id) const {
  const User* user = store_->FindUser(id);
  return user == nullptr ? "游客" : user->display_name;
}

std::vector<const User*> AccountService::ListByRole(Role role) const {
  std::vector<const User*> result;
  for (const User& user : store_->data().users) {
    if (user.role == role) result.push_back(&user);
  }
  return result;
}

bool IsOrganizerOf(const Actor& actor, const Activity& activity) {
  return actor.role == Role::kOrganizer &&
         actor.user_id == activity.organizer_id;
}

bool CanManage(const Actor& actor, const Activity& activity) {
  return actor.role == Role::kAdmin || IsOrganizerOf(actor, activity);
}

bool CanView(const Actor& actor, const Activity& activity) {
  if (activity.status == ActivityStatus::kDraft ||
      activity.status == ActivityStatus::kPending) {
    return CanManage(actor, activity);
  }
  return true;
}

}  // namespace ticketing
