#pragma once

// core_engine/common/status.hpp
//
// Purpose:
// - Provide a stable, explicit error-reporting type for the engine.
// - Keep error handling predictable across subsystems and (eventually) binaries.
//
// Guideline:
// - Prefer returning Status (or a future StatusOr<T>) over throwing exceptions
//   inside core storage paths.

#include <string>
#include <string_view>
#include <utility>

namespace core_engine {

// Status is the primary error-reporting mechanism across the engine.
//
// Rationale:
// - Exceptions make composition hard across DLL boundaries and can complicate
//   predictable error paths in low-level systems.
// - std::expected is promising, but keeping an explicit Status type gives you
//   full control over ABI and representation.
//
// Convention:
// - Functions that can fail return Status (or StatusOr<T> in the future).
// - "ok" is represented by StatusCode::kOk.

enum class StatusCode {
  kOk = 0,

  // Generic errors
  kInvalidArgument,
  kNotFound,
  kAlreadyExists,
  kUnimplemented,
  kInternal,

  // I/O / storage
  kIoError,
  kCorruption,
};
//Returns the status of an operation specified by a StatusCode and an optional message.
class Status {
 public:
  static Status Ok() { return Status(StatusCode::kOk, ""); }

  static Status InvalidArgument(std::string message) {
    return Status(StatusCode::kInvalidArgument, std::move(message));
  }

  static Status NotFound(std::string message) {
    return Status(StatusCode::kNotFound, std::move(message));
  }

  static Status AlreadyExists(std::string message) {
    return Status(StatusCode::kAlreadyExists, std::move(message));
  }

  static Status Unimplemented(std::string message) {
    return Status(StatusCode::kUnimplemented, std::move(message));
  }

  static Status Internal(std::string message) {
    return Status(StatusCode::kInternal, std::move(message));
  }

  static Status IoError(std::string message) {
    return Status(StatusCode::kIoError, std::move(message));
  }

  static Status Corruption(std::string message) {
    return Status(StatusCode::kCorruption, std::move(message));
  }

  StatusCode code() const { return code_; }
  std::string_view message() const { return message_; }

  bool ok() const { return code_ == StatusCode::kOk; }

  std::string ToString() const;

 private:
  Status(StatusCode code, std::string message) : code_(code), message_(std::move(message)) {}

  StatusCode code_;
  std::string message_;
};

}  // namespace core_engine
