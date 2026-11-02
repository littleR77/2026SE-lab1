// Copyright 2026 R
// S2 数据持久化：内存中的数据表、事务和文本文件存取。
// 业务模块只通过 Store 读写数据；唯一性等业务规则由各服务检查。
#ifndef DATA_STORE_H_
#define DATA_STORE_H_

#include <string>
#include <vector>

#include "common/status.h"
#include "domain/model.h"

namespace ticketing {

// 全部数据表。复制一份 Data 就得到一次完整快照。
struct Data {
  std::vector<User> users;
  std::vector<Activity> activities;
  std::vector<EventSession> sessions;
  std::vector<TicketType> ticket_types;
  std::vector<Registration> registrations;
  std::vector<Ticket> tickets;
  std::vector<CheckinAssignment> assignments;
  std::vector<Checkin> checkins;
  std::vector<ChangeRequest> change_requests;
  std::vector<Message> messages;
  std::vector<Comment> comments;
  std::vector<CommentLike> likes;
  std::vector<Rating> ratings;
  std::vector<Favorite> favorites;
  std::vector<OperationLog> logs;
  Id next_id = 1;          // 所有实体共用的自增编号
  int64_t next_queue = 1;  // 候补排队序号
  Time clock_now = 0;      // 保存模拟时钟，重新打开后时间不回退
};

class Store {
 public:
  Store() = default;
  Store(const Store&) = delete;
  Store& operator=(const Store&) = delete;

  Data& data() { return data_; }
  const Data& data() const { return data_; }

  // 分配新的实体编号或候补序号。
  Id NextId() { return data_.next_id++; }
  int64_t NextQueueSeq() { return data_.next_queue++; }

  // 按编号查找；找不到时返回 nullptr。
  User* FindUser(Id id);
  User* FindUserByAccount(const std::string& account);
  Activity* FindActivity(Id id);
  EventSession* FindSession(Id id);
  TicketType* FindTicketType(Id id);
  Registration* FindRegistration(Id id);
  Ticket* FindTicketByCode(const std::string& code);
  // 找到报名当前关联的票券（正式票或候补凭证）。
  Ticket* FindTicketOf(Id registration_id);
  ChangeRequest* FindChangeRequest(Id id);
  Comment* FindComment(Id id);
  const Checkin* FindCheckin(const std::string& ticket_code) const;

  // 归属关系查询，结果按编号升序。
  std::vector<EventSession*> SessionsOf(Id activity_id);
  std::vector<TicketType*> TicketTypesOf(Id session_id);

  // 标记数据已变化；SaveIfDirty 只在有变化时写文件。
  void MarkDirty() { dirty_ = true; }
  bool dirty() const { return dirty_; }

  // 把全部数据写入文本文件（先写临时文件再替换，避免写一半）。
  Status Save(const std::string& path);
  Status SaveIfDirty(const std::string& path);
  // 从文件读取全部数据；文件不存在时返回错误且不修改当前数据。
  Status Load(const std::string& path);

 private:
  Data data_;
  bool dirty_ = false;
};

// 事务：构造时保存快照，未调用 Commit 就析构时恢复快照，
// 保证一组关联写入“全部成功或全部不生效”（REQ-025）。
class Transaction {
 public:
  explicit Transaction(Store* store);
  ~Transaction();
  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;

  void Commit();

 private:
  Store* store_;
  Data snapshot_;
  bool committed_ = false;
};

}  // namespace ticketing

#endif  // DATA_STORE_H_
