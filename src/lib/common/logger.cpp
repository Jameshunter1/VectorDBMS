#include <core_engine/common/logger.hpp>

#include <iostream>

namespace core_engine {

void Log(LogLevel level, std::string_view message) {
  // Keep logging simple and dependency-free.
  // In a database engine, you usually want structured logging later.
  // This is the seam where that future work plugs in.

  const char* level_text = "INFO";
  switch (level) {
    case LogLevel::kDebug:
      level_text = "DEBUG";
      break;
    case LogLevel::kInfo:
      level_text = "INFO";
      break;
    case LogLevel::kWarn:
      level_text = "WARN";
      break;
    case LogLevel::kError:
      level_text = "ERROR";
      break;
  }

  std::cerr << "[" << level_text << "] " << message << "\n";
}

}  // namespace core_engine
