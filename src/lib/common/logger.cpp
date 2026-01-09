/* Simple logging utility for the database engine. It is used to log messages to the console. */
#include <core_engine/common/logger.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

core_engine::LogLevel DetermineDefaultLogLevel() {
#ifdef NDEBUG
  core_engine::LogLevel level = core_engine::LogLevel::kInfo;
#else
  core_engine::LogLevel level = core_engine::LogLevel::kDebug;
#endif

  const char* env = std::getenv("CORE_ENGINE_LOG_LEVEL");
  if (env == nullptr) {
    return level;
  }
  std::string value(env);

  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (value == "debug") {
    return core_engine::LogLevel::kDebug;
  }
  if (value == "info") {
    return core_engine::LogLevel::kInfo;
  }
  if (value == "warn" || value == "warning") {
    return core_engine::LogLevel::kWarn;
  }
  if (value == "error" || value == "err") {
    return core_engine::LogLevel::kError;
  }

  return level;
}

} // namespace

namespace core_engine {

// Default: DEBUG in debug builds, INFO in release builds (overridable via
// CORE_ENGINE_LOG_LEVEL environment variable)
static LogLevel g_min_log_level = DetermineDefaultLogLevel();

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
