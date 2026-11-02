// Copyright 2026 R
#include "service/notifier.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ticketing {

bool Notifier::Notify(Id user_id, Id registration_id, const std::string& type,
                      const std::string& content,
                      const std::string& event_key) {
  for (const Message& message : store_->data().messages) {
    if (message.event_key == event_key) return false;
  }
  Message message;
  message.id = store_->NextId();
  message.user_id = user_id;
  message.registration_id = registration_id;
  message.type = type;
  message.content = content;
  message.event_key = event_key;
  message.created_at = clock_->Now();
  store_->data().messages.push_back(message);
  store_->MarkDirty();
  return true;
}

std::vector<const Message*> Notifier::ListMine(const Actor& actor) const {
  std::vector<const Message*> result;
  if (actor.user_id == 0) return result;
  for (const Message& message : store_->data().messages) {
    if (message.user_id == actor.user_id) result.push_back(&message);
  }
  std::sort(result.begin(), result.end(),
            [](const Message* a, const Message* b) { return a->id > b->id; });
  return result;
}

int Notifier::UnreadCount(const Actor& actor) const {
  int count = 0;
  for (const Message* message : ListMine(actor)) {
    if (!message->read) ++count;
  }
  return count;
}

void Notifier::MarkAllRead(const Actor& actor) {
  if (actor.user_id == 0) return;
  for (Message& message : store_->data().messages) {
    if (message.user_id == actor.user_id && !message.read) {
      message.read = true;
      store_->MarkDirty();
    }
  }
}

}  // namespace ticketing
