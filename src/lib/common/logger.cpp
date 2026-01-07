#include <core_engine/common/logger.hpp>

#include <iostream>

namespace core_engine {

// Default: DEBUG in debug builds, INFO in release builds
static LogLevel g_min_log_level =
#ifdef NDEBUG
    LogLevel::kInfo;
#else
    LogLevel::kDebug;
#endif

void SetMinLogLevel(LogLevel level) {
  g_min_log_level = level;
}

LogLevel GetMinLogLevel() {
  return g_min_log_level;
}

void Log(LogLevel level, std::string_view message) {
  // Filter out logs below minimum level
  if (level < g_min_log_level) {
    return;
  }

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

} // namespace core_engine
