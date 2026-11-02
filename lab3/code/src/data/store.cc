// Copyright 2026 R
#include "data/store.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "common/file_util.h"

namespace ticketing {
namespace {

constexpr char kFileHeader[] = "TICKETING-DATA 1";

// 转义字段中的反斜杠、制表符和换行，保证一条记录占一行。
std::string Escape(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '\t': out += "\\t"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      default: out += c;
    }
  }
  return out;
}

std::string Unescape(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '\\' || i + 1 == text.size()) {
      out += text[i];
      continue;
    }
    const char next = text[++i];
    if (next == 't') {
      out += '\t';
    } else if (next == 'n') {
      out += '\n';
    } else if (next == 'r') {
      out += '\r';
    } else {
      out += next;
    }
  }
  return out;
}

// 组装一行记录：标签加若干个以制表符分隔的字段。
class LineWriter {
 public:
  explicit LineWriter(const char* tag) : text_(tag) {}
  LineWriter& Int(int64_t value) {
    text_ += '\t';
    text_ += std::to_string(value);
    return *this;
  }
  LineWriter& Bool(bool value) { return Int(value ? 1 : 0); }
  LineWriter& Str(const std::string& value) {
    text_ += '\t';
    text_ += Escape(value);
    return *this;
  }
  std::string Finish() const { return text_ + '\n'; }

 private:
  std::string text_;
};

// 拆分一行记录。读取越界或数字非法时把 ok() 置为 false。
class LineReader {
 public:
  explicit LineReader(const std::string& line) {
    size_t start = 0;
    while (true) {
      const size_t tab = line.find('\t', start);
      fields_.push_back(Unescape(line.substr(start, tab - start)));
      if (tab == std::string::npos) break;
      start = tab + 1;
    }
  }
  const std::string& tag() const { return fields_[0]; }
  bool ok() const { return ok_; }

  // 依次读取下一个字段。
  int64_t Int() {
    const std::string& text = Next();
    char* end = nullptr;
    const int64_t value = std::strtoll(text.c_str(), &end, 10);
    if (text.empty() || *end != '\0') ok_ = false;
    return value;
  }
  bool Bool() { return Int() != 0; }
  std::string Str() { return Next(); }

 private:
  const std::string& Next() {
    static const std::string kEmpty;
    if (index_ >= fields_.size()) {
      ok_ = false;
      return kEmpty;
    }
    return fields_[index_++];
  }

  std::vector<std::string> fields_;
  size_t index_ = 1;
  bool ok_ = true;
};

// 修改申请中的名额列表编码为 "类型编号:名额,类型编号:名额"。
std::string EncodeCapacities(const std::vector<CapacityChange>& changes) {
  std::string text;
  for (const CapacityChange& change : changes) {
    if (!text.empty()) text += ',';
    text += std::to_string(change.ticket_type_id) + ':' +
            std::to_string(change.capacity);
  }
  return text;
}

bool DecodeCapacities(const std::string& text,
                      std::vector<CapacityChange>* out) {
  size_t start = 0;
  while (start < text.size()) {
    size_t comma = text.find(',', start);
    if (comma == std::string::npos) comma = text.size();
    const std::string item = text.substr(start, comma - start);
    const size_t colon = item.find(':');
    if (colon == std::string::npos) return false;
    CapacityChange change;
    change.ticket_type_id = std::strtoll(item.c_str(), nullptr, 10);
    change.capacity = std::atoi(item.c_str() + colon + 1);
    out->push_back(change);
    start = comma + 1;
  }
  return true;
}

template <typename T, typename Pred>
T* FindIf(std::vector<T>* items, Pred pred) {
  auto it = std::find_if(items->begin(), items->end(), pred);
  return it == items->end() ? nullptr : &*it;
}

}  // namespace

User* Store::FindUser(Id id) {
  return FindIf(&data_.users, [id](const User& u) { return u.id == id; });
}

User* Store::FindUserByAccount(const std::string& account) {
  return FindIf(&data_.users,
                [&account](const User& u) { return u.account == account; });
}

Activity* Store::FindActivity(Id id) {
  return FindIf(&data_.activities,
                [id](const Activity& a) { return a.id == id; });
}

EventSession* Store::FindSession(Id id) {
  return FindIf(&data_.sessions,
                [id](const EventSession& s) { return s.id == id; });
}

