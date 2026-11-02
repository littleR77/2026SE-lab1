// Copyright 2026 R
// M1 账户与权限：注册、登录、密码哈希和对象级权限判断（REQ-001、REQ-024）。
#ifndef SERVICE_ACCOUNT_SERVICE_H_
#define SERVICE_ACCOUNT_SERVICE_H_

#include <string>
#include <vector>

#include "common/status.h"
#include "data/store.h"
#include "domain/model.h"
#include "service/audit_log.h"

namespace ticketing {

class AccountService {
 public:
  AccountService(Store* store, AuditLog* audit)
      : store_(store), audit_(audit) {}

  // 注册账号。自行注册的账号只能是参与者；平台创建其他角色时 role 可指定。
  // 前置条件：账号为 3～20 位字母、数字或下划线且未被占用；显示名非空；
  //           密码至少 6 位。
  // 结果：成功时 *out_id 为新用户编号，密码只保存哈希。
  Status Register(const std::string& account, const std::string& display_name,
                  const std::string& password, Role role, Id* out_id);

  // 登录。账号不存在或密码错误时返回同一条错误，不泄露账号是否存在。
  Status Login(const std::string& account, const std::string& password,
               Actor* out);

  const User* FindUser(Id id) const;
  // 显示名；游客或找不到时返回 "游客"。
  std::string NameOf(Id id) const;
  // 指定角色的全部用户，按编号升序。
  std::vector<const User*> ListByRole(Role role) const;

  // 计算加盐密码哈希（FNV-1a 迭代，演示用）。
  static std::string HashPassword(const std::string& account,
                                  const std::string& password);

 private:
  Store* store_;
  AuditLog* audit_;
};

// 权限判断：actor 是否为该活动的组织者（管理员不算）。
bool IsOrganizerOf(const Actor& actor, const Activity& activity);
// 权限判断：actor 能否管理该活动（本人组织者或管理员）。
bool CanManage(const Actor& actor, const Activity& activity);
// 权限判断：actor 能否查看该活动。草稿和待审核只有组织者本人和管理员可见。
bool CanView(const Actor& actor, const Activity& activity);

}  // namespace ticketing

#endif  // SERVICE_ACCOUNT_SERVICE_H_
