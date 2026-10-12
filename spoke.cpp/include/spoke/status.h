#pragma once
#include <string>

namespace spoke {

enum class StatusCode {
  Ok = 0,
  InvalidArgument = 1,
  IoError = 2,
  UnsupportedFormat = 3,
  OutOfMemory = 4,
  InternalError = 5,
};

struct Status {
  StatusCode code = StatusCode::Ok;
  std::string message;

  bool ok() const { return code == StatusCode::Ok; }
  static Status Ok() { return Status{StatusCode::Ok, ""}; }
  static Status Error(StatusCode c, std::string m) { return Status{c, std::move(m)}; }
  std::string toString() const;
};

template <typename T>
struct Result {
  bool ok = false;
  T value{};
  Status status{StatusCode::InternalError, "empty"};

  static Result<T> success(T v) {
    Result<T> r;
    r.ok = true;
    r.value = std::move(v);
    r.status = Status::Ok();
    return r;
  }
  static Result<T> failure(Status s) {
    Result<T> r;
    r.ok = false;
    r.status = std::move(s);
    return r;
  }
};

}  // namespace spoke