TicketType* Store::FindTicketType(Id id) {
  return FindIf(&data_.ticket_types,
                [id](const TicketType& t) { return t.id == id; });
}

Registration* Store::FindRegistration(Id id) {
  return FindIf(&data_.registrations,
                [id](const Registration& r) { return r.id == id; });
}

Ticket* Store::FindTicketByCode(const std::string& code) {
  return FindIf(&data_.tickets,
                [&code](const Ticket& t) { return t.code == code; });
}

Ticket* Store::FindTicketOf(Id registration_id) {
  // 一份报名可能有过候补凭证和正式票，取最后发放的那张。
  Ticket* found = nullptr;
  for (Ticket& ticket : data_.tickets) {
    if (ticket.registration_id == registration_id) found = &ticket;
  }
  return found;
}

ChangeRequest* Store::FindChangeRequest(Id id) {
  return FindIf(&data_.change_requests,
                [id](const ChangeRequest& c) { return c.id == id; });
}

Comment* Store::FindComment(Id id) {
  return FindIf(&data_.comments, [id](const Comment& c) { return c.id == id; });
}

const Checkin* Store::FindCheckin(const std::string& ticket_code) const {
  for (const Checkin& checkin : data_.checkins) {
    if (checkin.ticket_code == ticket_code) return &checkin;
  }
  return nullptr;
}

std::vector<EventSession*> Store::SessionsOf(Id activity_id) {
  std::vector<EventSession*> result;
  for (EventSession& session : data_.sessions) {
    if (session.activity_id == activity_id) result.push_back(&session);
  }
  return result;
}

std::vector<TicketType*> Store::TicketTypesOf(Id session_id) {
  std::vector<TicketType*> result;
  for (TicketType& type : data_.ticket_types) {
    if (type.session_id == session_id) result.push_back(&type);
  }
  return result;
}

Status Store::Save(const std::string& path) {
  std::string out = std::string(kFileHeader) + '\n';
  out += LineWriter("META").Int(data_.next_id).Int(data_.next_queue)
             .Int(data_.clock_now).Finish();
  for (const User& u : data_.users) {
    out += LineWriter("USER").Int(u.id).Str(u.account).Str(u.display_name)
               .Str(u.password_hash).Int(static_cast<int>(u.role)).Finish();
  }
  for (const Activity& a : data_.activities) {
    out += LineWriter("ACT").Int(a.id).Int(a.organizer_id).Str(a.title)
               .Str(a.description).Str(a.category)
               .Int(static_cast<int>(a.status)).Str(a.review_reason)
               .Str(a.cancel_reason).Int(a.version).Int(a.created_at).Finish();
  }
  for (const EventSession& s : data_.sessions) {
    out += LineWriter("SES").Int(s.id).Int(s.activity_id).Str(s.location)
               .Int(s.start_at).Int(s.end_at).Int(s.reg_open_at)
               .Int(s.reg_close_at).Int(s.checkin_start_at).Bool(s.cancelled)
               .Bool(s.finished).Finish();
  }
  for (const TicketType& t : data_.ticket_types) {
    out += LineWriter("TT").Int(t.id).Int(t.session_id).Str(t.name)
               .Int(t.capacity).Bool(t.cancelled).Finish();
  }
  for (const Registration& r : data_.registrations) {
    out += LineWriter("REG").Int(r.id).Int(r.user_id).Int(r.session_id)
               .Int(r.ticket_type_id).Int(static_cast<int>(r.status))
               .Int(r.queue_seq).Int(r.created_at).Int(r.updated_at)
               .Bool(r.hidden_by_owner).Finish();
  }
  for (const Ticket& t : data_.tickets) {
    out += LineWriter("TK").Str(t.code).Int(t.registration_id)
               .Int(static_cast<int>(t.kind)).Bool(t.valid).Finish();
  }
  for (const CheckinAssignment& a : data_.assignments) {
    out += LineWriter("ASG").Int(a.checker_id).Int(a.session_id).Finish();
  }
  for (const Checkin& c : data_.checkins) {
    out += LineWriter("CHK").Str(c.ticket_code).Int(c.checker_id)
               .Int(c.checked_at).Finish();
  }
  for (const ChangeRequest& c : data_.change_requests) {
    out += LineWriter("CR").Int(c.id).Int(c.activity_id).Int(c.session_id)
               .Int(c.requester_id).Int(c.base_version)
               .Int(static_cast<int>(c.status)).Str(c.reason)
               .Str(c.new_location).Int(c.new_start_at).Int(c.new_end_at)
               .Int(c.new_reg_close_at).Int(c.new_checkin_start_at)
               .Int(c.created_at).Str(EncodeCapacities(c.capacities)).Finish();
  }
  for (const Message& m : data_.messages) {
    out += LineWriter("MSG").Int(m.id).Int(m.user_id).Int(m.registration_id)
               .Str(m.type).Str(m.content).Str(m.event_key).Bool(m.read)
               .Int(m.created_at).Finish();
  }
  for (const Comment& c : data_.comments) {
    out += LineWriter("CMT").Int(c.id).Int(c.activity_id).Int(c.parent_id)
               .Int(c.user_id).Str(c.guest_name).Str(c.content)
               .Int(static_cast<int>(c.badge)).Int(c.created_at).Finish();
  }
  for (const CommentLike& l : data_.likes) {
    out += LineWriter("LIKE").Int(l.comment_id).Int(l.user_id).Finish();
  }
  for (const Rating& r : data_.ratings) {
    out += LineWriter("RATE").Int(r.activity_id).Int(r.user_id).Int(r.stars)
               .Int(r.created_at).Finish();
  }
  for (const Favorite& f : data_.favorites) {
    out += LineWriter("FAV").Int(f.user_id).Int(f.activity_id)
               .Int(f.created_at).Finish();
  }
  for (const OperationLog& l : data_.logs) {
    out += LineWriter("LOG").Int(l.id).Int(l.actor_id).Str(l.actor_name)
               .Str(l.action).Str(l.target).Int(l.activity_id).Str(l.result)
               .Int(l.created_at).Finish();
  }
  if (!WriteWholeFile(path, out)) {
    return Status::Error("无法写入数据文件：" + path);
  }
  dirty_ = false;
  return Status::Ok();
}

