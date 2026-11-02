// Copyright 2026 R
// 业务操作的返回结果：是否成功，以及给用户看的说明。
#ifndef COMMON_STATUS_H_
#define COMMON_STATUS_H_

#include <string>
#include <utility>

namespace ticketing {

class Status {
 public:
  static Status Ok(std::string message = "") {
    return Status(true, std::move(message));
  }
  static Status Error(std::string message) {
    return Status(false, std::move(message));
  }

  bool ok() const { return ok_; }
  const std::string& message() const { return message_; }

 private:
  Status(bool ok, std::string message)
      : ok_(ok), message_(std::move(message)) {}

  bool ok_;
  std::string message_;
};

}  // namespace ticketing

#endif  // COMMON_STATUS_H_
