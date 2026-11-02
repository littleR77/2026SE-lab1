// Copyright 2026 R
// M8 消息通知：个人消息区，同一事件只通知一次（REQ-033、REQ-035）。
#ifndef SERVICE_NOTIFIER_H_
#define SERVICE_NOTIFIER_H_

#include <string>
#include <vector>

#include "common/time_util.h"
#include "data/store.h"
#include "domain/model.h"

namespace ticketing {

class Notifier {
 public:
  Notifier(Store* store, const Clock* clock) : store_(store), clock_(clock) {}

  // 给 user_id 发送消息。event_key 已存在时不再发送，返回 false。
  bool Notify(Id user_id, Id registration_id, const std::string& type,
              const std::string& content, const std::string& event_key);

  // 本人消息，按时间倒序。游客返回空列表。
  std::vector<const Message*> ListMine(const Actor& actor) const;
  int UnreadCount(const Actor& actor) const;
  // 把本人全部消息标为已读。
  void MarkAllRead(const Actor& actor);

 private:
  Store* store_;
  const Clock* clock_;
};

}  // namespace ticketing

#endif  // SERVICE_NOTIFIER_H_