Status Store::SaveIfDirty(const std::string& path) {
  if (!dirty_) return Status::Ok();
  return Save(path);
}

Status Store::Load(const std::string& path) {
  std::string content;
  if (!ReadWholeFile(path, &content)) {
    return Status::Error("数据文件不存在：" + path);
  }
  Data loaded;
  size_t start = 0;
  int line_number = 0;
  while (start < content.size()) {
    size_t end = content.find('\n', start);
    if (end == std::string::npos) end = content.size();
    std::string line = content.substr(start, end - start);
    start = end + 1;
    ++line_number;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line_number == 1) {
      if (line != kFileHeader) return Status::Error("数据文件格式不正确");
      continue;
    }
    if (line.empty()) continue;
    LineReader r(line);
    const std::string& tag = r.tag();
    if (tag == "META") {
      loaded.next_id = r.Int();
      loaded.next_queue = r.Int();
      loaded.clock_now = r.Int();
    } else if (tag == "USER") {
      User u;
      u.id = r.Int();
      u.account = r.Str();
      u.display_name = r.Str();
      u.password_hash = r.Str();
      u.role = static_cast<Role>(r.Int());
      loaded.users.push_back(u);
    } else if (tag == "ACT") {
      Activity a;
      a.id = r.Int();
      a.organizer_id = r.Int();
      a.title = r.Str();
      a.description = r.Str();
      a.category = r.Str();
      a.status = static_cast<ActivityStatus>(r.Int());
      a.review_reason = r.Str();
      a.cancel_reason = r.Str();
      a.version = static_cast<int>(r.Int());
      a.created_at = r.Int();
      loaded.activities.push_back(a);
    } else if (tag == "SES") {
      EventSession s;
      s.id = r.Int();
      s.activity_id = r.Int();
      s.location = r.Str();
      s.start_at = r.Int();
      s.end_at = r.Int();
      s.reg_open_at = r.Int();
      s.reg_close_at = r.Int();
      s.checkin_start_at = r.Int();
      s.cancelled = r.Bool();
      s.finished = r.Bool();
      loaded.sessions.push_back(s);
    } else if (tag == "TT") {
      TicketType t;
      t.id = r.Int();
      t.session_id = r.Int();
      t.name = r.Str();
      t.capacity = static_cast<int>(r.Int());
      t.cancelled = r.Bool();
      loaded.ticket_types.push_back(t);
    } else if (tag == "REG") {
      Registration g;
      g.id = r.Int();
      g.user_id = r.Int();
      g.session_id = r.Int();
      g.ticket_type_id = r.Int();
      g.status = static_cast<RegStatus>(r.Int());
      g.queue_seq = r.Int();
      g.created_at = r.Int();
      g.updated_at = r.Int();
      g.hidden_by_owner = r.Bool();
      loaded.registrations.push_back(g);
    } else if (tag == "TK") {
      Ticket t;
      t.code = r.Str();
      t.registration_id = r.Int();
      t.kind = static_cast<TicketKind>(r.Int());
      t.valid = r.Bool();
      loaded.tickets.push_back(t);
    } else if (tag == "ASG") {
      CheckinAssignment a;
      a.checker_id = r.Int();
      a.session_id = r.Int();
      loaded.assignments.push_back(a);
    } else if (tag == "CHK") {
      Checkin c;
      c.ticket_code = r.Str();
      c.checker_id = r.Int();
      c.checked_at = r.Int();
      loaded.checkins.push_back(c);
    } else if (tag == "CR") {
      ChangeRequest c;
      c.id = r.Int();
      c.activity_id = r.Int();
      c.session_id = r.Int();
      c.requester_id = r.Int();
      c.base_version = static_cast<int>(r.Int());
      c.status = static_cast<ChangeStatus>(r.Int());
      c.reason = r.Str();
      c.new_location = r.Str();
      c.new_start_at = r.Int();
      c.new_end_at = r.Int();
      c.new_reg_close_at = r.Int();
      c.new_checkin_start_at = r.Int();
      c.created_at = r.Int();
      if (!DecodeCapacities(r.Str(), &c.capacities)) {
        return Status::Error("数据文件第 " + std::to_string(line_number) +
                             " 行损坏");
      }
      loaded.change_requests.push_back(c);
    } else if (tag == "MSG") {
      Message m;
      m.id = r.Int();
      m.user_id = r.Int();
      m.registration_id = r.Int();
      m.type = r.Str();
      m.content = r.Str();
      m.event_key = r.Str();
      m.read = r.Bool();
      m.created_at = r.Int();
      loaded.messages.push_back(m);
    } else if (tag == "CMT") {
      Comment c;
      c.id = r.Int();
      c.activity_id = r.Int();
      c.parent_id = r.Int();
      c.user_id = r.Int();
      c.guest_name = r.Str();
      c.content = r.Str();
      c.badge = static_cast<Badge>(r.Int());
      c.created_at = r.Int();
      loaded.comments.push_back(c);
    } else if (tag == "LIKE") {
      CommentLike l;
      l.comment_id = r.Int();
      l.user_id = r.Int();
      loaded.likes.push_back(l);
    } else if (tag == "RATE") {
      Rating g;
      g.activity_id = r.Int();
      g.user_id = r.Int();
      g.stars = static_cast<int>(r.Int());
      g.created_at = r.Int();
      loaded.ratings.push_back(g);
    } else if (tag == "FAV") {
      Favorite f;
      f.user_id = r.Int();
      f.activity_id = r.Int();
      f.created_at = r.Int();
      loaded.favorites.push_back(f);
    } else if (tag == "LOG") {
      OperationLog l;
      l.id = r.Int();
      l.actor_id = r.Int();
      l.actor_name = r.Str();
      l.action = r.Str();
      l.target = r.Str();
      l.activity_id = r.Int();
      l.result = r.Str();
      l.created_at = r.Int();
      loaded.logs.push_back(l);
    } else {
      return Status::Error("数据文件第 " + std::to_string(line_number) +
                           " 行类型未知");
    }
    if (!r.ok()) {
      return Status::Error("数据文件第 " + std::to_string(line_number) +
                           " 行损坏");
    }
  }
  if (line_number == 0) return Status::Error("数据文件为空");
  data_ = loaded;
  dirty_ = false;
  return Status::Ok();
}

Transaction::Transaction(Store* store)
    : store_(store), snapshot_(store->data()) {}

Transaction::~Transaction() {
  if (!committed_) store_->data() = snapshot_;
}

void Transaction::Commit() {
  committed_ = true;
  store_->MarkDirty();
}

}  // namespace ticketing
