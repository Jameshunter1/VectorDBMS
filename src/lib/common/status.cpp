#include <core_engine/common/status.hpp>

#include <sstream>

namespace core_engine {

static const char* ToCodeString(StatusCode code) {
  switch (code) {
  case StatusCode::kOk:
    return "OK";
  case StatusCode::kInvalidArgument:
    return "INVALID_ARGUMENT";
  case StatusCode::kNotFound:
    return "NOT_FOUND";
  case StatusCode::kAlreadyExists:
    return "ALREADY_EXISTS";
  case StatusCode::kUnimplemented:
    return "UNIMPLEMENTED";
  case StatusCode::kInternal:
    return "INTERNAL";
  case StatusCode::kIoError:
    return "IO_ERROR";
  case StatusCode::kCorruption:
    return "CORRUPTION";
  }

  // Future-proofing: if new enum values are added, we still produce a string.
  return "UNKNOWN";
}

std::string Status::ToString() const {
  std::ostringstream out;
  out << ToCodeString(code_);
  if (!message_.empty()) {
    out << ": " << message_;
  }
  return out.str();
}

} // namespace core_engine
